#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include "terminal.h"
#include "pasta.h"
#include "stack.h"
#include "vars.h"
#include "base.h"
#include "int.h"
#include "file.h"

uint8_t * memory;

// Note: Pico SDK hangs on unaligned access; so be weary of casts to (uint16_t *)
// In theory 'mem' should be 16-bit aligned as long as 'memory' is.
// Of course, accessing data using 'mem' implicitly yields system alignment.
// which in practice will be little endian for most machines.
uint16_t * mem;


uint8_t prim_group_len;
#define MAX_PRIM_GROUPS 8
PrimGroupCb prim_groups[MAX_PRIM_GROUPS];

int read_char(FILE * file) {
    int result;
#ifdef ANSI_TERM
    result = isatty(fileno(file)) ? getch() : fgetc(file);
#else
    result = fgetc(file);
#ifdef PICO_SDK
    if (result == '\r') result = '\n'; // \r is EOL in serial terminal#endif
    if (isatty(fileno(file))) printf("%c", result);
#endif
#endif
  return result;
}

/** 
 * Find string in unique string list, or add it.
 * @return relative pointer to string
 */
uint16_t unique_string(char * string) {
    int i = mem[STRINGS];

    while (memory[i] != 0) {
        if (strcmp(string, (char *) (&memory[i+1])) == 0) return i+1;
        i += memory[i]; // follow chain
        if(i >= mem[END_OF+STRINGS]) { printf("String overflow!\n"); return 0; }
    }
    //printf("Adding new string '%s' at pos %d\n", string, i);

    int len = strlen(string) + 1; // include 0 at end
    memory[i] = len+1; // = skip count

    int result = i+1;
    strcpy((char *) (&memory[result]), string);
    memory[i+len+1] = 0; // mark end of chain

    // Update top register
    mem[TOP_OF+STRINGS] = i+len+1;

    return result;
}

#ifdef ANALYZE_VARS
Stack varstackmodel; // variable stack model
uint16_t UQSTR_ARGS;
uint16_t UQSTR_ENUM;
uint16_t UQSTR_BITFIELD;
uint16_t UQSTR_DEFINE;

uint16_t GLOBAL_GET;
uint16_t GLOBAL_GET_AT;
#endif

#ifdef LEXICAL_SCOPING

uint16_t UQSTR_CLOSURE; // Reference to the unique string used for a closure variable
uint16_t UQSTR_PARENT;  // Reference to the unique string used to point to a closure for the parent scope

// Define closure as an anonymous variable,
// indicating the position in the variable list at bind time
uint16_t bind(uint16_t value) {
    // printf("Binding %d\n", value);
    Variable * closure = add_var(UQSTR_CLOSURE, value);
    return ((uint8_t *) closure) - memory;
}
#endif

uint8_t add_primitive_group(PrimGroupCb cb) {
    uint8_t result = prim_group_len << 5;
    prim_groups[prim_group_len++] = cb;
    return result;
}

uint16_t add_primitive(uint16_t prim) {
    //printf("Add prim %d at %d\n", prim, code_end);
    memory[mem[TOP_OF+CODE]++] = 0; // Specify type 'primitive'
    memory[mem[TOP_OF+CODE]++] = prim;

#ifdef LEXICAL_SCOPING
    return bind(mem[TOP_OF+CODE]-2);
#else
    return mem[TOP_OF+CODE]-2;
#endif
}

static inline
bool is_bracket_char(int ch) {
    return ch == '(' || ch == ')' || ch == '{' || ch == '}';
}

static inline
bool is_whitespace_char(int ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}
int next_non_whitespace_char(FILE * infile, int until) {
    int ch = read_char(infile);
    while (ch != EOF && ch != until && is_whitespace_char(ch)) ch = read_char(infile);
    return ch;
}

#define CMD_EVAL 0b00
#define CMD_PUSH 0b01
#define CMD_REF  0b10
#define CMD_SKIP 0b11

#define BYTE_FOLLOWS 0b111101
#define WORD_FOLLOWS 0b111110
#define LONG_FOLLOWS 0b111111

//
// Running
//

Stack argstack;

void run_code(unsigned char * code, int length, bool toplevel, bool from_stdin);

char * cmds[] = { "eval", "push", "ref", "skip" };
void print_asm(unsigned char * code, int code_end) {
    for(int i=0; i<code_end; i++) {
        uint8_t cmd = code[i] & 0b11;
        int value = code[i] >> 2;
        char mode = 's';
        if(value == WORD_FOLLOWS) {
            value = code[i++];
            value |= code[i++] << 8;
            mode = 'w';
        }
        printf("%s %c %d\n", cmds[cmd], mode, value);
    }
}

void run_func(uint16_t func) {

#ifdef LEXICAL_SCOPING
    // 'func' actually points to an anonymous closure variable.
    uint16_t closure = func;
    func = ((Variable *) &memory[func])->value;
#endif

    // Avoid interpreting zeroes and other random (but hopefully low) functions
    if (func < mem[CODE] || func > mem[TOP_OF+CODE]) { printf("Invalid function reference.\n"); return; }
    uint8_t type = memory[func];
    uint16_t num_args = peek(&argstack);
    uint16_t result = 0;

    if (type == 0) { // signals primitive func
        uint8_t prim = memory[func+1];
        uint8_t group = (prim >> 5) & 0b111;
        // Final sanity check
        if (prim_groups[group] == NULL) { printf("Primitive %d.%d not found\n", group, prim & 0b11111); return; }
        // invoke primitive group callback
        result = prim_groups[group](prim & 0b11111);
    } else {
#ifdef LEXICAL_SCOPING
        // Start the function 'stack frame' with a variable referencing the closure,
        // serving as a marker to skip any intermediate variables during lookup.
        add_var(UQSTR_PARENT, closure);
#endif
        // 'type' is actually a skip_code instruction
        uint16_t length = memory[func+1];
        length |= memory[func+2] << 8;
        run_code(&memory[func+3], length, false, false);
        result = pop(&argstack);
        // assume for now all args are taken
    }

    drop(&argstack, num_args+1); // also drop 'num_args' value itself
    push(&argstack, result);
}

void run_code(unsigned char * code, int length, bool toplevel, bool from_stdin) {
#ifdef LEXICAL_SCOPING
    int saved_vars_top = mem[TOP_OF+VARS]-sizeof(Variable); // include parent scope ref in actual start of 'frame'
#else
    int saved_vars_top = mem[TOP_OF+VARS];
#endif
    int saved_argstack = argstack.length;
    int saved_datastack = mem[TOP_OF+DATA];

    for (int i=0; i<length; i++) {
        uint8_t cmd = code[i] & 0b11;
        uint16_t value = code[i] >> 2;
        if (value == WORD_FOLLOWS) {
            value = code[i+1];
            value |= (code[i+2] << 8);
            i+=2;
        }

        switch(cmd) {
            case CMD_EVAL:
                //printf("eval %d\n", value);
                if(value == 0) { // clear argstack at end of line
                    // Show expression result status if file is stdin,
                    // even if it is piped input
                    if(from_stdin) printf("[%d] Ok.\n", pop(&argstack));
                    argstack.length = saved_argstack;
                    mem[TOP_OF+DATA] = saved_datastack;
                } else {
                    push(&argstack, value); // push n args
                    run_func(item(&argstack, value+1));
                }
                break;
            case CMD_PUSH:
                //printf("push %d\n", value);
                push(&argstack, value);
                break;
            case CMD_REF:
                //printf("ref %d\n", value);
#ifdef ANALYZE_VARS
                // LSB: 1=local, 0=global
                if (value & 1) {
//                    printf("Varstack length %d\n", (mem[TOP_OF+VARS]-mem[VARS]) / sizeof(Variable));
                    Variable * var = (Variable *) &memory[mem[TOP_OF+VARS]-(value&~1)];
//                    printf("Graaf in var '%s'\n", (char *) &memory[var->name]);
                    push(&argstack, var->value);
                } else {
                    Variable * var = (Variable *) &memory[(mem[VARS]+value)];
                    push(&argstack, var->value);
                }
#else
                push(&argstack, safe_lookup_var(value)->value);
#endif
                break;
            case CMD_SKIP:
                //printf("skip %d\n", value);
#ifdef LEXICAL_SCOPING
                push(&argstack, bind((code-memory)+i-2));
#else
                push(&argstack, (code-memory)+i-2); // push start address of block
#endif
                i += value; // then skip
                break;
        }
    }

    // Reset scope, argstack
    if (!toplevel) {
        mem[TOP_OF+VARS] = saved_vars_top;
        int result = pop(&argstack);
//        if (argstack.length < saved_argstack) { printf ("TODO I suspect underflow when taking args\n"); }
        argstack.length = saved_argstack;
        push(&argstack, result);
        mem[TOP_OF+DATA] = saved_datastack;
    }
}

void add_command(unsigned char cmd, uint16_t value) {
    if (value > 59) {
        // Can't encode within 2^6 - 4
        // encode as 'word follows' (for now always 16-bit)
        memory[mem[TOP_OF+CODE]++] = cmd | (WORD_FOLLOWS << 2);
        // Little endian
        memory[mem[TOP_OF+CODE]++] = value & 0xFF;
        memory[mem[TOP_OF+CODE]++] = (value >> 8) & 0xFF;
    } else {
        memory[mem[TOP_OF+CODE]++] = cmd | (value << 2);
    }
    if(mem[TOP_OF+CODE] >= (uint16_t) (mem[END_OF+CODE]-1)) printf("Code overflow!\n");
}

int parse_int(FILE * infile, int ch, int * result, int radix) {
    // Allow letters for numbers 10 and higher, dependent on radix
    int normalized = ch;
    if (ch >= 'a' && ch <= 'z') normalized = '0'+10+(ch-'a');
    else if (ch >= 'A' && ch <= 'Z') normalized = '0'+10+(ch-'A');
    while(normalized >= '0' && normalized <= '0'+radix) {
        *result = (*result * radix) + (normalized-'0');
        ch = read_char(infile);
        normalized = ch;
        if (ch >= 'a' && ch <= 'z') normalized = '0'+10+(ch-'a');
        else if (ch >= 'A' && ch <= 'Z') normalized = '0'+10+(ch-'A');
        else if (ch < '0' || ch > '9') break;
    }
    return ch;
}

// Used for characters in strings and chars
char * escapes = "nrtfe";
char * ctrlchars = "\n\r\t\f\e";

// Call after having parsed '\'
int read_escaped_char(FILE * infile, int * result) {
    int ch = read_char(infile);
    if (ch == 'x') {
        ch = parse_int(infile, read_char(infile), result, 16);
    } else {
        for (int j=0; j<strlen(escapes); j++) {
            if (escapes[j] == ch) { ch = ctrlchars[j]; break; }
        }
        *result=ch;
        ch = read_char(infile);
    }

    return ch;
}

#ifdef ANALYZE_VARS
enum { VAR_LOCAL, VAR_INTERMEDIATE, VAR_GLOBAL };

void generate_var_ref(uint16_t name) {
//    printf("Start analysis of %s\n", (char*) &memory[name]);
    int var_type = VAR_LOCAL;
    int where = 0;

#ifdef LEXICAL_SCOPING
    int parents[16];
    int num_parents=0;
#endif

    int i=varstackmodel.length-1;
    while (i > 0 && varstackmodel.values[i] != name) {
        int16_t val = ((int16_t) (varstackmodel.values[i]));
        if (val == 0) {
            var_type=VAR_INTERMEDIATE;
#ifdef LEXICAL_SCOPING
            parents[num_parents++]=i*sizeof(Variable);
            if(num_parents > 16) printf("Too much nesting\n");
#endif
            i--;
        } else if(val < 0) {
            i += val;
            printf("Skipped %d to %s\n", val,  (char *) &memory[varstackmodel.values[i]]);
        } else i--;
    }

    if (i < 0 || varstackmodel.values[i] != name) { printf("Not found: %s\n", (char *) &memory[name]); return; } // var not found
    else where=i*sizeof(Variable); // 2*16 = 32 bit

    while (i > 0) {
        if(varstackmodel.values[i] == 0) break;
        i--;
    }
    if(varstackmodel.values[i] != 0) var_type=VAR_GLOBAL;

    if(var_type == VAR_INTERMEDIATE) {
//        printf("Constructing intermediate reference\n");
#ifdef LEXICAL_SCOPING
        // `get-at` (name may be improved upon) gets relative indices
        // Note that in benchmark.pasta it is not actually faster than
        // the get "name" from below (but the scenario isn't too
        // challenging on this point either).
        // Also note that we do not yet have a 'set-at'.
        add_command(CMD_PUSH, GLOBAL_GET_AT);
        int previous=(varstackmodel.length)*sizeof(Variable);
        for (int i=0;i<num_parents;i++) {
            add_command(CMD_PUSH, previous - parents[i]);
            previous = parents[i]-sizeof(Variable); // TODO not sure why this -sizeof... works out
        }
        add_command(CMD_PUSH, previous - where);
        add_command(CMD_EVAL, num_parents+2);
#else
        // Just 'get' intermediate vars by name
        add_command(CMD_PUSH, GLOBAL_GET);
        add_command(CMD_PUSH, name);
        add_command(CMD_EVAL, 2);
#endif
    } else {
        if (var_type == VAR_GLOBAL) {} // where -= mem[VARS]; // offset in 32-bit Variables == even
        else where = ((varstackmodel.length*sizeof(Variable))-where) | 1; // make odd to flag local
        add_command(CMD_REF, where);

//printf("Var %s found on %d, type %d varstack length %d\n", (char *) &memory[name], where, var_type, varstackmodel.length);
    }
}
#endif

// We compile toplevel code directly to memory as one big main function,
// which starts after the primitive references.
// Notice this presently still misses a proper skip_code header,
// like we have for blocks (but primitive references are differentiated with a zero).
char buffer[256];

int parse2(FILE * infile, int until, bool repl) {

    int num_args = 0;
#ifdef ANALYZE_VARS
    bool in_args = false;
    bool in_define = false;
    uint16_t to_define = 0;
#endif

    int ch = next_non_whitespace_char(infile, until);

    while (ch != until && ch != EOF) {
        if (ch == '#') {
            // skip comments
            do { ch = read_char(infile); } while (ch != '\n');
//            ch = next_non_whitespace_char(infile, until);
            if (ch == until) goto end;
        }

        if (ch >='0' && ch <= '9') {
            int radix = 10;
            if (ch == '0') {
                ch = read_char(infile);
                if(ch == 'b') { radix = 2; ch = read_char(infile); }
                else if(ch == 'x') { radix = 16; ch = read_char(infile); }
            }
            int result = 0;
            ch = parse_int(infile, ch, &result, radix);
            add_command(CMD_PUSH, result);
            num_args++;
            if (!is_whitespace_char(ch) || ch == until) continue; // so that superfluous 'ch' is processed in next round
        } else if (ch=='\'') {
            ch = read_char(infile);
            int result=ch;
            if(ch == '\\') ch = read_escaped_char(infile, &result);
            else ch=read_char(infile);
            if (ch != '\'') printf("Error: expecting char\n");
            add_command(CMD_PUSH, result);
            num_args++;
        } else if (ch=='\"') {
            int i = 0;
            ch = read_char(infile);
            while (ch != '\"') {
                int result = ch;
                if(ch == '\\') ch = read_escaped_char(infile, &result);
                else ch = read_char(infile);
                buffer[i++] = result;
            }
            buffer[i++] = 0;
            uint16_t uqstr = unique_string(buffer);
            add_command(CMD_PUSH, uqstr);
            num_args++;
#ifdef ANALYZE_VARS
            if (in_define && num_args == 2) { to_define = uqstr; }
            if (in_args) push(&varstackmodel, uqstr);
#endif
        } else if (ch == '{') {
#ifdef ANALYZE_VARS
            push(&varstackmodel, UQSTR_CLOSURE); // Parent scope will have a closure var
            int vml = varstackmodel.length;
            push(&varstackmodel, 0); // register scope boundary (== parent pointer)
#endif
            int skip_from = mem[TOP_OF+CODE];
            add_command(CMD_SKIP, 0xFFFF); // Force creation of word-sized value, to be overwritten later
            parse2(infile, '}', repl);
            int relative = mem[TOP_OF+CODE] - skip_from - 3;
            memory[skip_from+1] = relative & 0xFF;
            memory[skip_from+2] = (relative >> 8) & 0xFF;
            num_args++;
#ifdef ANALYZE_VARS
            varstackmodel.length = vml; // Done parsing block == done analyzing block
//            push(&varstackmodel, UQSTR_CLOSURE); // Do leave closure
#endif
        } else if (ch == ';') {
            add_command(CMD_EVAL, num_args);
            add_command(CMD_EVAL, 0); // clear argstack
            num_args = 0;
#ifdef ANALYZE_VARS
            if (in_define) {
                push(&varstackmodel, to_define);
                in_define=false;
                to_define=0;
            }
            in_args = false;
#endif
        } else if (ch == '(') {
            parse2(infile, ')', repl);
            num_args++;
        } else if (!is_whitespace_char(ch)) {
            // printf("Label\n");
            int i=0;
            while (ch != EOF && !is_whitespace_char(ch) && !is_bracket_char(ch) && ch != ';') { buffer[i++] = ch; ch = read_char(infile); }
            buffer[i++] = 0;
            //printf("Label is: '%s'\n", buffer);
            uint16_t uqstr = unique_string(buffer);
#ifdef ANALYZE_VARS
            generate_var_ref(uqstr);
            num_args++;
            if(num_args == 1 && (UQSTR_ARGS == uqstr || UQSTR_ENUM == uqstr || UQSTR_BITFIELD == uqstr)) { in_args = true; }
            if(num_args == 1 && UQSTR_DEFINE == uqstr) { in_define = true; }
#else
            add_command(CMD_REF, uqstr); // #else
            num_args++;
#endif
            if (!is_whitespace_char(ch) || ch == until) continue; // so that superfluous 'ch' is processed in next round
        }

        ch = next_non_whitespace_char(infile, until);
    }

end:

    if(num_args > 0) add_command(CMD_EVAL, num_args);

#ifdef ANALYZE_VARS
        if (in_define) {
            push(&varstackmodel, to_define);
            in_define=false;
            to_define=0;
        }
        in_args = false;
#endif

    return ch;
}

void parse(FILE * infile, bool repl) {
    bool interactive = isatty(fileno(infile));

    int pc = mem[TOP_OF+CODE];
    int ch = parse2(infile, '\n', repl);
    while (ch != EOF) {
        run_code(&memory[pc], mem[TOP_OF+CODE]-pc, true, infile == stdin);
        if (interactive && argstack.length > 0) printf("[%d] Ok.\n> ", peek(&argstack));
        pc = mem[TOP_OF+CODE];
        ch = parse2(infile, '\n', repl);
    }
    run_code(&memory[pc], mem[TOP_OF+CODE]-pc, true, infile == stdin);
    if (interactive && argstack.length > 0) printf("[%d] Ok.\n", peek(&argstack));
}
/*
Most of our current programs fit in 4k of regs (1/8), vars (3/8) and strings (4/8)
combined with 8k of code, but we can afford to double these sizes:

0000  8k Regs / sprites / vars / strings
2000 16k Code
6000  4k Default tiles
7000  3k Extra tiles (3/4)
7C00  1k Screen memory (or just start at 8000)
8000 32k User / sprite map memory

A slightly more code-oriented leayout could be realized for instance as follows:

0000 12k Regs / sprites / vars / strings
3000 20k Code
8000 4k Default tiles
9000 3k Extra tiles (3/4)
9C00 1k Screen memory (or just start at A000)
A000 24k User / sprite map memory
*/

void pasta_init() {
    memory = calloc(MAX_MEM / 2, 2); // ask for 16-bit aligned
    mem = (uint16_t *) memory;

    // Fill our basic memory layout registers
    mem[DATA] = 0x0020;
    mem[VARS] = 0x0400;
    mem[STRINGS] = 0x1000;
    mem[CODE] = 0x2000;
//    mem[END_OF+CODE] = 0xFFFF; // Use all mem unless a specific machine registers more memory blocks

    // Init segments and their tops
    memory[mem[VARS]] = 0;
    memory[mem[STRINGS]] = 0;

    // Setting top of regs / data == bottom of temporal expression value stack
    // to be just after reserved regs (TBD).
    mem[TOP_OF+DATA] = 0x00C0; //mem[DATA];
    mem[TOP_OF+VARS] = mem[VARS];
    mem[TOP_OF+STRINGS] = mem[STRINGS];
    mem[TOP_OF+CODE] = mem[CODE];

    argstack.length = 0;
    argstack.size = 256;
    argstack.values = malloc(256*sizeof(uint16_t));

    prim_group_len = 0;

#ifdef ANALYZE_VARS
    varstackmodel.length = 0;
    varstackmodel.size = 256;
    varstackmodel.values =malloc(256*sizeof(uint16_t));
#endif

#ifdef LEXICAL_SCOPING
    UQSTR_CLOSURE = unique_string("(closure)");
    UQSTR_PARENT = unique_string("(parent)");
#endif

    register_base_prims();
    register_int_prims();
    register_file_prims();

#ifdef ANALYZE_VARS
    UQSTR_ARGS = unique_string("args");
    UQSTR_ENUM = unique_string("enum");
    UQSTR_BITFIELD = unique_string("bitfield");
    UQSTR_DEFINE = unique_string("define");
    GLOBAL_GET = lookup_variable(unique_string("get"))->value;
    GLOBAL_GET_AT = lookup_variable(unique_string("get-at"))->value;

    // We can call init_varstackmodel here, but it can not reproduce
    // the model of (inactive) native functions.
    init_varstackmodel();
#endif

    FILE * infile;
    if ((infile = open_file("recipes/lib.pasta", "rb"))) {
        parse(infile, true);
        fclose(infile);
    }
}

bool do_repl;

// Either run argument file(s), REPL, or both.
void * mainloop(void * arg) {
    char ** args = (char **) arg;

    int i = 0;
    FILE * infile;

    do_repl = true;

#ifdef ANALYZE_VARS
    init_varstackmodel();
#endif

    // Save a reset state
    for (int i=0; i<8; i++) {
        mem[INITIAL+TOP_OF+i] = mem[TOP_OF+i];
    }

    if (args == NULL || args[i] == NULL || strcmp(args[i], "-") == 0) {
        // Load in a default rom.ram file
        // Base libs are already loaded at this point, if found;
        // In practice you would use either the libs, or the rom.ram file.
        if ((infile = open_file("rom.ram", "r"))) {
            //printf("Running from rom.ram\n");
            load(infile, 0xFF, 0xFF);
            fclose(infile);
#ifdef ANALYZE_VARS
            init_varstackmodel();
#endif
            refresh();
        }// else printf("No rom.ram found\n");
    }

    while (args != NULL && args[i] != NULL && strcmp(args[i], "-") != 0) {
        char * filename = args[i++];

        if ((infile = open_file(filename, "r"))) {
            if (strcmp(strrchr(filename, '.'), ".ram") == 0) {
                load(infile, 0xFF, 0xFF);
#ifdef ANALYZE_VARS
                init_varstackmodel();
#endif
                refresh();
            }
            else parse(infile, true);

            fclose(infile);
        } else {
            printf("Error opening file: %s\n", filename);
        }
    }

#ifdef ANSI_TERM
        if (isatty(fileno(stdin))) init_terminal();
#endif /* ANSI_TERM */

    // Try to autorun if no "-" arg detected
    if ((args == NULL || args[i] == NULL)) {
        //printf("Trying to autorun\n");
        Variable * var = quiet_lookup_variable(unique_string("run"));
        if (var != NULL) {
            if (args != NULL && args[i] == NULL) do_repl = false; // Signal not to default to repl after autorun
            //printf("Running run func\n");
            push(&argstack, var->value);
            push(&argstack, 1);
            run_func(var->value);
            argstack.length = 0;
        } else if (i != 0) do_repl = false;
    }

    // Allow interactive running even after file args using "-"
    if (do_repl) {
        printf("\nREADY.\n> ");


#ifdef PICO_SDK
        // On embedded situations, cycle over reading from stdin.
        // Ideally, something like this should allow us to disconnect using ^D
        // and then initiate a fresh connection later. In practice clients don't
        // seem compelled to hang up on just a ^D.
        while(1) {
#endif
            parse(stdin, true);
#ifdef PICO_SDK
            clearerr(stdin);
            reset_buffer();
        }
#endif
    }
#ifdef ANSI_TERM
        if (isatty(fileno(stdin))) reset_terminal();
#endif /* ANSI_TERM */

    return NULL;
}


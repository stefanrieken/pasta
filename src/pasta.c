#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include "terminal.h"
#include "pasta.h"
#include "stack.h"
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

#ifdef LEXICAL_SCOPING

uint16_t CLOSURE; // Reference to the unique string used for a closure variable
uint16_t PARENT;  // Reference to the unique string used to point to a closure for the parent scope

// Define closure as an anonymous variable,
// indicating the position in the variable list at bind time
uint16_t bind(uint16_t value) {
    // printf("Binding %d\n", value);
    Variable * closure = add_var(CLOSURE, value);
    return ((uint8_t *) closure) - memory;
}
#endif


void ls() {
    printf("Known variables: ");
    int i = mem[VARS];
    while(i < mem[TOP_OF+VARS]) {
        Variable * var = (Variable *) &memory[i];

#ifdef LEXICAL_SCOPING
        // Instead of printing "(closure)" a thousand times,
        // print a '*' to suggest that the preceding var may have been a function.
//        if (var->name == CLOSURE) { if (i != mem[VARS]) printf("*"); } else
        if (var->name != CLOSURE)
#endif
//        printf(" %s (%d)", str(var->name), var->value);
        printf("%s ", str(var->name));
        i += sizeof(Variable);
    }
    printf("\n");
}

void strings() {
    printf("Known strings:");
    int i = mem[STRINGS];
    while(memory[i] != 0) { printf(" %s", &memory[i+1]); i += memory[i]; }
    printf("\n");
}

Variable * add_var(uint16_t name, uint16_t value) {
    if (mem[TOP_OF+VARS] >= mem[END_OF+VARS]) { printf("Variable overflow! %d %d\n", mem[TOP_OF+VARS], mem[END_OF+VARS]); return 0; }
    Variable * var = (Variable *) &memory[mem[TOP_OF+VARS]];
    var->name = name;
    var->value = value;
    mem[TOP_OF+VARS] += sizeof(Variable);
    return var;
}

Variable * add_variable(char * name, uint16_t value) {
    return add_var(unique_string(name), value);
}

Variable * quiet_lookup_variable(uint16_t name) {
    for (int i=mem[TOP_OF+VARS]-sizeof(Variable);i>=mem[VARS];i-=sizeof(Variable)) {
        Variable * var = (Variable *) &memory[i];

#ifdef LEXICAL_SCOPING
        if(var->name == PARENT) {
            i = var->value;
            continue;
        }
#endif

        if(var->name == name) return var;
    }
    return NULL;
}

Variable * lookup_variable(uint16_t name) {
    Variable * var = quiet_lookup_variable(name);
    if (var != NULL) return var;

    // Not found
    if (name >= mem[STRINGS] && name < mem[END_OF+STRINGS]) {
        printf("Var not found: %s\n", (char *) (&memory[name])); // may reasonably assume name string ref is ok
    } else {
        printf("Var not found: %d\n", name);
    }
    return NULL;
}

Variable * safe_lookup_var(uint16_t name) {
    Variable * var = lookup_variable(name);
    if (var == NULL) return (Variable *) (&memory[mem[VARS]]); // return false
    else return var;
}

uint16_t set_var(uint16_t name, uint16_t value) {
    Variable * var = lookup_variable(name);
    if(var != NULL) {
        var->value = value;
        return 1;// memory-((uint8_t*) var);
    }
    return 0;
}

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
        add_var(PARENT, closure);
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
                push(&argstack, safe_lookup_var(value)->value);
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

char * escapes = "nrtfe";
char * ctrlchars = "\n\r\t\f\e";

// We compile toplevel code directly to memory as one big main function,
// which starts after the primitive references.
// Notice this presently still misses a proper skip_code header,
// like we have for blocks (but primitive references are differentiated with a zero).
void parse(FILE * infile, bool repl) {
    bool interactive = isatty(fileno(infile));

    Stack countstack = { 256, 0, malloc(256) };
    Stack skipstack = { 256, 0, malloc(256) };

    int pc = mem[TOP_OF+CODE];
    char buffer[256];

    if(interactive)
        printf("\nREADY.\n> ");

    // allow for top-level statements without brackets
    bopen(&countstack);
    int ch = next_non_whitespace_char(infile, '\n');

    while (ch != EOF) {
        if (ch == '#') {
            // skip comments
            do { ch = read_char(infile); } while (ch != '\n');
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
            count(&countstack, 1);
//            continue;
            if (!is_whitespace_char(ch) || (countstack.length == 1 && ch == '\n')) continue; // so that superfluous 'ch' is processed in next round
        } else if (ch=='\"') {
            int i = 0;
            ch = read_char(infile);
            while (ch != '\"') {
                if (ch == '\\') {
                    ch = read_char(infile);
                    if (ch == 'x') {
                        int result = 0;
                        ch = parse_int(infile, read_char(infile), &result, 16);
                        buffer[i++] = result;
                        continue;
                    }

	                for (int j=0; j<strlen(escapes); j++) {
                        if (escapes[j] == ch) { ch = ctrlchars[j]; break; }
                    }
                }
                buffer[i++] = ch;
                ch = read_char(infile);
            }
            buffer[i++] = 0;
            add_command(CMD_PUSH, unique_string(buffer));
            count(&countstack, 1);
        } else if (ch == '{') {
            bopen(&countstack);
            bopen(&skipstack);
            count(&skipstack, mem[TOP_OF+CODE]); // TODO make this always relative to last opening accolade, to make nesting work
            add_command(CMD_SKIP, 0xFFFF); // Force creation of word-sized value, to be overwritten later
        } else if (ch == '}' || ch == ';') {
            uint16_t num_args = bclose(&countstack);
            // Assuming we want 'separator-separated' commands inside {},
            // and assuming there is an expression without an end separator,
            // finalize that expression:
            if (!(ch == '}' && num_args == 0)) // This prevents a final ';' from clearing the return value by adding EVAL 0
                add_command(CMD_EVAL, num_args);
//            if (ch == '}') printf("Num args: %d\n", num_args);

            if (ch == '}') {
                int where = bclose(&skipstack);
                int relative = mem[TOP_OF+CODE] - where - 3;

                // Little endian
                memory[where+1] = relative & 0xFF;
                memory[where+2] = (relative >> 8) & 0xFF;
                count(&countstack, 1);
            } else {
                bopen(&countstack); // add new statement
            }
        } else if (ch == '(') {
            bopen(&countstack);
        } else if (ch == ')' || (countstack.length == 1 && ch == '\n')) {
            // printf("Num args at level %d: %d\n", countstack.length, peek(&countstack));
            if (ch == '\n' && peek(&countstack) == 0) { ch = next_non_whitespace_char(infile, '\n'); continue; }
            uint16_t num_args = bclose(&countstack);//-1;
            if (num_args > 0) {
                if (ch != '\n') { count(&countstack, 1); } // count return value from previous expr
                if (ch == '\n') { bopen(&countstack); } // for bracketless
                add_command(CMD_EVAL, num_args);
            } else if (ch == ')') printf("Bracket mismatch!\n");

            if(countstack.length == 1 && ch == '\n') {
                // Add command to clear argstack
                add_command(CMD_EVAL, 0);
                if (repl) {
                    run_code(&memory[pc], mem[TOP_OF+CODE]-pc, true, infile == stdin);
                    if (interactive && argstack.length > 0) printf("[%d] Ok.\n", peek(&argstack));
                    pc = mem[TOP_OF+CODE];
                }
                if(interactive) printf("> ");
          }

        } else if (!is_whitespace_char(ch)) {
            // printf("Label\n");
            int i=0;
            while (ch != EOF && !is_whitespace_char(ch) && !is_bracket_char(ch) && ch != ';') { buffer[i++] = ch; ch = read_char(infile); }
            buffer[i++] = 0;
            //printf("Label is: '%s'\n", buffer);
            add_command(CMD_REF, unique_string(buffer));
            count(&countstack, 1);
            if (!is_whitespace_char(ch) || (countstack.length == 1 && ch =='\n')) continue; // so that superfluous 'ch' is processed in next round
        }

        ch = next_non_whitespace_char(infile, '\n');
    }
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
    argstack.values = malloc(256);

    prim_group_len = 0;

#ifdef LEXICAL_SCOPING
    CLOSURE = unique_string("(closure)");
    PARENT = unique_string("(parent)");
#endif

    register_base_prims();
    register_int_prims();
    register_file_prims();

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

    if (args == NULL || args[i] == NULL || strcmp(args[i], "-") == 0) {
        // Load in a default rom.ram file
        // Base libs are already loaded at this point, if found;
        // In practice you would use either the libs, or the rom.ram file.
        if ((infile = open_file("rom.ram", "r"))) {
            //printf("Running from rom.ram\n");
            load(infile, 0xFF, 0xFF);
            fclose(infile);
            refresh();
        }// else printf("No rom.ram found\n");
    }

    while (args != NULL && args[i] != NULL && strcmp(args[i], "-") != 0) {
        char * filename = args[i++];

        if ((infile = open_file(filename, "r"))) {
            if (strcmp(strrchr(filename, '.'), ".ram") == 0) {
                load(infile, 0xFF, 0xFF);
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
        }
    }

    // Allow interactive running even after file args using "-"
    if (do_repl) {

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


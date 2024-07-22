#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include "pasta.h"
#include "stack.h"
#include "base.h"
#include "int.h"
#include "chef.h"

uint8_t * memory;

int code_end;
// We now compile code directly to memory as one big main function,
// which starts after the primitive references.
// Notice this presently still misses a proper skip_code header,
// like we have for blocks (and primitive references are typed with a zero).
int main_start;

int vars_end;

uint8_t prim_group_len;
#define MAX_PRIM_GROUPS 8
PrimGroupCb prim_groups[MAX_PRIM_GROUPS];

/** 
 * Find string in unique string list, or add it.
 * @return relative pointer to string
 */
uint16_t unique_string(char * string) {
    int i = STRING_START;
    while (memory[i] != 0) {
        if (strcmp(string, (char *) (&memory[i+1])) == 0) return i+1;
        i += memory[i]; // follow chain
        if((i-STRING_START) >= MAX_STRING) { printf("String overflow!\n"); return 0; }
    }
    // printf("Adding new string '%s' at pos %d\n", string, i);

    int len = strlen(string) + 1; // include 0 at end
    memory[i] = len+1; // = skip count

    int result = i+1;
    strcpy((char *) (&memory[result]), string);
    memory[i+len+1] = 0; // mark end of chain

    return result;
}

void ls() {
    printf("Known variables:");
    int i = VARS_START;
    while(i < vars_end) {
        Variable * var = (Variable *) &memory[i];
        printf(" %s (%d)", str(var->name), var->value);
        i += sizeof(Variable);
    }
    printf("\n");
}

void strings() {
    printf("Known strings:");
    int i = STRING_START;
    while(memory[i] != 0) { printf(" %s", &memory[i+1]); i += memory[i]; }
    printf("\n");
}

Variable * add_var(uint16_t name, uint16_t value) {
    if (vars_end >= VARS_START+MAX_VARS) { printf("Variable overflow!\n"); return 0; }
    Variable * var = (Variable *) &memory[vars_end];
    var->name = name;
    var->value = value;
    vars_end += sizeof(Variable);
    return var;
}

Variable * add_variable(char * name, uint16_t value) {
    return add_var(unique_string(name), value);
}

Variable * lookup_variable(uint16_t name) {
    for (int i=vars_end-sizeof(Variable);i>=VARS_START;i-=sizeof(Variable)) {
        Variable * var = (Variable *) &memory[i];
        if(var->name == name) return var;
    }
    // Not found
    if (name >= STRING_START && name < MAX_STRING) {
        printf("Var not found: %s\n", (char *) (&memory[name])); // may reasonably assume name string ref is ok
    } else {
        printf("Var not found: %d\n", name);
    }
    return NULL;
}

Variable * safe_lookup_var(uint16_t name) {
    Variable * var = lookup_variable(name);
    if (var == NULL) return (Variable *) (&memory[VARS_START]); // return false
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
    memory[code_end] = 0; // Specify type 'primitive'
    memory[code_end+1] = prim;
    uint16_t result = code_end;
    code_end += 2;
    return result;
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
    int ch = fgetc(infile);
    while (ch != EOF && ch != until && is_whitespace_char(ch)) ch = fgetc(infile);
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
            value = ((uint16_t *) (&code[i+1]))[0];
            i += 2;
            mode = 'w';
        }
        printf("%s %c %d\n", cmds[cmd], mode, value);
    }
}

void run_func(uint16_t func) {
    uint8_t type = memory[func];
    uint16_t num_args = peek(&argstack);
    uint16_t result = 0;

    if (type == 0) { // signals primitive func
        uint8_t prim = memory[func+1];
        uint8_t group = (prim >> 5) & 0b111;
        // invoke primitive group callback
        result = prim_groups[group](prim & 0b11111);
    } else {
        // 'type' is actually a skip_code instruction
        uint16_t length = ((uint16_t *) (&memory[func+1]))[0];
        run_code(&memory[func+3], length, false, false);
        result = pop(&argstack);
        // assume for now all args are taken
    }

    drop(&argstack, num_args+1); // also drop 'num_args' value itself
    push(&argstack, result);
}

void run_code(unsigned char * code, int length, bool toplevel, bool from_stdin) {
    int saved_vars_end = vars_end;
    int saved_argstack = argstack.length;

    for (int i=0; i<length; i++) {
        uint8_t cmd = code[i] & 0b11;
        int value = code[i] >> 2;
        if (value == WORD_FOLLOWS) {
            value = ((uint16_t *) (&code[i+1]))[0];
            i+=2;
        }

        switch(cmd) {
            case CMD_EVAL:
                //printf("eval %d\n", value);
                if(value == 0) { // clear argstack at end of line
                    // Show expression rsult status if file is stdin,
                    // even if it is piped input
                    if(from_stdin) printf("[%d] Ok.\n", pop(&argstack));
                    argstack.length = 0;
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
                push(&argstack, (code-memory)+i-2); // push start address of block
                i += value; // then skip
                break;
        }
    }

    // Reset scope, argstack
    if (!toplevel) {
        vars_end = saved_vars_end;
        int result = pop(&argstack);
        argstack.length = saved_argstack;
        push(&argstack, result);
    }
}

int add_command(unsigned char cmd, uint32_t value, unsigned char * code, int code_idx) {
    int result;
    if (value > 59) {
        // Can't encode within 2^6 - 4
        // encode as 'word follows' (for now always 16-bit)
        code[code_idx] = cmd | (WORD_FOLLOWS << 2);
        // Just piggybacking on system endianness here
        ((uint16_t *) (&code[code_idx+1]))[0] = value;
        result = code_idx+3;
    } else {
        code[code_idx] = cmd | (value << 2);
        result = code_idx+1;
    }
    if(result > MAX_CODE) printf("Code overflow!\n");
    return result;
}

int parse_int(FILE * infile, int ch, int * result, int radix) {
    // Allow letters for numbers 10 and higher, dependend on radix
    int normalized = ch;
    if (ch >= 'a' && ch <= 'z') normalized = '0'+10+(ch-'a');
    else if (ch >= 'A' && ch <= 'Z') normalized = '0'+10+(ch-'A');
    while(normalized >= '0' && normalized <= '0'+radix) {
        *result = (*result * radix) + (normalized-'0');
        ch = fgetc(infile);
        normalized = ch;
        if (ch >= 'a' && ch <= 'z') normalized = '0'+10+(ch-'a');
        else if (ch >= 'A' && ch <= 'Z') normalized = '0'+10+(ch-'A');
        else if (ch < '0' || ch > '9') break;
    }
    return ch;
}

char * escapes = "nrtfe";
char * ctrlchars = "\n\r\t\f\e";

void parse(FILE * infile, bool repl) {
    bool interactive = isatty(fileno(infile));

    Stack countstack = { 256, 0, malloc(256) };
    Stack skipstack = {256, 0, malloc(256) };

    uint8_t * code = memory; //&memory[CODE_START];
    main_start = code_end;
    int pc = code_end;
    char buffer[256];

    if(interactive)
        printf("\nREADY.\n> ");

    // allow for top-level statements without brackets
    bopen(&countstack);
    int ch = next_non_whitespace_char(infile, '\n');

    while (ch != EOF) {
        if (ch == '#') {
            // skip comments
            do { ch = fgetc(infile); } while (ch != '\n');
            //ch = next_non_whitespace_char(infile, '\n');
        }

        if (ch >='0' && ch <= '9') {
            int radix = 10;
            if (ch == '0') {
                ch = fgetc(infile);
                if(ch == 'b') { radix = 2; ch = fgetc(infile); }
                else if(ch == 'x') { radix = 16; ch = fgetc(infile); }
            }
            int result = 0;
            ch = parse_int(infile, ch, &result, radix);
            code_end = add_command(CMD_PUSH, result, code, code_end);
            count(&countstack, 1);
//            continue;
            if (!is_whitespace_char(ch) || (countstack.length == 1 && ch == '\n')) continue; // so that superfluous 'ch' is processed in next round
        } else if (ch=='\"') {
            int i = 0;
            ch = fgetc(infile);
            while (ch != '\"') {
                if (ch == '\\') {
                    ch = fgetc(infile);
                    if (ch == 'x') {
                        int result = 0;
                        ch = parse_int(infile, fgetc(infile), &result, 16);
                        buffer[i++] = result;
                        continue;
                    }

	                for (int j=0; j<strlen(escapes); j++) {
                        if (escapes[j] == ch) { ch = ctrlchars[j]; break; }
                    }
                }
                buffer[i++] = ch;
                ch = fgetc(infile);
            }
            buffer[i++] = 0;
            code_end = add_command(CMD_PUSH, unique_string(buffer), code, code_end);
            count(&countstack, 1);
        } else if (ch == '{') {
            bopen(&countstack);
            bopen(&skipstack);
            count(&skipstack, code_end); // TODO make this always relative to last opening accolade, to make nesting work
            code_end = add_command(CMD_SKIP, 0xFFFF, code, code_end); // Force creation of word-sized value, to be overwritten later
        } else if (ch == '}' || ch == ';') {
            uint16_t num_args = bclose(&countstack);
            // Assuming we want 'separator-separated' commands inside {},
            // and assuming there is an expression without an end separator,
            // finalize that expression:
            if (!(ch == '}' && num_args == 0)) // This prevents a final ';' from clearing the return value by adding EVAL 0
                code_end = add_command(CMD_EVAL, num_args, code, code_end);
//            if (ch == '}') printf("Num args: %d\n", num_args);

            if (ch == '}') {
                int where = bclose(&skipstack);
                ((uint16_t *) (&code[where+1]))[0] = code_end - where - 3; // 3 == size of skip instruction itself
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
                code_end = add_command(CMD_EVAL, num_args, code, code_end);
            } else if (ch == ')') printf("Bracket mismatch!\n");

            if(countstack.length == 1 && ch == '\n') {
                // Add command to clear argstack
                code_end = add_command(CMD_EVAL, 0, code, code_end);
                //print_asm(&code[main_start], code_end-main_start);
                if (repl) {
                    run_code(&code[pc], code_end-pc, true, infile == stdin);
                    //uint16_t result = peek(&argstack);
                    if (interactive && argstack.length > 0) printf("[%d] Ok.\n", peek(&argstack));
                    pc = code_end;
                }
                if(interactive) printf("> ");
          }

        } else if (!is_whitespace_char(ch)) {
            // printf("Label\n");
            int i=0;
            while (ch != EOF && !is_whitespace_char(ch) && !is_bracket_char(ch) && ch != ';') { buffer[i++] = ch; ch = fgetc(infile); }
            buffer[i++] = 0;
            //printf("Label is: '%s'\n", buffer);
            code_end = add_command(CMD_REF, unique_string(buffer), code, code_end);
            count(&countstack, 1);
            if (!is_whitespace_char(ch) || (countstack.length == 1 && ch == '\n')) continue; // so that superfluous 'ch' is processed in next round
        }

        ch = next_non_whitespace_char(infile, '\n');
    }
}


void pasta_init() {
    memory = malloc(MAX_MEM);
    memory[STRING_START] = 0;
    memory[VARS_START] = 0;
    vars_end = VARS_START;
    code_end = CODE_START;

    argstack.length = 0;
    argstack.size = 256;
    argstack.values = malloc(256);

    add_variable("false", 0);
    add_variable("true", 1);
    unique_string("+"); // test (de)duplication of strings

    register_base_prims();
    register_int_prims();
    register_chef_prims();
}

void * mainloop(void * arg) {
    char ** args = (char **) arg;

    int i = 0;
    while (args[i] != NULL && strcmp(args[i], "-") != 0) {
        FILE * infile = fopen(args[i++], "r");
        parse(infile, true);
        fclose(infile);
    }

    // Allow interactive running even after file args using "-"
    if (i == 0 || (args[i] != NULL && strcmp(args[i], "-") == 0)) {
        if (isatty(fileno(stdin))) {
            strings();
            ls();
        }
        parse(stdin, true);
    }

    return NULL;
}


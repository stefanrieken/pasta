#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "stack.h"

uint8_t * memory;

#define MAX_MEM (64 * 1024)

// The parse buffer is not presently used,
// but may be re-introduced when parsing prefixed code.
// (alternative: just reverse args to each expression as we parse it)
#define PARSE_BUFFER_START (256)

#define CODE_START (1 * 1024)
#define MAX_PARSE_BUFFER (CODE_START - PARSE_BUFFER_START)

#define STRING_START (4 * 1024)
#define MAX_CODE (STRING_START - CODE_START)

int code_end;
// We now compile code directly to memory as one big main function,
// which starts after the primitive references.
// Notice this presently still misses a proper skip_code header,
// like we have for blocks (and primitive references are typed with a zero).
int main_start;

#define VARS_START 8 * 1024
#define MAX_STRING (VARS_START - STRING_START)

#define MAX_VARS (MAX_MEM - VARS_START)

int vars_end;

// Primitive defs
#define PRIM_PLUS 40
#define PRIM_EQ 41
#define PRIM_HELLO 42
#define PRIM_PRINT 43
#define PRIM_PRINTS 44
#define PRIM_LS 45
#define PRIM_CODE 46
#define PRIM_IF 47
#define PRIM_DEFINE 48
#define PRIM_ARGS 49
#define PRIM_REMAINDER 50

typedef struct __attribute__((__packed__)) Variable {
    uint16_t name;
    uint16_t value; // TODO do we assume all types' values fit in 16 bits?
} Variable;

/** 
 * Find string in unique string list, or add it.
 * @return relative pointer to string
 */
uint16_t unique_string(char * string) {
    int i = 0; // or absolute: i=STRING_START
    while (memory[STRING_START+i] != 0) {
        if (strcmp(string, (char *) (&memory[STRING_START+i+1])) == 0) return i+1;
        i += memory[STRING_START+i]; // follow chain
        if(i >= MAX_STRING) { printf("String overflow!\n"); return 0; }
    }
    // printf("Adding new string '%s' at pos %d\n", string, i);

    int len = strlen(string) + 1; // include 0 at end
    memory[STRING_START+i] = len+1; // = skip count

    int result = i+1;
    strcpy((char *) (&memory[STRING_START+result]), string);
    memory[STRING_START+i+len+1] = 0; // mark end of chain

    return result;
}

#define str(v) ((char *) &memory[STRING_START+v])

uint16_t add_var(uint16_t name, uint16_t value) {
    if (vars_end >= VARS_START+MAX_VARS) { printf("Variable overflow!\n"); return 0; }
    Variable * var = (Variable *) &memory[vars_end];
    var->name = name;
    var->value = value;
    uint16_t result = vars_end;
    vars_end += sizeof(Variable);
    return result;
}

uint16_t add_variable(char * name, uint16_t value) {
    return add_var(unique_string(name), value);
}

Variable * lookup_variable(uint16_t name) {
    for (int i=VARS_START;i<vars_end;i+=sizeof(Variable)) {
        Variable * var = (Variable *) &memory[i];
        if(var->name == name) return var;
    }

    printf("Var not found: %d\n", name);
    return (Variable *) &memory[VARS_START]; // false
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
int next_non_whitespace_char(int until) {
    int ch = getchar();
    while (ch != until && is_whitespace_char(ch)) ch = getchar();
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

void run_code(unsigned char * code, int length, bool toplevel);

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

void run_func(Stack * argstack, uint16_t num_args) {
    uint16_t n = num_args;
    uint16_t func = item(argstack, n--);
    uint8_t type;

    start_run_func:

    type = memory[func];
    uint16_t temp;
    uint16_t result = 0;

    if (type == 0) { // signals primitive func
        uint8_t prim = memory[func+1];
        switch(prim) {
            case PRIM_PLUS:
                temp = item(argstack, n--);
                result = temp + item(argstack, n--);
                break;
            case PRIM_EQ:
                temp = item(argstack, n--);
                result = temp == item(argstack, n--);
               break;
            case PRIM_HELLO:
                printf("Hello from primitive #%d\n", prim);
                result = 0;
               break;
            case PRIM_PRINT:
                while(n != 0) { temp = item(argstack, n--); printf("%d ", temp); }
                printf("\n");
                result = temp;
                break;
            case PRIM_PRINTS:
                while(n != 0) { temp = item(argstack, n--); printf("%s", (char *) &memory[STRING_START + temp]); }
                result = temp;
                break;
            case PRIM_LS:
                ls();
                result = 0;
                break;
            case PRIM_CODE:
                print_asm(&memory[CODE_START+main_start], code_end-main_start);
                result = 0;
                break;
            case PRIM_IF:
                temp = item(argstack, n--);
                func = item(argstack, n--);
                drop(argstack, num_args);
                num_args = 0; n = 0;
                if(temp) {
                    goto start_run_func;
                }
                else result = 0;
                break;
            case PRIM_DEFINE:
                temp = item(argstack, n--);
                add_var(temp, item(argstack, n--)); // return the value
                break;
            case PRIM_ARGS:
                // As we call another function to pop our own args,
                // the callstack is a bit crowded, e.g.: "y" "x" 43 42
                // So define args first, then pop values
                temp = vars_end;
                for (int i=0; i<n;i++) add_var(item(argstack, n-i), 0);
                drop(argstack, num_args);
                for (int i=0; i<n;i++) ((Variable *) (&memory[temp]))[i].value = item(argstack, n-i);
                n -= num_args-1;
                result = 0;
                break;
            case PRIM_REMAINDER:
                printf("TODO work out arrays & use for remainder args\n");
                break;
            default:
                // It is very easy to come here by triggering the
                // evaluation of a random value as a function.
                printf("Invalid function reference: %d\n", func);
                result = 0;
                break;
        }
    } else {
        // 'type' is actually a skip_code instruction
        uint16_t length = ((uint16_t *) (&memory[func+1]))[0];
        run_code(&memory[func+3], length, false);
        result = pop(argstack);
        // assume for now all args are taken
        n = 0;
    }

    if (n != 0) printf("WARN: %d leftover args\n", n);
    drop(argstack, num_args);
    push(argstack, result);
}

void run_code(unsigned char * code, int length, bool toplevel) {
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
                    printf("[%d] Ok.\n", pop(&argstack));
                    argstack.length = 0;
                }
                else run_func(&argstack, value);
                break;
            case CMD_PUSH:
                //printf("push %d\n", value);
                push(&argstack, value);
                break;
            case CMD_REF:
                //printf("ref %d\n", value);
                push(&argstack, lookup_variable(value)->value);
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

char * escapes = "nrtfe";
char * ctrlchars = "\n\r\t\f\e";

void compile(bool repl) {
    Stack countstack = { 256, 0, malloc(256) };
    Stack skipstack = {256, 0, malloc(256) };

    uint8_t * code = &memory[CODE_START];
    main_start = code_end;
    int pc = code_end;
    char buffer[256];

    printf("\nREADY.\n> ");

    // allow for top-level statements without brackets
    bopen(&countstack);
    int ch = next_non_whitespace_char('\n');

    while (ch != EOF) {
        if (ch >='0' && ch <= '9') {
            // printf("Parsing int\n");
            int result = 0;
            do {
                result = (result * 10) + (ch-'0');
                ch = getchar();
            } while(ch >= '0' && ch <= '9');
            code_end = add_command(CMD_PUSH, result, code, code_end);
            count(&countstack, 1);
            if (!is_whitespace_char(ch) || (countstack.length == 1 && ch == '\n')) continue; // so that superfluous 'ch' is processed in next round
        } else if (ch=='\"') {
            int i = 0;
            do {
                ch = getchar();
	        if (ch == '\\') {
	          ch = getchar();
	          for (int i=0; i<strlen(escapes); i++)
		    if (escapes[i] == ch) { ch = ctrlchars[i]; break; }
	        }
                if (ch != '\"') buffer[i++] = ch;
            } while (ch != '\"');
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
            code_end = add_command(CMD_EVAL, num_args, code, code_end);

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
                    run_code(&code[pc], code_end-pc, true);
                    pc = code_end;
                }
                printf("> ");
          }

        } else if (!is_whitespace_char(ch)) {
            // printf("Label\n");
            int i=0;
            while (!is_whitespace_char(ch) && !is_bracket_char(ch) && ch != ';') { buffer[i++] = ch; ch = getchar(); }
            buffer[i++] = 0;
            //printf("Label is: '%s'\n", buffer);
            code_end = add_command(CMD_REF, unique_string(buffer), code, code_end);
            count(&countstack, 1);
            if (!is_whitespace_char(ch) || (countstack.length == 1 && ch == '\n')) continue; // so that superfluous 'ch' is processed in next round
        }

        ch = next_non_whitespace_char('\n');
    }
}

int main (int argc, char ** argv) {
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
    add_variable("+", add_primitive(PRIM_PLUS));
    add_variable("=", add_primitive(PRIM_EQ));
    add_variable("hi", add_primitive(PRIM_HELLO));
    add_variable("print", add_primitive(PRIM_PRINT));
    add_variable("prints", add_primitive(PRIM_PRINTS));
    add_variable("ls", add_primitive(PRIM_LS));
    add_variable("code", add_primitive(PRIM_CODE));
    add_variable("if", add_primitive(PRIM_IF));
    add_variable("define", add_primitive(PRIM_DEFINE));
    add_variable("args", add_primitive(PRIM_ARGS));
    add_variable("remainder", add_primitive(PRIM_REMAINDER));

    printf("Known strings:");
    int i = STRING_START;
    while(memory[i] != 0) { printf(" %s", &memory[i+1]); i += memory[i]; }
    printf("\n");

    ls();
    compile(true);
}

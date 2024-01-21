#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "stack.h"

uint8_t * memory;

#define MAX_MEM (64 * 1024)

#define PARSE_BUFFER_START (256)

#define CODE_START (1 * 1024)
#define MAX_PARSE_BUFFER (CODE_START - PARSE_BUFFER_START)

#define STRING_START (4 * 1024)
#define MAX_CODE (STRING_START - CODE_START)

int code_end;

#define VARS_START 8 * 1024
#define MAX_STRING (VARS_START - STRING_START)

#define MAX_VARS (MAX_MEM - VARS_START)

int vars_end;

// Primitive defs
#define PRIM_PLUS 40
#define PRIM_EQ 41
#define PRIM_HELLO 42
#define PRIM_PRINT 43
#define PRIM_LS 44
#define PRIM_IF 45
#define PRIM_DEFINE 46
#define PRIM_ARGS 47
#define PRIM_REMAINDER 48

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
    return 0;
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

void run_func(uint16_t func, uint16_t num_args) {
    uint8_t type = memory[func];
    uint16_t temp;

    if (type == 0) { // signals primitive func
        uint8_t prim = memory[func+1];
        switch(prim) {
            case PRIM_PLUS:
                push(&argstack, pop(&argstack) + pop(&argstack));
                break;
            case PRIM_EQ:
                push(&argstack, pop(&argstack) == pop(&argstack));
               break;
            case PRIM_HELLO:
                printf("Hello from primitive #%d\n", prim);
                push(&argstack, 0);
               break;
            case PRIM_PRINT:
                for(int i=0;i<num_args;i++) { temp = pop(&argstack); printf("%d ", temp); }
                printf("\n");
                push(&argstack, temp);
                break;
            case PRIM_LS:
                ls();
                push(&argstack, 0);
                break;
            case PRIM_IF:
                if(pop(&argstack)) { run_func(pop(&argstack), 0); }
                else push(&argstack, 0);
                break;
            case PRIM_DEFINE:
                // Do this in two code lines since C also doesn't make evaluation order promises
                temp = pop(&argstack);
                add_var(temp, peek(&argstack)); // return the value
                break;
            case PRIM_ARGS:
            printf("hiero %d\n", num_args);
                // As we call another function to pop our own args,
                // the callstack is a bit crowded, e.g.: "y" "x" 43 42
                // So define args first, then pop values
                temp = vars_end;
                for (int i=0; i<num_args;i++) add_var(pop(&argstack), 0);
                for (int i=0; i<num_args;i++) ((Variable *) (&memory[temp]))[i].value = pop(&argstack);
                push(&argstack, 0);
                break;
            case PRIM_REMAINDER:
                printf("TODO work out arrays & use for remainder args\n");
                break;
            default:
                // It is very easy to come here by triggering the
                // evaluation of a random value as a function.
                printf("Invalid function reference: %d\n", func);
                push(&argstack, 0);
                break;
        }
    } else {
        // 'type' is actually a skip_code instruction
        uint16_t length = ((uint16_t *) (&memory[func+1]))[0];
        run_code(&memory[func+3], length, false);
    }
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
                else run_func(pop(&argstack), value-1);
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
    if(result > MAX_PARSE_BUFFER) printf("Parse buffer overflow!\n");
    return result;
}

char * cmds[] = { "eval", "push", "ref", "skip" };
void print_asm(unsigned char * code, int code_idx) {
    for(int i=0; i<code_idx; i++) {
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

void compile_postfixed() {
    Stack countstack = { 256, 0, malloc(256) };
    Stack skipstack = {256, 0, malloc(256) };

    uint8_t * code = &memory[PARSE_BUFFER_START];
    int code_idx = 0;
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
            code_idx = add_command(CMD_PUSH, result, code, code_idx);
            count(&countstack, 1);
            if (!is_whitespace_char(ch) || (countstack.length == 1 && ch == '\n')) continue; // so that superfluous 'ch' is processed in next round
        } else if (ch=='\"') {
            int i = 0;
            do {
                ch = getchar();
                if (ch != '\"') buffer[i++] = ch;
            } while (ch != '\"');
            buffer[i++] = 0;
            code_idx = add_command(CMD_PUSH, unique_string(buffer), code, code_idx);
            count(&countstack, 1);
        } else if (ch == '{') {
            bopen(&countstack);
            bopen(&skipstack);
            count(&skipstack, code_idx); // TODO make this always relative to last opening accolade, to make nesting work
            code_idx = add_command(CMD_SKIP, 0xFFFF, code, code_idx); // Force creation of word-sized value, to be overwritten later
        } else if (ch == '}') {
            uint16_t num_args = bclose(&countstack);
            // Assuming we want 'separator-separated' commands inside {},
            // and assuming there is an expression without an end separator,
            // finalize that expression:
            code_idx = add_command(CMD_EVAL, num_args, code, code_idx);

            count(&countstack, 1);
            int where = bclose(&skipstack);
            ((uint16_t *) (&code[where+1]))[0] = code_idx - where - 3; // 3 == size of skip instruction itself
        } else if (ch == '(') {
            bopen(&countstack);
        } else if (ch == ')' || (countstack.length == 1 && ch == '\n')) {
            // printf("Num args at level %d: %d\n", countstack.length, peek(&countstack));
            uint16_t num_args = bclose(&countstack);//-1;
            if (num_args > 0) {
                if (ch != '\n') { count(&countstack, 1); } // count return value from previous expr
                if (ch == '\n') { bopen(&countstack); } // for bracketless
                code_idx = add_command(CMD_EVAL, num_args, code, code_idx);
            } else if (ch == ')') printf("Bracket mismatch!\n");

            if(countstack.length == 1 && ch == '\n') {
                // Add command to clear argstack
                code_idx = add_command(CMD_EVAL, 0, code, code_idx);
                //print_asm(code, code_idx);
                run_code(code, code_idx, true);
                code_idx = 0;
                argstack.length = 0;
                printf("> ");
          }

        } else {
            // printf("Label\n");
            int i=0;
            while (!is_whitespace_char(ch) && !is_bracket_char(ch)) { buffer[i++] = ch; ch = getchar(); }
            buffer[i++] = 0;
            // push(&argstack, lookup_variable(unique_string(buffer)));
            code_idx = add_command(CMD_REF, unique_string(buffer), code, code_idx);
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

    add_variable("true", 1);
    add_variable("false", 0);
    unique_string("+"); // test (de)duplication of strings
    add_variable("+", add_primitive(PRIM_PLUS));
    add_variable("=", add_primitive(PRIM_EQ));
    add_variable("hi", add_primitive(PRIM_HELLO));
    add_variable("print", add_primitive(PRIM_PRINT));
    add_variable("ls", add_primitive(PRIM_LS));
    add_variable("if", add_primitive(PRIM_IF));
    add_variable("define", add_primitive(PRIM_DEFINE));
    add_variable("args", add_primitive(PRIM_ARGS));
    add_variable("remainder", add_primitive(PRIM_REMAINDER));

    printf("Known strings:");
    int i = STRING_START;
    while(memory[i] != 0) { printf(" %s", &memory[i+1]); i += memory[i]; }
    printf("\n");

    ls();
//    run_postfixed();
    compile_postfixed();
}

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

char * memory;

#define MAX_MEM (64 * 1024)

#define CODE_START (1 * 1024)

#define STRING_START (4 * 1024)
#define MAX_CODE (STRING_START - CODE_START)

int code_end;

#define VARS_START 8 * 1024
#define MAX_STRING (VARS_START - STRING_START)

#define MAX_VARS (MAX_MEM - VARS_START)

int vars_end;

// Primitive defs
#define PRIM_HELLO 42
#define PRIM_PRINT 43

typedef struct Variable {
    uint16_t name;
    uint16_t value; // TODO assume all types' values fit in 16 bits?
} Variable;

uint16_t argstack[256];
int num_args = 0; // TODO this should be a bracket counting stack value

/** 
 * Find string in unique string list, or add it.
 * @return relative pointer to string
 */
uint16_t unique_string(char * string) {
    int i = 0; // or absolute: i=STRING_START
    while (memory[STRING_START+i] != 0) {
        if (strcmp(string, &memory[STRING_START+i+1]) == 0) return i+1;
        i += memory[STRING_START+i]; // follow chain
        if(i >= MAX_STRING) { printf("String overflow!\n"); return 0; }
    }
    printf("Adding new string '%s' at pos %d\n", string, i);

    int len = strlen(string) + 1; // include 0 at end
    memory[STRING_START+i] = len+1; // = skip count

    int result = i+1;
    strcpy(&memory[STRING_START+result], string);
    memory[STRING_START+i+len+1] = 0; // mark end of chain

    return result;
}

#define str(v) ((char *) &memory[STRING_START+v])

uint16_t add_variable(char * name, uint16_t value) {
    if (vars_end >= VARS_START+MAX_VARS) { printf("Variable overflow!\n"); return 0; }
    Variable * var = (Variable *) &memory[vars_end];
    var->name = unique_string(name);
    var->value = value;
    uint16_t result = vars_end;
    vars_end += sizeof(Variable);
    return result;
}

uint16_t lookup_variable(uint16_t name) {
    for (int i=VARS_START;i<vars_end;i+=sizeof(Variable)) {
        Variable * var = (Variable *) &memory[i];
        if(var->name == name) return var->value;
    }

    printf("Var not found: %d\n", name);
    return 0;
}

void run_func(uint16_t func) {
    uint8_t type = memory[func];

    if (type == 0) {
        uint8_t prim = memory[func+1];
        switch(prim) {
            case PRIM_PRINT:
              printf("My arg is: %d\n", argstack[num_args-1]); num_args--;
              break;
            default:
              printf("Hello from primitive #%d\n", prim);
              break;
        }
    } else {
        printf("TODO: interpret native code.\n");
    }
}

uint16_t add_primitive(uint16_t prim) {
    //printf("Add prim %d at %d\n", prim, code_end);
    memory[code_end] = 0; // Specify type 'primitive'
    memory[code_end+1] = prim;
    uint16_t result = code_end;
    code_end += 2;
    return result;
}

bool is_whitespace_char(int ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}
int next_non_whitespace_char() {
    int ch = getchar();
    while (is_whitespace_char(ch)) ch = getchar();
    return ch;
}

/**
 * Parse and run postfixed code on-the-fly.
 * This is not the end goal (which is to compile (prefixed) code),
 * but makes for a quick proof-of-concept.
 */
void run_postfixed() {
    char buffer[256];

    num_args = 0;

    int ch = next_non_whitespace_char();
    while (ch != EOF) {
        if (ch >='0' && ch <= '9') {
            // printf("Parsing int\n");
            int result = 0;
            do {
                result = (result * 10) + (ch-'0');
                ch = getchar();
            } while(ch >= '0' && ch <= '9');
            argstack[num_args++] = result;
        } else if (ch=='\"') {
            printf("Todo parse string\n");
        } else if (ch == '(') {
            num_args = 0;
        } else if (ch == ')') {
            uint16_t func = argstack[num_args-1]; num_args--;
            printf("Running func %d with %d args\n", func, num_args);
            run_func(func);
        } else {
            // printf("Label\n");
            int i=0;
            while (!is_whitespace_char(ch) && ch != '(' && ch != ')') { buffer[i++] = ch; ch = getchar(); }
            buffer[i++] = 0;
            argstack[num_args++] = lookup_variable(unique_string(buffer));
            if (!is_whitespace_char(ch)) continue; // so that superfluous 'ch' is processed in next round
        }

        ch = next_non_whitespace_char();
    }
}

int main (int argc, char ** argv) {
    memory = malloc(MAX_MEM);
    memory[STRING_START] = 0;
    memory[VARS_START] = 0;
    vars_end = VARS_START;
    code_end = CODE_START;

    // test program
    unique_string("+");
    unique_string("define");
    unique_string("+");
    add_variable("yay", 42);
    add_variable("hi", add_primitive(PRIM_HELLO));
    add_variable("print", add_primitive(PRIM_PRINT));

    printf("Known strings:\n");
    int i = STRING_START;
    while(memory[i] != 0) { printf("  %s\n", &memory[i+1]); i += memory[i]; }
    printf("Known variables:\n");
    i = VARS_START;
    while(i < vars_end) {
        Variable * var = (Variable *) &memory[i];
        printf("  %s: %d\n", str(var->name), var->value);
        i += sizeof(Variable);
    }

    run_postfixed();
}
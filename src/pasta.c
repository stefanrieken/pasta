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
uint16_t UQSTR_BIND;

uint16_t GLOBAL_GET;
uint16_t GLOBAL_GET_AT;
#endif

#ifdef LEXICAL_SCOPING

uint16_t UQSTR_CLOSURE; // Reference to the unique string used for a closure variable
uint16_t UQSTR_PARENT;  // Reference to the unique string used to point to a closure for the parent scope

// Define closure as an anonymous variable,
// indicating the position in the variable list at bind time
uint16_t bind(uint16_t value) {
    //printf("Binding %d\n", value);
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

// Bind primitives regardless of AUTO_BIND
#ifdef LEXICAL_SCOPING
    return bind(mem[TOP_OF+CODE]-2);
#else
    return mem[TOP_OF+CODE]-2;
#endif
}

//
// Running
//

Stack argstack;

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

void parse_and_run(FILE * infile, bool repl) {
    bool interactive = isatty(fileno(infile));

    int pc = mem[TOP_OF+CODE];
    int ch = parse(infile, '\n', repl);
    while (ch != EOF) {
        run_code(&memory[pc], mem[TOP_OF+CODE]-pc, true, infile == stdin);
        if (interactive && argstack.length > 0) printf("[%d] Ok.\n> ", peek(&argstack));
        pc = mem[TOP_OF+CODE];
        ch = parse(infile, '\n', repl);
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
    UQSTR_BIND = unique_string("bind");
    GLOBAL_GET = lookup_variable(unique_string("get"))->value;
    GLOBAL_GET_AT = lookup_variable(unique_string("get-at"))->value;

    // We can call init_varstackmodel here, but it can not reproduce
    // the model of (inactive) native functions.
    init_varstackmodel();
#endif

    FILE * infile;
    if ((infile = open_file("recipes/lib.pasta", "rb"))) {
        parse_and_run(infile, true);
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
            else parse_and_run(infile, true);

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
            run_func(var->value, true);
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
            parse_and_run(stdin, true);
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


#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "stack.h"

void run_func(uint16_t func);

typedef struct __attribute__((__packed__)) Variable {
    uint16_t name;
    uint16_t value; // TODO do we assume all types' values fit in 16 bits?
} Variable;

Variable * add_variable(char * name, uint16_t value);
Variable * add_var(uint16_t name, uint16_t value);
uint16_t set_var(uint16_t name, uint16_t value);
Variable * lookup_variable(uint16_t name);

void parse(FILE * infile, bool repl);

void print_asm(unsigned char * code, int code_end);
void ls();
void strings();

extern Stack argstack;

#define str(v) ((char *) &memory[v])

//
// Primitives
//
typedef uint16_t (*PrimGroupCb)(uint8_t prim);

uint8_t add_primitive_group(PrimGroupCb cb);
uint16_t add_primitive(uint16_t prim);

//
// memory layout
//
extern uint8_t * memory;

#define MAX_MEM (64 * 1024)

#define CODE_START 0x0400

#define STRING_START 0x2400
#define MAX_CODE (STRING_START - CODE_START)
extern int code_end;
extern int main_start;

#define VARS_START 0x3000
#define MAX_STRING (VARS_START - STRING_START)

#define MAX_VARS (MAX_MEM - VARS_START)
extern int vars_end;

//
// Callbacks
//
void pasta_init();
void * mainloop(void * arg);

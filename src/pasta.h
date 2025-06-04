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
Variable * quiet_lookup_variable(uint16_t name);
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

// The array containing all of the system's memory
extern uint8_t * memory;
// A 16-bit view on the same array, mainly used to access the memory layout registers

// The name of the (subset of) memory layout registers administered by Pasta
extern uint16_t * mem;
enum {
    DATA,
    VARS,
    STRINGS,
    CODE
    // Memory layout registry is (for now) 8 words wide, so 4 more values may be added by specific machines
};
// So we can write e.g. mem[END_OF+VARS] to reinterpret the next registry item as an end indicator
#define END_OF 1
// so we can write e.g. mem[TOP_OF+VARS] to access the top-of-memory registers
#define TOP_OF 8

#define MAX_MEM (64 * 1024)


// Wrapper so that implementations can select between using
// fopen (where files are separate) or fmemopen (where files are linked in).
FILE * open_file (const char * filename, const char * mode);

//
// Init / run function
//
void pasta_init();
void * mainloop(void * arg);

// Signals that mainloop intends to fall back to repl after run loop.
// Can be used to fine-tune such behaviour as halting run loop on stin.
extern bool do_repl;

//
// Callbacks for which Pasta expects an implementation
//
extern void refresh(); // Trigger state refreshes, think redrawing a screen after loading a binary

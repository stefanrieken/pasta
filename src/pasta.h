#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "stack.h"

extern uint16_t UQSTR_CLOSURE; // Reference to the unique string used for a closure variable
extern uint16_t UQSTR_PARENT;  // Reference to the unique string used to point to a closure for the parent scope

uint16_t bind(uint16_t value);

void run_func(uint16_t func, bool is_bound);

void parse_and_run(FILE * infile, bool repl);

void print_asm(unsigned char * code, int code_end);

extern Stack argstack;

#define str(v) ((char *) &memory[v])

/** 
 * Find string in unique string list, or add it.
 * @return relative pointer to string
 */
uint16_t unique_string(char * string);

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
// and mem[INITIAL+TOP_OF+VARS] for a reset state
#define INITIAL 8

#define MAX_MEM (64 * 1024)

#ifdef ANALYZE_VARS
extern Stack varstackmodel; // variable stack model
extern uint16_t UQSTR_ARGS;
extern uint16_t UQSTR_ENUM;
extern uint16_t UQSTR_BITFIELD;
extern uint16_t UQSTR_DEFINE;
extern uint16_t UQSTR_BIND;

extern uint16_t GLOBAL_GET;
extern uint16_t GLOBAL_GET_AT;
#endif

#define CMD_EVAL 0b00
#define CMD_PUSH 0b01
#define CMD_REF  0b10
#define CMD_SKIP 0b11

#define BYTE_FOLLOWS 0b111101
#define WORD_FOLLOWS 0b111110
#define LONG_FOLLOWS 0b111111

void run_code(unsigned char * code, int length, bool toplevel, bool from_stdin);
int parse(FILE * infile, int until, bool repl);

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

// For lazy evaluation we probably need a whole new eval loop.
// Any eager callbacks may survive the transition just by redefining this function:
#define next_arg() (item(&argstack, n--))
#define skip_arg() if (n > 1) { n--; }

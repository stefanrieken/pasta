void run_func(uint16_t func);

uint16_t add_variable(char * name, uint16_t value);
uint16_t add_var(uint16_t name, uint16_t value);

void print_asm(unsigned char * code, int code_end);
void ls();
void strings();

typedef struct __attribute__((__packed__)) Variable {
    uint16_t name;
    uint16_t value; // TODO do we assume all types' values fit in 16 bits?
} Variable;

extern Stack argstack;

#define str(v) ((char *) &memory[STRING_START+v])

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

#define CODE_START (1 * 1024)

#define STRING_START (4 * 1024)
#define MAX_CODE (STRING_START - CODE_START)
extern int code_end;
extern int main_start;

#define VARS_START 8 * 1024
#define MAX_STRING (VARS_START - STRING_START)

#define MAX_VARS (MAX_MEM - VARS_START)
extern int vars_end;

#include <stdint.h>
#include <stdio.h>

#include "pasta.h"

// Base primitive defs
enum {
    PRIM_RETURN,
    PRIM_PRINT,
    PRIM_$,
    PRIM_C,
    PRIM_LS,
    PRIM_STRINGS,
    PRIM_CODE,
    PRIM_IF,
    PRIM_LOOP,
    PRIM_DEFINE,
    PRIM_SET,
    PRIM_ADD,
    PRIM_SUB,
    PRIM_ARGS,
    PRIM_REMAINDER,
    PRIM_ENUM,
    PRIM_BITFIELD,
    PRIM_NOT,
    PRIM_NOTB,
    PRIM_GET_BYTE,
    PRIM_GET_WORD,
    PRIM_SET_BYTE,
    PRIM_SET_WORD,
    PRIM_SET_ALL_BYTE,
    PRIM_LSB,
    PRIM_MSB,
    PRIM_READ,
    PRIM_GETC
};

// Convert an integer value to a base-n string representation 
// (for printing), stored on the temporal data stack.
uint16_t val_to_base(uint16_t val, uint16_t base, uint16_t positions) {
    // Return the position of the new value
    uint16_t result = mem[TOP_OF+DATA];

    if (positions == 0) {
        // Decide positions based on val
        uint16_t val2 = val;
        while (val2 != 0) { val2 = val2 / base; positions++; }
        if (positions == 0) positions = 1;
    }

    int divider = 1;
    while (positions > 1) { divider *= base; positions--; }

    while (divider != 0) {
        uint8_t digit = (val / divider) % base;
        uint8_t start = digit < 10 ? '0' : 'a' - 10;
        memory[mem[TOP_OF+DATA]++] = start + ((val / divider) % base);
        divider /= base;
    }

    memory[mem[TOP_OF+DATA]++] = '\0';
    return result;
}

FILE * readfile = NULL;
uint16_t base_prim_group_cb(uint8_t prim) {
    uint16_t n = peek(&argstack);
    uint16_t temp, func, func2;
    uint16_t result = 0;
    Variable * var;

    switch(prim) {
        case PRIM_RETURN:
            // May also call this 'pass', 'quote', 'ref', etc.
            // The point is to turn a lone value into an expression
            // instead of accidentally evaluating it as a function
            result = item(&argstack, n--);
            break;
        case PRIM_PRINT:
            while(n != 1) { temp = item(&argstack, n--); printf("%s", (char *) &memory[temp]); }
            result = temp;
            break;
        case PRIM_$:
            temp = item(&argstack, n--);
            uint16_t base = n > 1 ? item(&argstack, n--) : 10;
            uint16_t positions = n > 1 ? item(&argstack, n--) : 0;
            result = val_to_base(temp, base, positions);
            break;
        case PRIM_C:
            result = mem[TOP_OF+DATA];
            memory[mem[TOP_OF+DATA]++] = item(&argstack, n--);
            memory[mem[TOP_OF+DATA]++] = '\0';
            break;
        case PRIM_LS:
            ls();
            result = 0;
            break;
        case PRIM_STRINGS:
            strings();
            result = 0;
            break;
        case PRIM_CODE:
            print_asm(&memory[mem[CODE]], mem[END_OF+CODE] - mem[CODE]);
            result = 0;
            break;
        case PRIM_IF:
            temp = item(&argstack, n--);
            func = item(&argstack, n--);
            if(temp) {
                push(&argstack, 0); // indicate that there are zero args to the block
                run_func(func);
                result = pop(&argstack);
            } else if (n > 1) {
                func2 = item(&argstack, n--);
                push(&argstack, 0); // indicate that there are zero args to the block
                run_func(func2);
                result = pop(&argstack);
            } else result = 0;
            n = 1;
            break;
        case PRIM_LOOP:
            func = item(&argstack, n--);
            func2 = 0;
            if (n > 1) {
                func2 = item(&argstack, n--);
            }
            do {
                push(&argstack, 0); // indicate that there are zero args to the block
                run_func(func);
                result = pop(&argstack);
                if (result != 0 && func2 != 0) {
	            push(&argstack, 0); // indicate that there are zero args to the block
                    run_func(func2);
                    pop(&argstack);
                }
            } while(result != 0);
            break;            
        case PRIM_DEFINE:
            temp = item(&argstack, n--);
            add_var(temp, item(&argstack, n--));
            break;
        case PRIM_SET:
            temp = item(&argstack, n--);
            set_var(temp, item(&argstack, n--));
            break;
        case PRIM_ADD:
            var = lookup_variable(item(&argstack, n--));
            var->value += item(&argstack, n--);
            result = var->value;
            break;
        case PRIM_SUB:
            var = lookup_variable(item(&argstack, n--));
            var->value -= item(&argstack, n--);
            result = var->value;
            break;
        case PRIM_ARGS:
            // As we call another function to pop our own args,
            // the callstack is a bit crowded, e.g.: 2 "y" "x" <prim_args> 2 43 42
            // So define args first, then pop values
            temp = mem[TOP_OF+VARS];
            for (int i=0; i<n-1;i++) add_var(item(&argstack, n-i), 0);
            drop(&argstack, n+1); // include prim func reference, n
            for (int i=0; i<n-1;i++) ((Variable *) (&memory[temp]))[i].value = item(&argstack, n-i);
            // leave the second drop to run_func
            n = 1;
            result = 0;
            break;
        case PRIM_REMAINDER:
            // Any remaining args are now on the argstack like so:
            // remaining1 remaining2 named1 named2 num_args prim_remainder n
            // 1 "z" <prim_remainder> num_args remaining2 remaining1 named2 named1
            //
            // In other words, (as our stack actually goes up), there is an array of
            // leftover values on the stack which is in right order, but is lacking
            // the right count at the right place; also, stack values are temporal.
            printf("TODO work out arrays & use for remainder args\n");
            break;
        case PRIM_ENUM:                
            for (int i=0; i<n-1;i++) add_var(item(&argstack, n-i), i);
            n = 1;
            result = 0;
            break;
        case PRIM_BITFIELD:
            for (int i=0; i<n-1;i++) add_var(item(&argstack, n-i), 1 << i);
            n = 1;
            result = 0;
            break;
        case PRIM_NOT:
            result = item(&argstack, n--);
            result = result ? 0 : 1;
            break;
        case PRIM_NOTB:
            result = ~item(&argstack, n--); // Note that the resultl is 16-bit
            break;
        case PRIM_GET_BYTE:
        case PRIM_GET_WORD:
            temp = item(&argstack, n--);
            while (n > 1) temp += item(&argstack, n--); // Allow struct indexing: getb mystruct myenum
            result = memory[temp];
            if (prim == PRIM_GET_WORD) result |= memory[temp+1] << 8;
            break;
        case PRIM_SET_BYTE:
        case PRIM_SET_WORD:
            temp = item(&argstack, n--);
            while (n > 2) temp += item(&argstack, n--); // Allow struct indexing: setb mystruct myenum
            result = item(&argstack, n--);
            memory[temp] = result & 0xFF;
            if (prim == PRIM_SET_WORD) memory[temp+1] = result >> 8;
            break;
        case PRIM_SET_ALL_BYTE:
            temp = item(&argstack, n--);
            for(int i=0; i<n-1; i++) {
                memory[temp+i] = item(&argstack, n-i);
            }
            n = 1;
            result = 0;
            break;
        case PRIM_MSB:
            temp = item(&argstack, n--);
            result = temp >> 8;
            break;
        case PRIM_LSB:
            temp = item(&argstack, n--);
            result = temp & 0xFF;
            break;
        case PRIM_READ: // Open a file for reading in callback
            if (n != 3) { printf("Wrong num of args to read\n"); break; }
            temp = item(&argstack, n--);
            func = item(&argstack, n--);
            if((readfile = fopen((const char*) &memory[temp], "r"))) {
                push(&argstack, 1); // No real file handle yet, but some differentiator from stdin 
                push(&argstack, 1); // 1 arg
                run_func(func);
                result = pop(&argstack);
                fclose(readfile);
            } else printf ("bah he\n");
            break;
        case PRIM_GETC:
            temp = n > 1 ? item(&argstack, n--) : 0;
            result = fgetc(temp == 1 ? readfile : stdin);
            break;
        default:
            // It is very easy to come here by triggering the
            // evaluation of a random value as a function.
            printf("Invalid primitive reference: %d\n", prim);
            result = 0;
            break;
    }

    if (n != 1) printf("WARN: %d leftover args\n", n);

    return result;
}

void register_base_prims() {
    uint8_t group = add_primitive_group(base_prim_group_cb);

    add_variable("return", add_primitive(group | PRIM_RETURN));
    add_variable("print", add_primitive(group | PRIM_PRINT));
    add_variable("$", add_primitive(group | PRIM_$));
    add_variable("c", add_primitive(group | PRIM_C));
    add_variable("ls", add_primitive(group | PRIM_LS));
    add_variable("strings", add_primitive(group | PRIM_STRINGS));
    add_variable("code", add_primitive(group | PRIM_CODE));
    add_variable("if", add_primitive(group | PRIM_IF));
    add_variable("loop", add_primitive(group | PRIM_LOOP));
    add_variable("define", add_primitive(group | PRIM_DEFINE));
    add_variable("set", add_primitive(group | PRIM_SET)); // consider: set (at "foo") 42; to re-use set as setw (or just use '=')
    add_variable("add", add_primitive(group | PRIM_ADD));
    add_variable("sub", add_primitive(group | PRIM_SUB));
    add_variable("args", add_primitive(group | PRIM_ARGS));
    add_variable("remainder", add_primitive(group | PRIM_REMAINDER));
    add_variable("enum", add_primitive(group | PRIM_ENUM));
    add_variable("bitfield", add_primitive(group | PRIM_BITFIELD));
    add_variable("!", add_primitive(group | PRIM_NOT));
    add_variable("~", add_primitive(group | PRIM_NOTB));
    add_variable("getb", add_primitive(group | PRIM_GET_BYTE));
    add_variable("getw", add_primitive(group | PRIM_GET_WORD));
    add_variable("setb", add_primitive(group | PRIM_SET_BYTE));
    add_variable("setw", add_primitive(group | PRIM_SET_WORD));
    add_variable("setallb", add_primitive(group | PRIM_SET_ALL_BYTE));
    add_variable("lsb", add_primitive(group | PRIM_LSB));
    add_variable("msb", add_primitive(group | PRIM_MSB));
    add_variable("read", add_primitive(group | PRIM_READ));
    add_variable("getc", add_primitive(group | PRIM_GETC));
}

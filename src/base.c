#include <stdint.h>
#include <stdio.h>

#include "pasta.h"

// Base primitive defs
enum {
    PRIM_HELLO,
    PRIM_RETURN,
    PRIM_PRINT,
    PRIM_PRINTX,
    PRIM_PRINTS,
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
    PRIM_GET_BYTE,
    PRIM_SET_BYTE,
    PRIM_SET_ALL_BYTE,
    PRIM_LSB,
    PRIM_MSB
};

uint16_t base_prim_group_cb(uint8_t prim) {
    uint16_t n = peek(&argstack);
    uint16_t temp, func, func2;
    uint16_t result = 0;
    Variable * var;

    switch(prim) {
        case PRIM_HELLO:
            // This is where you typically land when you make a wild reference
            // and the zeroes in memory suggest the first primitive function.
            printf("Hello from primitive #%d\n", prim);
            result = 0;
            break;
        case PRIM_RETURN:
            // May also call this 'pass', 'quote', 'ref', etc.
            // The point is to turn a lone value into an expression
            // instead of accidentally evaluating it
            result = item(&argstack, n--);
            break;
        case PRIM_PRINT:
            while(n != 1) { temp = item(&argstack, n--); printf("%d ", temp); }
            printf("\n");
            result = temp;
            break;
        case PRIM_PRINTX:
            while(n != 1) { temp = item(&argstack, n--); printf("%X ", temp); }
            printf("\n");
            result = temp;
            break;
        case PRIM_PRINTS:
            while(n != 1) { temp = item(&argstack, n--); printf("%s", (char *) &memory[temp]); }
            result = temp;
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
            print_asm(&memory[CODE_START+main_start], code_end-main_start);
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
            temp = vars_end;
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
        case PRIM_GET_BYTE:
            temp = item(&argstack, n--);
            if (n > 1) temp += item(&argstack, n--); // Allow struct indexing: get mystruct myenum
            result = memory[temp];
            break;
        case PRIM_SET_BYTE:
            temp = item(&argstack, n--);
            if (n > 2) temp += item(&argstack, n--); // Allow struct indexing: get mystruct myenum
            result = item(&argstack, n--);
            memory[temp] = result;
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

    add_variable("hi", add_primitive(group | PRIM_HELLO));
    add_variable("return", add_primitive(group | PRIM_RETURN));
    add_variable("print", add_primitive(group | PRIM_PRINT));
    add_variable("printx", add_primitive(group | PRIM_PRINTX));
    add_variable("prints", add_primitive(group | PRIM_PRINTS));
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
    add_variable("getb", add_primitive(group | PRIM_GET_BYTE));
    add_variable("setb", add_primitive(group | PRIM_SET_BYTE));
    add_variable("setallb", add_primitive(group | PRIM_SET_ALL_BYTE));
    add_variable("lsb", add_primitive(group | PRIM_LSB));
    add_variable("msb", add_primitive(group | PRIM_MSB));
}

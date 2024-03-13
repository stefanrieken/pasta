#include <stdint.h>
#include <stdio.h>

#include "stack.h"
#include "selfless.h"

// Base primitive defs
enum {
    PRIM_HELLO,
    PRIM_PRINT,
    PRIM_PRINTS,
    PRIM_LS,
    PRIM_STRINGS,
    PRIM_CODE,
    PRIM_IF,
    PRIM_DEFINE,
    PRIM_ARGS,
    PRIM_REMAINDER
};

uint16_t base_prim_group_cb(uint8_t prim) {
    uint16_t n = peek(&argstack);
    uint16_t temp;
    uint16_t result = 0;

        switch(prim) {
            case PRIM_HELLO:
                printf("Hello from primitive #%d\n", prim);
                result = 0;
               break;
            case PRIM_PRINT:
                while(n != 1) { temp = item(&argstack, n--); printf("%d ", temp); }
                printf("\n");
                result = temp;
                break;
            case PRIM_PRINTS:
                while(n != 1) { temp = item(&argstack, n--); printf("%s", (char *) &memory[STRING_START + temp]); }
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
                uint16_t func = item(&argstack, n--);
                if(temp) {
                    push(&argstack, 0); // indicate that there are zero args to the block
                    run_func(func);
                    result = pop(&argstack);
                } else if (n > 1) {
                    func = item(&argstack, n--);
                    push(&argstack, 0); // indicate that there are zero args to the block
                    run_func(func);
                    result = pop(&argstack);
                } else result = 0;
                n = 1;
                break;
            case PRIM_DEFINE:
                temp = item(&argstack, n--);
                add_var(temp, item(&argstack, n--)); // return the value
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
    add_variable("print", add_primitive(group | PRIM_PRINT));
    add_variable("prints", add_primitive(group | PRIM_PRINTS));
    add_variable("ls", add_primitive(group | PRIM_LS));
    add_variable("strings", add_primitive(group | PRIM_STRINGS));
    add_variable("code", add_primitive(group | PRIM_CODE));
    add_variable("if", add_primitive(group | PRIM_IF));
    add_variable("define", add_primitive(group | PRIM_DEFINE));
    add_variable("args", add_primitive(group | PRIM_ARGS));
    add_variable("remainder", add_primitive(group | PRIM_REMAINDER));
}

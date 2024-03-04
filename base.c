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
    PRIM_CODE,
    PRIM_IF,
    PRIM_DEFINE,
    PRIM_ARGS,
    PRIM_REMAINDER
};

uint16_t base_prim_group_cb(uint8_t prim, uint16_t n) {
    uint16_t temp;
    uint16_t result = 0;

        switch(prim) {
            case PRIM_HELLO:
                printf("Hello from primitive #%d\n", prim);
                result = 0;
               break;
            case PRIM_PRINT:
                while(n != 0) { temp = item(&argstack, n--); printf("%d ", temp); }
                printf("\n");
                result = temp;
                break;
            case PRIM_PRINTS:
                while(n != 0) { temp = item(&argstack, n--); printf("%s", (char *) &memory[STRING_START + temp]); }
                result = temp;
                break;
            case PRIM_LS:
                ls();
                result = 0;
                break;
            case PRIM_CODE:
                print_asm(&memory[CODE_START+main_start], code_end-main_start);
                result = 0;
                break;
            case PRIM_IF:
                temp = item(&argstack, n--);
                uint16_t func = item(&argstack, n--);
                n = 0;
                if(temp) {
                    run_func(func, 0);
                    result = pop(&argstack);
                }
                else result = 0;
                break;
            case PRIM_DEFINE:
                temp = item(&argstack, n--);
                add_var(temp, item(&argstack, n--)); // return the value
                break;
            case PRIM_ARGS:
                // As we call another function to pop our own args,
                // the callstack is a bit crowded, e.g.: "y" "x" 43 42
                // So define args first, then pop values
                temp = vars_end;
                for (int i=0; i<n;i++) add_var(item(&argstack, n-i), 0);
                drop(&argstack, n+1);
                for (int i=0; i<n;i++) ((Variable *) (&memory[temp]))[i].value = item(&argstack, n-i);
                // leave the second drop to run_func
                n = 0;
                result = 0;
                break;
            case PRIM_REMAINDER:
                printf("TODO work out arrays & use for remainder args\n");
                break;
            default:
                // It is very easy to come here by triggering the
                // evaluation of a random value as a function.
                printf("Invalid primitive reference: %d\n", prim);
                result = 0;
                break;
        }

        if (n != 0) printf("WARN: %d leftover args\n", n);

        return result;
}

void register_base_prims() {
    uint8_t group = add_primitive_group(base_prim_group_cb);

    add_variable("hi", add_primitive(group | PRIM_HELLO));
    add_variable("print", add_primitive(group | PRIM_PRINT));
    add_variable("prints", add_primitive(group | PRIM_PRINTS));
    add_variable("ls", add_primitive(group | PRIM_LS));
    add_variable("code", add_primitive(group | PRIM_CODE));
    add_variable("if", add_primitive(group | PRIM_IF));
    add_variable("define", add_primitive(group | PRIM_DEFINE));
    add_variable("args", add_primitive(group | PRIM_ARGS));
    add_variable("remainder", add_primitive(group | PRIM_REMAINDER));
}

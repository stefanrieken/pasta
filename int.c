#include <stdint.h>
#include <stdio.h>

#include "stack.h"
#include "selfless.h"

// Base primitive defs
enum {
    PRIM_PLUS, //=1,
    PRIM_MINUS, //=1,
    PRIM_TIMES,
    PRIM_DIV,
    PRIM_REST,
    PRIM_EQ
};

uint16_t int_prim_group_cb(uint8_t prim, uint16_t n) {
    uint16_t val = item(&argstack, n--);
    uint16_t result = 0;

        switch(prim) {
            case PRIM_PLUS:
                result = val + item(&argstack, n--);
                break;
            case PRIM_MINUS:
                result = val - item(&argstack, n--);
                break;
            case PRIM_TIMES:
                result = val * item(&argstack, n--);
                break;
            case PRIM_DIV:
                result = val / item(&argstack, n--);
                break;
            case PRIM_REST:
                result = val % item(&argstack, n--);
                break;
            case PRIM_EQ:
                result = val == item(&argstack, n--);
                break;
            default:
                printf("Invalid primitive reference: %d\n", prim);
                result = 0;
                break;
        }

        if (n != 0) printf("WARN: %d leftover args\n", n);

        return result;
}

void register_int_prims() {
    uint8_t group = add_primitive_group(int_prim_group_cb);

    add_variable("+", add_primitive(group | PRIM_PLUS));
    add_variable("-", add_primitive(group | PRIM_MINUS));
    add_variable("*", add_primitive(group | PRIM_TIMES));
    add_variable("/", add_primitive(group | PRIM_DIV));
    add_variable("=", add_primitive(group | PRIM_EQ));
    add_variable("%", add_primitive(group | PRIM_REST));
}

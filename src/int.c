#include <stdint.h>
#include <stdio.h>

#include "vars.h"
#include "pasta.h"

// Base primitive defs
enum {
    PRIM_PLUS, //=1,
    PRIM_MINUS, //=1,
    PRIM_TIMES,
    PRIM_DIV,
    PRIM_REST,
    PRIM_LT,
    PRIM_LTE,
    PRIM_GT,
    PRIM_GTE,
    PRIM_EQ,
    PRIM_ANDB,
    PRIM_AND,
    PRIM_ORB,
    PRIM_OR,
    PRIM_XOR,
    PRIM_LEFT_SHIFT,
    PRIM_RIGHT_SHIFT
};

uint16_t int_prim_group_cb(uint8_t prim) {
    uint16_t n = peek(&argstack);
    uint16_t result = next_arg();
        switch(prim) {
            case PRIM_PLUS:
                while (n > 1) result += next_arg();
                break;
            case PRIM_MINUS:
                while (n > 1) result -= next_arg();
                break;
            case PRIM_TIMES:
                while (n > 1) result *= next_arg();
                break;
            case PRIM_DIV:
                result = result / next_arg();
                break;
            case PRIM_REST:
                result = result % next_arg();
                break;
            case PRIM_LT:
                result = result < next_arg();
                break;
            case PRIM_LTE:
                result = result <= next_arg();
                break;
            case PRIM_GT:
                result = result > next_arg();
                break;
            case PRIM_GTE:
                result = result >= next_arg();
                break;
            case PRIM_EQ:
                result = result == next_arg();
                break;
            case PRIM_ANDB:
                while (n > 1) result &= next_arg();
                break;
            case PRIM_AND:
                // Have to overcome C's laziness; eval item at argstack first
                while (n > 1) result = next_arg() && result;
                break;
            case PRIM_ORB:
                while (n > 1) result |= next_arg();
                break;
            case PRIM_OR:
                // Have to overcome C's laziness; eval item at argstack first
                while (n > 1) result = next_arg() || result;
                break;
            case PRIM_XOR:
                while (n > 1) result ^= next_arg();
                break;
            case PRIM_LEFT_SHIFT:
                result = result << next_arg();
                break;
            case PRIM_RIGHT_SHIFT:
                result = result >> next_arg();
                break;
            default:
                printf("Invalid primitive reference: %d\n", prim);
                result = 0;
                break;
        }

        if (n != 1) printf("WARN: %d leftover args\n", n-1);

        return result;
}

void register_int_prims() {
    uint8_t group = add_primitive_group(int_prim_group_cb);

    add_variable("+", add_primitive(group | PRIM_PLUS));
    add_variable("-", add_primitive(group | PRIM_MINUS));
    add_variable("*", add_primitive(group | PRIM_TIMES));
    add_variable("/", add_primitive(group | PRIM_DIV));
    add_variable("%", add_primitive(group | PRIM_REST));
    add_variable("<", add_primitive(group | PRIM_LT));
    add_variable("<=", add_primitive(group | PRIM_LTE));
    add_variable(">", add_primitive(group | PRIM_GT));
    add_variable(">=", add_primitive(group | PRIM_GTE));
    add_variable("=", add_primitive(group | PRIM_EQ));
    add_variable("&", add_primitive(group | PRIM_ANDB));
    add_variable("&&", add_primitive(group | PRIM_AND));
    add_variable("|", add_primitive(group | PRIM_ORB));
    add_variable("||", add_primitive(group | PRIM_OR));
    add_variable("^", add_primitive(group | PRIM_XOR));
    add_variable("<<", add_primitive(group | PRIM_LEFT_SHIFT));
    add_variable(">>", add_primitive(group | PRIM_RIGHT_SHIFT));
}

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include "terminal.h"
#include "pasta.h"
#include "stack.h"
#include "vars.h"
#include "base.h"
#include "int.h"
#include "file.h"

// Run a function OR a block.
//
// is_bound:
// - should be 'true' for any block preceded by `bind` (aka `lambda`, aka "this is a function")
// - AUTO_BIND overrides this value; but best is to pass is_bound just like when binding is implicit
// - dynamic scoping also renders this parameter irrelevant; again, just pass it duly anyway
void run_func(uint16_t func, bool is_bound) {

#ifdef LEXICAL_SCOPING
    uint16_t closure = func;
#ifndef AUTO_BIND
    if (is_bound)
#endif
    {
        // 'func' actually points to an anonymous closure variable.
        func = ((Variable *) &memory[func])->value;
    }
#endif

    // Avoid interpreting zeroes and other random (but hopefully low) functions
    if (func < mem[CODE] || func > mem[TOP_OF+CODE]) { printf("Invalid function reference.\n"); return; }

    uint8_t type = memory[func];
    uint16_t num_args = peek(&argstack);
    uint16_t result = 0;

    if (type == 0) { // signals primitive func
        uint8_t prim = memory[func+1];
        uint8_t group = (prim >> 5) & 0b111;
        // Final sanity check
        if (prim_groups[group] == NULL) { printf("Primitive %d.%d not found\n", group, prim & 0b11111); return; }
        // invoke primitive group callback
        result = prim_groups[group](prim & 0b11111);
    } else {
        int saved_vars_top = mem[TOP_OF+VARS];
#ifdef LEXICAL_SCOPING
#ifndef AUTO_BIND
        if (is_bound)
#endif
        {
            // Start the function 'stack frame' with a variable referencing the closure,
            // serving as a marker to skip any intermediate variables during lookup.
            add_var(UQSTR_PARENT, closure);
        }
#endif
        // 'type' is actually a skip_code instruction
        uint16_t length = memory[func+1];
        length |= memory[func+2] << 8;
        run_code(&memory[func+3], length, false, false);
        mem[TOP_OF+VARS] = saved_vars_top;
        result = pop(&argstack);
        // assume for now all args are taken
    }

    drop(&argstack, num_args+1); // also drop 'num_args' value itself
    push(&argstack, result);
}

/**
 * Run native code eagerly by evaluating an expression's argument components
 * (including the function reference) to the argstack, then calling run_func
 */
void run_code(unsigned char * code, int length, bool toplevel, bool from_stdin) {
//    int saved_vars_top = mem[TOP_OF+VARS];
    int saved_argstack = argstack.length;
    int saved_datastack = mem[TOP_OF+DATA];

    for (int i=0; i<length; i++) {
        uint8_t cmd = code[i] & 0b11;
        uint16_t value = code[i] >> 2;
        if (value == WORD_FOLLOWS) {
            value = code[i+1];
            value |= (code[i+2] << 8);
            i+=2;
        }

        switch(cmd) {
            case CMD_EVAL:
                //printf("eval %d\n", value);
                if(value == 0) { // clear argstack at end of line
                    // Show expression result status if file is stdin,
                    // even if it is piped input
                    if(from_stdin) printf("[%d] Ok.\n", pop(&argstack));
                    argstack.length = saved_argstack;
                    mem[TOP_OF+DATA] = saved_datastack;
                } else {
                    push(&argstack, value); // push n args
                    run_func(item(&argstack, value+1), true);
                }
                break;
            case CMD_PUSH:
                //printf("push %d\n", value);
                push(&argstack, value);
                break;
            case CMD_REF:
                //printf("ref %d\n", value);
#ifdef ANALYZE_VARS
                // LSB: 1=local, 0=global
                if (value & 1) {
//                    printf("Varstack length %d\n", (mem[TOP_OF+VARS]-mem[VARS]) / sizeof(Variable));
                    Variable * var = (Variable *) &memory[mem[TOP_OF+VARS]-(value&~1)];
//                    printf("Graaf in var '%s'\n", (char *) &memory[var->name]);
                    push(&argstack, var->value);
                } else {
                    Variable * var = (Variable *) &memory[(mem[VARS]+value)];
                    push(&argstack, var->value);
                }
#else
                push(&argstack, safe_lookup_var(value)->value);
#endif
                break;
            case CMD_SKIP:
                //printf("skip %d\n", value);
#if defined(LEXICAL_SCOPING) && defined(AUTO_BIND)
                push(&argstack, bind((code-memory)+i-2));
#else
                push(&argstack, (code-memory)+i-2); // push start address of block
#endif
                i += value; // then skip
                break;
        }
    }

    // Reset scope, argstack
    if (!toplevel) {
//        mem[TOP_OF+VARS] = saved_vars_top;
        int result = pop(&argstack);
//        if (argstack.length < saved_argstack) { printf ("TODO I suspect underflow when taking args\n"); }
        argstack.length = saved_argstack;
        push(&argstack, result);
        mem[TOP_OF+DATA] = saved_datastack;
    }
}


#include <stdint.h>
#include <stdio.h>

#include "pasta.h"
#include "vars.h"

// Base primitive defs
// NOTE: this group is almost out of space! (32 max)
enum {
    PRIM_RETURN,
    PRIM_BIND,
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
    PRIM_GET,
    PRIM_GET_AT,
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
    PRIM_GETC,
    PRIM_RESET
};

void ls() {
    printf("Known variables: ");
    int i = mem[VARS];
    while(i < mem[TOP_OF+VARS]) {
        Variable * var = (Variable *) &memory[i];

#ifdef LEXICAL_SCOPING
        // Instead of printing "(closure)" a thousand times,
        // print a '*' to suggest that the preceding var may have been a function.
//        if (var->name == UQSTR_CLOSURE) { if (i != mem[VARS]) printf("*"); } else
        if (var->name != UQSTR_CLOSURE)
#endif
//        printf(" %s (%d)", str(var->name), var->value);
        printf("%s ", str(var->name));
        i += sizeof(Variable);
    }
    printf("\n");
}

void strings() {
    printf("Known strings:");
    int i = mem[STRINGS];
    while(memory[i] != 0) { printf(" %s", &memory[i+1]); i += memory[i]; }
    printf("\n");
}

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
            result = next_arg();
            break;
        case PRIM_BIND:
#if defined(LEXICAL_SCOPING) && !defined(AUTO_BIND)
            result = bind(next_arg());
#else
            // If we automatically bind functions, make manual `bind` a no-op
            // Same if we don't have lexical scoping at all
            result = next_arg();
#endif
            break;
        case PRIM_PRINT:
            while(n != 1) { temp = next_arg(); printf("%s", (char *) &memory[temp]); }
            result = temp;
            break;
        case PRIM_$:
            temp = next_arg();
            uint16_t base = n > 1 ? next_arg() : 10;
            uint16_t positions = n > 1 ? next_arg() : 0;
            result = val_to_base(temp, base, positions);
            break;
        case PRIM_C:
            result = mem[TOP_OF+DATA];
            memory[mem[TOP_OF+DATA]++] = next_arg();
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
            print_asm(&memory[mem[CODE]], mem[TOP_OF+CODE] - mem[CODE]);
            result = 0;
            break;
        case PRIM_IF:
            temp = next_arg();
            if(temp) {
                func = next_arg();
		skip_arg(); // discard 'else'
                push(&argstack, 0); // indicate that there are zero args to the block
                run_func(func, false);
                result = pop(&argstack);
            } else if (n > 1) {
		skip_arg();
                func = next_arg();
                push(&argstack, 0); // indicate that there are zero args to the block
                run_func(func, false);
                result = pop(&argstack);
            } else result = 0;
            break;
        case PRIM_LOOP:
            func = next_arg();
            func2 = 0;
            if (n > 1) {
                func2 = next_arg();
            }
            do {
                push(&argstack, 0); // indicate that there are zero args to the block
                run_func(func, false);
                result = pop(&argstack);
                // Obsolete: this is for the syntax loop { test } { loop }
                if (result != 0 && func2 != 0) {
	            push(&argstack, 0); // indicate that there are zero args to the block
                    run_func(func2, false);
                    pop(&argstack);
                }
            } while(result != 0);
            break;            
        case PRIM_DEFINE:
            temp = next_arg();
            add_var(temp, next_arg());
            break;
        case PRIM_SET:
            temp = next_arg();
            set_var(temp, next_arg());
            break;
        case PRIM_GET:
//            printf("in prim get\n");
            temp = next_arg();
            result = get_var(temp);
            break;
        case PRIM_GET_AT: // (Name may be improved) Hop to var via parent references using relative indices
//            printf("in prim get-at %d\n", n);
            temp = mem[TOP_OF+VARS];
            while (n > 1) {
                temp -= next_arg(); // now we are at the (intermediate parent) variable
                temp = ((Variable *) &memory[temp])->value; // get its pointer or end value
//            printf("Found variable %s\n", (char*) &memory[((Variable *) &memory[temp])->name]);
            }
            result = temp;
//            result = ((Variable *) &memory[temp])->value;
//            printf("Found variable %s\n", (char*) &memory[((Variable *) &memory[temp])->name]);
//            printf("done prim get-at\n");
            break;
        case PRIM_ADD:
            var = lookup_variable(next_arg());
            var->value += next_arg();
            result = var->value;
            break;
        case PRIM_SUB:
            var = lookup_variable(next_arg());
            var->value -= next_arg();
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
            result = next_arg();
            result = result ? 0 : 1;
            break;
        case PRIM_NOTB:
            result = ~next_arg(); // Note that the resultl is 16-bit
            break;
        case PRIM_GET_BYTE:
        case PRIM_GET_WORD:
            temp = next_arg();
            while (n > 1) temp += next_arg(); // Allow struct indexing: getb mystruct myenum
            result = memory[temp];
            if (prim == PRIM_GET_WORD) result |= memory[temp+1] << 8;
            break;
        case PRIM_SET_BYTE:
        case PRIM_SET_WORD:
            temp = next_arg();
            while (n > 2) temp += next_arg(); // Allow struct indexing: setb mystruct myenum
            result = next_arg();
            memory[temp] = result & 0xFF;
            if (prim == PRIM_SET_WORD) memory[temp+1] = result >> 8;
            break;
        case PRIM_SET_ALL_BYTE:
            temp = next_arg();
            for(int i=0; i<n-1; i++) {
                memory[temp+i] = item(&argstack, n-i);
            }
            n = 1;
            result = 0;
            break;
        case PRIM_MSB:
            temp = next_arg();
            result = temp >> 8;
            break;
        case PRIM_LSB:
            temp = next_arg();
            result = temp & 0xFF;
            break;
        case PRIM_READ: // Open a file for reading in callback
            if (n != 3) { printf("Wrong num of args to read\n"); break; }
            temp = next_arg();
            func = next_arg();
            if((readfile = fopen((const char*) &memory[temp], "r"))) {
                push(&argstack, 1); // No real file handle yet, but some differentiator from stdin 
                push(&argstack, 1); // 1 arg
                run_func(func, true);
                result = pop(&argstack);
                fclose(readfile);
            } else printf ("bah he\n");
            break;
        case PRIM_GETC:
            temp = n > 1 ? next_arg() : 0;
            result = fgetc(temp == 1 ? readfile : stdin);
            break;
        case PRIM_RESET:
            for (int i=0; i<8;i++) {
                mem[TOP_OF+i] = mem[INITIAL+TOP_OF+i];
                n = 1;
            }
#ifdef ANALYZE_VARS
            init_varstackmodel();
#endif
            result = 0;
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
    add_variable("bind", add_primitive(group | PRIM_BIND));
    add_variable("print", add_primitive(group | PRIM_PRINT));
    add_variable("$", add_primitive(group | PRIM_$));
    add_variable("c", add_primitive(group | PRIM_C));
    add_variable("ls", add_primitive(group | PRIM_LS));
    add_variable("strings", add_primitive(group | PRIM_STRINGS));
    add_variable("code", add_primitive(group | PRIM_CODE));
    add_variable("if", add_primitive(group | PRIM_IF));
    add_variable("loop", add_primitive(group | PRIM_LOOP));
    add_variable("define", add_primitive(group | PRIM_DEFINE));
    add_variable("set", add_primitive(group | PRIM_SET));
    add_variable("get", add_primitive(group | PRIM_GET));
    add_variable("get-at", add_primitive(group | PRIM_GET_AT));
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
    add_variable("reset", add_primitive(group | PRIM_RESET));
}

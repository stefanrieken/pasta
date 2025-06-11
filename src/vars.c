#include <stdint.h>

#include "stack.h"
#include "pasta.h"
#include "vars.h"

extern Stack varstackmodel;
Variable * add_var(uint16_t name, uint16_t value) {
//    printf("Adding var %s val %d\n", (char *) &memory[name], value);
    if (mem[TOP_OF+VARS] >= mem[END_OF+VARS]) { printf("Variable overflow! %d %d\n", mem[TOP_OF+VARS], mem[END_OF+VARS]); return 0; }
    Variable * var = (Variable *) &memory[mem[TOP_OF+VARS]];
    var->name = name;
    var->value = value;
    mem[TOP_OF+VARS] += sizeof(Variable);
    return var;
}

Variable * add_variable(char * name, uint16_t value) {
    uint16_t uqstr = unique_string(name);
    return add_var(uqstr, value);
}

Variable * quiet_lookup_variable(uint16_t name) {
    for (int i=mem[TOP_OF+VARS]-sizeof(Variable);i>=mem[VARS];i-=sizeof(Variable)) {
        Variable * var = (Variable *) &memory[i];

#ifdef LEXICAL_SCOPING
        if(var->name == UQSTR_PARENT) {
            i = var->value;
            continue;
        }
#endif

        if(var->name == name) return var;
    }
    return NULL;
}

Variable * lookup_variable(uint16_t name) {
    Variable * var = quiet_lookup_variable(name);
    if (var != NULL) return var;

    // Not found
    if (name >= mem[STRINGS] && name < mem[END_OF+STRINGS]) {
        printf("Var not found: %s\n", (char *) (&memory[name])); // may reasonably assume name string ref is ok
    } else {
        printf("Var not found: %d\n", name);
    }
    return NULL;
}

Variable * safe_lookup_var(uint16_t name) {
    Variable * var = lookup_variable(name);
    if (var == NULL) return (Variable *) (&memory[mem[VARS]]); // return false
    else return var;
}

uint16_t set_var(uint16_t name, uint16_t value) {
    Variable * var = lookup_variable(name);
    if(var != NULL) {
        var->value = value;
        return 1;// memory-((uint8_t*) var);
    }
    return 0;
}

uint16_t get_var(uint16_t name) {
    Variable * var = lookup_variable(name);
    if(var != NULL) {
        return var->value;
    }
    return 0;
}

#ifdef ANALYZE_VARS
// Call this after having added primitives / before parsing code
// Reproduces the var stack model for the top level (i.e., it maps any globals)
void init_varstackmodel() {
    varstackmodel.length=0;
    for (int i=mem[VARS];i<mem[TOP_OF+VARS]; i += sizeof(Variable)) {
        Variable * var = (Variable *) &memory[i];
        if(var->name == UQSTR_PARENT) { printf("parenting involved\n"); push(&varstackmodel, 0); }// Because we do the same on 'manual' analysis
        else push(&varstackmodel, var->name);
    }
}
#endif

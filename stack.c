#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "stack.h"

uint16_t pop(Stack * stack) {
    if(stack->length == 0) { printf("Stack underflow!\n"); return 0; }
    stack->length--;
    uint16_t result = stack->values[stack->length];
    return result;
}

void push(Stack * stack, uint16_t value) {
    if(stack->length >= stack->size) { printf("Stack overflow!\n"); return; }
    stack->values[stack->length] = value;
    stack->length++;
}

// Bracket counting uses the stack differently:
void bopen(Stack * stack) {
    if(stack->length >= stack->size) { printf("Stack overflow!\n"); return; }
    stack->values[stack->length] = 0;
    stack->length++;
}

uint16_t bclose(Stack * stack) {
    if(stack->length == 0) { printf("Stack underflow!\n"); return 0; }
    stack->length--;
    return stack->values[stack->length];
}

void count(Stack * stack, int num) {
    if(stack->length > 0)
      stack->values[stack->length-1] += num;
}

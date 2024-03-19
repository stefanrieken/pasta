#ifndef STACK_H
#define STACK_H

typedef struct Stack {
    uint16_t size;
    uint16_t length;
    uint16_t * values;
} Stack;

uint16_t pop(Stack * stack);
void push(Stack * stack, uint16_t value);
uint16_t peek(Stack * stack);
uint16_t item(Stack * stack, int i);
void drop(Stack * stack, int n);

// Bracket counting uses the stack differently:
void bopen(Stack * stack);
uint16_t bclose(Stack * stack);
void count(Stack * stack, int num);

#endif /* STACK_H */

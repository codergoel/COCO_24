#ifndef STACK_H
#define STACK_H

#include "parserDef.h"  // Ensure it includes the definition of ParseNode

typedef struct StackItem {
    ParseNode* data;
    struct StackItem* next;
} StackItem;

typedef StackItem* StackNode;

typedef struct Stack {
    StackNode top;
} Stack;

// Stack function declarations
Stack* initializeStack();
void pushStack(Stack* stack, ParseNode* node);
void popStack(Stack* stack);
ParseNode* peekStack(Stack* stack);
bool isStackEmpty(Stack* stack);

#endif  // STACK_H

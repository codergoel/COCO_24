/*
   ====================================================================
   Syntax Analyzer (Parser) Implementation
   --------------------------------------------------------------------
   This file implements a predictive parser that:
   1. Constructs a parse table from grammar rules
   2. Computes FIRST and FOLLOW sets for grammar symbols
   3. Performs top-down parsing using the parse table
   4. Builds and traverses a parse tree
   5. Reports syntax errors if found in the input
   ====================================================================
*/

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "lexer.h"
#include "lexerDef.h"
#include "parser.h"
#include "parserDef.h"
#include "stack.h"

/* ========================== GLOBAL VARIABLES ========================== */

// Mapping from non-terminal enum values to their string representation
char* nonTerminalToString[NT_NOT_FOUND];

// Array of grammar rules read from grammar file
GrammarRule* Grammar[MAX_GRAMMAR_RULES];
int numOfRules = 0;

// FIRST and FOLLOW sets for each non-terminal
FirstFollowSet** First;
FirstFollowSet** Follow;
FirstFollowSet** AutoFirst;
FirstFollowSet** AutoFollow;

// Parse table: maps [non-terminal][token] -> grammar rule
GrammarRule*** parseTable;

// Initialization flags
bool grammarLoaded = false;
bool nonTerminalsInitialized = false;
bool firstFollowComputed = false;
bool parseTreeInitialized = false;

/* ========================== STACK OPERATIONS ========================== */

/**
 * Creates and initializes a new stack for parser operations
 * 
 * @return A pointer to the newly created stack
 */
Stack* initializeStack() {
    Stack* newStack = (Stack*)malloc(sizeof(Stack));
    if (!newStack) {
        fprintf(stderr, "Memory allocation failure for new stack\n");
        exit(-1);
    }
    
    // Initialize with an empty stack (null top pointer)
    newStack->top = NULL;
    return newStack;
}

/**
 * Adds a parse tree node to the top of the stack
 * 
 * @param stack The stack to push onto
 * @param node The parse tree node to push
 */
void pushStack(Stack* stack, ParseNode* node) {
    // Allocate memory for new stack item
    StackItem* newItem = (StackItem*)malloc(sizeof(StackItem));
    if (!newItem) {
        fprintf(stderr, "Memory allocation failure while creating new stack item\n");
        exit(-1);
    }
    
    // Set up the new stack item and adjust the top pointer
    newItem->data = node;
    newItem->next = stack->top;
    stack->top = newItem;
}

/**
 * Removes the top element from the stack
 * 
 * @param stack The stack to pop from
 */
void popStack(Stack* stack) {
    if (!isStackEmpty(stack)) {
        // Save the top item, update the top pointer, then free the old top
        StackItem* temp = stack->top;
        stack->top = stack->top->next;
        free(temp);
    }
}

/**
 * Returns the top element of the stack without removing it
 * 
 * @param stack The stack to peek at
 * @return The parse tree node at the top of the stack, or NULL if empty
 */
ParseNode* peekStack(Stack* stack) {
    return isStackEmpty(stack) ? NULL : stack->top->data;
}

/**
 * Checks if the stack is empty
 * 
 * @param stack The stack to check
 * @return true if the stack is empty, false otherwise
 */
bool isStackEmpty(Stack* stack) {
    return (stack->top == NULL);
}

/* ========================== GRAMMAR SYMBOL OPERATIONS ========================== */

/**
 * Creates a new node for the linked list of symbols in grammar rules
 * 
 * @param su The symbol unit to store in the node
 * @return A pointer to the newly created symbol node
 */
SymbolNode* createSymbolNode(SymbolUnit* su) {
    // Allocate memory for the new node
    SymbolNode* newNode = (SymbolNode*)malloc(sizeof(SymbolNode));
    if (!newNode) {
        fprintf(stderr, "Could not allocate memory for symbol node\n");
        return NULL;
    }
    
    // Initialize the new node
    newNode->prev = NULL;
    newNode->next = NULL;
    newNode->symbol = su;
    
    return newNode;
}

/**
 * Creates a new doubly-linked list for storing grammar symbols
 * 
 * @return A pointer to the newly created symbol list
 */
SymbolList* createSymbolList() {
    // Allocate memory for the list structure
    SymbolList* newList = (SymbolList*)malloc(sizeof(SymbolList));
    if (!newList) {
        fprintf(stderr, "Could not allocate memory for Symbol List\n");
        return NULL;
    }
    
    // Initialize the list as empty
    newList->count = 0;
    newList->head = NULL;
    newList->tail = NULL;
    
    return newList;
}

/**
 * Inserts a symbol node at the end of a symbol list
 * 
 * @param symList The symbol list to add to
 * @param node The symbol node to insert
 */
void insertSymbolNode(SymbolList* symList, SymbolNode* node) {
    if (symList->tail == NULL) {
        // List is empty, so this node becomes both head and tail
        symList->head = node;
    } else {
        // List is not empty, add to the end and update links
        symList->tail->next = node;
        node->prev = symList->tail;
    }
    
    // Update the tail pointer and increment the count
    symList->tail = node;
    ++(symList->count);
}

/* ========================== PARSE TREE OPERATIONS ========================== */

/**
 * Creates a new node for the parse tree
 * 
 * @return A pointer to the newly created parse node
 */
ParseNode* createParseNode() {
    // Allocate memory for the node
    ParseNode* newNode = (ParseNode*)malloc(sizeof(ParseNode));
    if (!newNode) {
        fprintf(stderr, "Could not allocate memory for parse tree node\n");
        return NULL;
    }
    
    // Allocate memory for children array with initial capacity
    newNode->children = (ParseNode**)malloc(INIT_CHILD_CAPACITY * sizeof(ParseNode*));
    if (!(newNode->children)) {
        fprintf(stderr, "Could not allocate memory for parse tree node's children\n");
        return newNode;  // Return partial node, caller should check children != NULL
    }
    
    // Initialize the node's fields
    newNode->capacity = INIT_CHILD_CAPACITY;
    newNode->size = 0;
    newNode->symbol = NULL;
    newNode->ste = NULL;
    newNode->lineNumber = -1;
    
    // Initialize children array to NULL pointers
    for (int i = 0; i < INIT_CHILD_CAPACITY; i++) {
        newNode->children[i] = NULL;
    }
    
    return newNode;
}

/**
 * Creates a new parse tree with an initialized root node
 * 
 * @return A pointer to the newly created parse tree
 */
ParseTree* createParseTree() {
    // Allocate memory for the tree structure
    ParseTree* newTree = (ParseTree*)malloc(sizeof(ParseTree));
    if (!newTree) {
        fprintf(stderr, "Could not allocate memory for creating parse tree\n");
        return NULL;
    }
    
    // Create root node for the tree
    newTree->root = createParseNode();
    
    return newTree;
}

/**
 * Adds a child node to a parent node in the parse tree
 * 
 * @param parent The parent node
 * @param child The child node to add
 */
void insertChild(ParseNode* parent, ParseNode* child) {
    // Validate parameters
    if (!parent) {
        fprintf(stderr, "Parent node is NULL. Cannot add child.\n");
        return;
    }
    if (!child) {
        fprintf(stderr, "Child node is NULL. Cannot add child.\n");
        return;
    }
    
    // Check if we need to resize the children array
    if (parent->size == parent->capacity) {
        // Double the capacity
        parent->capacity *= 2;
        parent->children = (ParseNode**)realloc(parent->children, 
                                               parent->capacity * sizeof(ParseNode*));
        if (!(parent->children)) {
            fprintf(stderr, "Could not allocate memory for resizing children array\n");
            return;
        }
    }
    
    // Add child and increment size
    parent->children[(parent->size)++] = child;
}

/* ========================== INITIALIZATION FUNCTIONS ========================== */

/**
 * Initializes the mapping from non-terminal enum values to their string representations
 * This is used for error reporting and debug output
 */
void initializeNonTerminalToString() {
    // Skip if already initialized
    if (nonTerminalsInitialized)
        return;
        
    nonTerminalsInitialized = true;
    
    // Allocate memory for each string
    for (int i = 0; i < NT_NOT_FOUND; i++) {
        nonTerminalToString[i] = malloc(NON_TERMINAL_COUNT);
    }
    
    // Map each non-terminal enum to its string representation
    nonTerminalToString[program] = "<program>";
    nonTerminalToString[otherFunctions] = "<otherFunctions>";
    nonTerminalToString[mainFunction] = "<mainFunction>";
    nonTerminalToString[stmts] = "<stmts>";
    nonTerminalToString[stmt] = "<stmt>";
    nonTerminalToString[function] = "<function>";
    nonTerminalToString[input_par] = "<input_par>";
    nonTerminalToString[output_par] = "<output_par>";
    nonTerminalToString[parameter_list] = "<parameter_list>";
    nonTerminalToString[dataType] = "<dataType>";
    nonTerminalToString[remaining_list] = "<remaining_list>";
    nonTerminalToString[primitiveDatatype] = "<primitiveDatatype>";
    nonTerminalToString[constructedDatatype] = "<constructedDatatype>";
    nonTerminalToString[typeDefinitions] = "<typeDefinitions>"; 
    nonTerminalToString[actualOrRedefined] = "<actualOrRedefined>"; 
    nonTerminalToString[typeDefinition] = "<typeDefinition>"; 
    nonTerminalToString[declarations] = "<declarations>"; 
    nonTerminalToString[declaration] = "<declaration>"; 
    nonTerminalToString[otherStmts] = "<otherStmts>"; 
    nonTerminalToString[returnStmt] = "<returnStmt>"; 
    nonTerminalToString[definetypestmt] = "<definetypestmt>";
    nonTerminalToString[fieldDefinition] = "<fieldDefinition>";
    nonTerminalToString[fieldType] = "<fieldType>"; 
    nonTerminalToString[fieldDefinitions] = "<fieldDefinitions>";
    nonTerminalToString[moreFields] = "<moreFields>"; 
    nonTerminalToString[global_or_not] = "<global_or_not>"; 
    nonTerminalToString[assignmentStmt] = "<assignmentStmt>"; 
    nonTerminalToString[iterativeStmt] = "<iterativeStmt>"; 
    nonTerminalToString[conditionalStmt] = "<conditionalStmt>"; 
    nonTerminalToString[elsePart] = "<elsePart>"; 
    nonTerminalToString[ioStmt] = "<ioStmt>"; 
    nonTerminalToString[funCallStmt] = "<funCallStmt>";
    nonTerminalToString[SingleOrRecId] = "<singleOrRecId>";
    nonTerminalToString[option_single_constructed] = "<option_single_constructed>"; 
    nonTerminalToString[oneExpansion] = "<oneExpansion>"; 
    nonTerminalToString[moreExpansions] = "<moreExpansions>"; 
    nonTerminalToString[arithmeticExpression] = "<arithmeticExpression>";
    nonTerminalToString[expPrime] = "<expPrime>"; 
    nonTerminalToString[term] = "<term>"; 
    nonTerminalToString[termPrime] = "<termPrime>";
    nonTerminalToString[factor] = "<factor>"; 
    nonTerminalToString[highPrecedenceOperators] = "<highPrecedenceOperators>";
    nonTerminalToString[lowPrecedenceOperators] = "<lowPrecedenceOperators>"; 
    nonTerminalToString[outputParameters] = "<outputParameters>"; 
    nonTerminalToString[inputParameters] = "<inputParameters>"; 
    nonTerminalToString[idList] = "<idList>"; 
    nonTerminalToString[booleanExpression] = "<booleanExpression>";
    nonTerminalToString[var] = "<var>";  
    nonTerminalToString[logicalOp] = "<logicalOp>";
    nonTerminalToString[relationalOp] = "<relationalOp>"; 
    nonTerminalToString[optionalReturn] = "<optionalReturn>"; 
    nonTerminalToString[more_ids] = "<more_ids>"; 
    nonTerminalToString[A] = "<A>"; 
}

/* ========================== UTILITY FUNCTIONS ========================== */

/**
 * Converts a token string to its corresponding Token enum value
 *
 * @param str The token string (e.g., "TK_PLUS")
 * @return The corresponding Token enum value, or TK_NOT_FOUND if not found
 */
Token getTokenFromString(char* str) {
    // Search through all token strings to find a match
    for (int i = 0; i < TK_NOT_FOUND; i++) {
        if (!strcmp(tokenToString[i], str))
            return (Token)i;
    }
    return TK_NOT_FOUND;
}

/**
 * Converts a non-terminal string to its corresponding NonTerminal enum value
 *
 * @param str The non-terminal string (e.g., "<program>")
 * @return The corresponding NonTerminal enum value, or NT_NOT_FOUND if not found
 */
NonTerminal getNonTerminalFromString(char* str) {
    // Search through all non-terminal strings to find a match
    for (int i = 0; i < NT_NOT_FOUND; i++) {
        if (!strcmp(nonTerminalToString[i], str))
            return (NonTerminal)i;
    }
    return NT_NOT_FOUND;
}

/**
 * Reads grammar rules from a file and loads them into memory
 * Each rule is structured as "LHS RHS_1 RHS_2 ... RHS_n"
 */
void readGrammar() {
    // Skip if already loaded
    if (grammarLoaded)
        return;
        
    grammarLoaded = true;
    
    // Buffer for reading each line of the grammar file
    char buff[MAX_RULE_LENGTH];
    
    // Open the grammar file
    FILE* fp = fopen("grammar.txt", "r");
    if (!fp) {
        fprintf(stderr, "Could not open grammar file\n");
        return;
    }
    
    // For tokenizing the input line
    char* oneTok;
    
    // Process each line (rule) from the grammar file
    while (fgets(buff, MAX_RULE_LENGTH, fp)) {
        // Get LHS (left-hand side) of the rule
        oneTok = strtok(buff, " \t\n\r");
        
        // Create a new grammar rule
        GrammarRule* gRule = (GrammarRule*)malloc(sizeof(GrammarRule));
        if (!gRule) {
            fprintf(stderr, "Could not allocate memory for rule\n");
            return;
        }
        
        // Create and set up the LHS symbol
        gRule->lhs = (SymbolUnit*)malloc(sizeof(SymbolUnit));
        if (!(gRule->lhs)) {
            fprintf(stderr, "Could not allocate memory for grammar symbol\n");
            return;
        }
        gRule->lhs->isNonTerminal = true;
        gRule->lhs->value.nt = getNonTerminalFromString(oneTok);
        
        // Create the right-hand side symbol list
        gRule->rhs = createSymbolList();
        
        // Process all tokens on the right-hand side of the rule
        oneTok = strtok(NULL, " \t\n\r");
        while (oneTok) {
            // Create a symbol unit for this RHS token
            SymbolUnit* su = (SymbolUnit*)malloc(sizeof(SymbolUnit));
            if (!su) {
                fprintf(stderr, "Could not allocate memory for RHS grammar symbol\n");
                break;
            }
            
            // Check if it's a non-terminal (starts with '<')
            if (oneTok[0] == '<') {
                su->isNonTerminal = true;
                su->value.nt = getNonTerminalFromString(oneTok);
            } else {
                // It's a terminal token (add "TK_" prefix for lookup)
                char tmp[TOKEN_STR_LEN] = "TK_";
                strcat(tmp, oneTok);
                su->isNonTerminal = false;
                su->value.t = getTokenFromString(tmp);
            }
            
            // Create a node and add it to the RHS list
            SymbolNode* rhsNode = createSymbolNode(su);
            insertSymbolNode(gRule->rhs, rhsNode);
            
            // Get the next token
            oneTok = strtok(NULL, " \t\n\r");
        }
        
        // Add the rule to the grammar
        Grammar[numOfRules++] = gRule;
    }
    
    fclose(fp);
}

/**
 * Prints the grammar rules to a file for debugging/verification
 */
void printGrammar() {
    FILE* fp = fopen("readgrm.txt", "w");
    
    // Print each rule with its components
    for (int i = 0; i < numOfRules; i++) {
        GrammarRule* tmpRule = Grammar[i];
        fprintf(fp, "%d. %s: ", i + 1, nonTerminalToString[tmpRule->lhs->value.nt]);
        
        // Iterate through the symbols on the RHS
        SymbolNode* tsNode = tmpRule->rhs->head;
        for (int j = 0; j < tmpRule->rhs->count; j++) {
            if (tsNode->symbol->isNonTerminal)
                fprintf(fp, "%s\t", nonTerminalToString[tsNode->symbol->value.nt]);
            else    
                fprintf(fp, "%s\t", tokenToString[tsNode->symbol->value.t]);
            tsNode = tsNode->next;
        }
        fprintf(fp, "\n");
    }
    
    fclose(fp);
}

/* ========================== FIRST & FOLLOW SET OPERATIONS ========================== */

/**
 * Inserts a node into a FIRST or FOLLOW set list
 *
 * @param lst The FIRST or FOLLOW set list
 * @param nd The node to insert
 */
void insertFirstFollowNode(FirstFollowSet* lst, FirstFollowNode* nd) {
    if (!(lst->tail)) {
        // List is empty
        lst->head = nd;
        lst->tail = nd;
    } else {
        // Add to the end
        lst->tail->next = nd;
        lst->tail = nd;
    }
}

/**
 * Prints the computed FIRST and FOLLOW sets to separate files
 */
void printComputedFirstAndFollow() {
    // Print FIRST sets
    FILE* foutFirst = fopen("computedfrst.txt", "w");
    for (int i = 0; i < NT_NOT_FOUND; i++) {
        fprintf(foutFirst, "%s:\t", nonTerminalToString[i]);
        FirstFollowNode* tmp = AutoFirst[i]->head;
        for (; tmp; tmp = tmp->next) {
            fprintf(foutFirst, "%s\t", tokenToString[tmp->tk]);
        }
        fprintf(foutFirst, "\n");
    }
    fclose(foutFirst);

    // Print FOLLOW sets
    FILE* foutFollow = fopen("computedfllw.txt", "w");
    for (int i = 0; i < NT_NOT_FOUND; i++) {
        fprintf(foutFollow, "%s:\t", nonTerminalToString[i]);
        FirstFollowNode* tmp = AutoFollow[i]->head;
        for (; tmp; tmp = tmp->next) {
            fprintf(foutFollow, "%s\t", tokenToString[tmp->tk]);
        }
        fprintf(foutFollow, "\n");
    }
    fclose(foutFollow);
}

/**
 * Checks if a token exists in a given FIRST or FOLLOW set
 *
 * @param lst The FIRST or FOLLOW set to check
 * @param tmpkey The token to look for
 * @return true if the token exists in the set, false otherwise
 */
bool existsInFirstFollow(FirstFollowSet* lst, Token tmpkey) {
    if (!lst) return false;
    
    // Iterate through the list to find the token
    FirstFollowNode* current = lst->head;
    for (; current; current = current->next) {
        if (current->tk == tmpkey)
            return true;
    }
    
    return false;
}

/**
 * Computes the FIRST set of a sequence of grammar symbols (RHS of a rule)
 *
 * @param grhs The symbol list (right-hand side of a rule)
 * @return The FIRST set of the symbol sequence
 */
FirstFollowSet* getFirstOfRhs(SymbolList* grhs) {
    // Create a new set for the result
    FirstFollowSet* res = (FirstFollowSet*)malloc(sizeof(FirstFollowSet));
    res->head = NULL;
    res->tail = NULL;
    
    // Start with the first symbol in the sequence
    SymbolNode* ritr = grhs->head;
    bool hasEps = true;  // Assume epsilon derivability initially
    
    // Process symbols until we find one that doesn't derive epsilon
    for (; ritr && hasEps; ritr = ritr->next) {
        FirstFollowNode* tmp;
        hasEps = false;  // Reset for this iteration
        
        // If it's a terminal, add it to FIRST and we're done
        if (!(ritr->symbol->isNonTerminal)) {
            if (!existsInFirstFollow(res, ritr->symbol->value.t)) {
                FirstFollowNode* vtmp = (FirstFollowNode*)malloc(sizeof(FirstFollowNode));
                vtmp->next = NULL;
                vtmp->tk = ritr->symbol->value.t;
                insertFirstFollowNode(res, vtmp);
            }
            break;  // Terminals don't derive epsilon, so stop here
        }
        
        // For non-terminals, add all tokens from its FIRST set
        for (tmp = AutoFirst[ritr->symbol->value.nt]->head; tmp; tmp = tmp->next) {
            if (tmp->tk == EPS) {
                hasEps = true;  // This non-terminal can derive epsilon
                continue;        // Don't add epsilon yet
            }
            
            // Add the token if it's not already in the result
            if (!existsInFirstFollow(res, tmp->tk)) {
                FirstFollowNode* cpytmp = (FirstFollowNode*)malloc(sizeof(FirstFollowNode));
                cpytmp->next = NULL;
                cpytmp->tk = tmp->tk;
                insertFirstFollowNode(res, cpytmp);
            }
        }
    }
    
    // If we went through all symbols and they all derive epsilon,
    // then add epsilon to the result
    if (ritr == NULL && hasEps) {
        FirstFollowNode* vvtmp = (FirstFollowNode*)malloc(sizeof(FirstFollowNode));
        vvtmp->next = NULL;
        vvtmp->tk = EPS;
        if (!existsInFirstFollow(res, EPS)) 
            insertFirstFollowNode(res, vvtmp);
    }
    
    return res;
}

/**
 * Frees the memory allocated for a FIRST or FOLLOW set
 *
 * @param rl The set to free
 */
void freeUpFirstFollow(FirstFollowSet* rl) {
    if (!rl) return;
    
    // Free each node in the list
    FirstFollowNode *tmp, *tn;
    tmp = rl->head;
    while (tmp) {
        tn = tmp;
        tmp = tmp->next;
        free(tn);
    }
}

/**
 * Populates the parse table using the grammar rules and FIRST/FOLLOW sets
 */
void addRulesToParseTable() {
    // For each grammar rule
    for (int gri = 0; gri < numOfRules; ++gri) {
        GrammarRule* tmpRule = Grammar[gri];
        
        // Get the FIRST set of the RHS
        FirstFollowSet* fOfRhs = getFirstOfRhs(tmpRule->rhs);
        FirstFollowNode* tItr;
        
        // For each terminal in FIRST(RHS), add the rule to the parse table
        for (tItr = fOfRhs->head; tItr; tItr = tItr->next) {
            // Check for conflicts (multiple rules for same cell)
            if (parseTable[tmpRule->lhs->value.nt][tItr->tk] != NULL)
                fprintf(stderr, "\nMultiple defined entries in parse table detected! (Overwriting the rule!)\n");
            
            // Add the rule to the parse table
            parseTable[tmpRule->lhs->value.nt][tItr->tk] = tmpRule;
        }
        
        // If epsilon is in FIRST(RHS), add the rule for each token in FOLLOW(LHS)
        if (existsInFirstFollow(fOfRhs, EPS)) {
            FirstFollowNode* antItr;
            for (antItr = AutoFollow[tmpRule->lhs->value.nt]->head; antItr; antItr = antItr->next) {
                // Check for conflicts
                if (parseTable[tmpRule->lhs->value.nt][antItr->tk] != NULL)
                    fprintf(stderr, "\nMultiple defined entries in parse table detected! (Overwriting the rule!)\n");
                
                // Add the rule to the parse table
                parseTable[tmpRule->lhs->value.nt][antItr->tk] = tmpRule;
            }
        }
        
        // Free the temporary FIRST set
        freeUpFirstFollow(fOfRhs);
    }
}

/**
 * Creates and initializes the parse table structure
 */
void initializeParseTable() {
    // Skip if already initialized
    if (parseTreeInitialized) return;
    parseTreeInitialized = true;
    
    // Allocate memory for the parse table (NT_NOT_FOUND × TK_NOT_FOUND matrix)
    parseTable = (GrammarRule***)malloc(NT_NOT_FOUND * sizeof(GrammarRule**));
    if (!parseTable) {
        fprintf(stderr, "Couldn't allocate memory for parseTable rows\n");
        return;
    }
    
    // Allocate memory for each row and initialize to NULL
    for (int nti = 0; nti < NT_NOT_FOUND; nti++) {
        parseTable[nti] = (GrammarRule**)malloc(TK_NOT_FOUND * sizeof(GrammarRule*));
        if (!parseTable[nti]) {
            fprintf(stderr, "Couldn't allocate memory for parseTable columns\n");
            return;
        }
        
        // Initialize all entries to NULL (no rule)
        for (int tki = 0; tki < TK_NOT_FOUND; tki++) {
            parseTable[nti][tki] = NULL;
        }
    }
    
    // Populate the parse table with rules
    addRulesToParseTable();
}

/**
 * Prints a single grammar rule to a file
 *
 * @param rule The grammar rule to print
 * @param fp The file to print to
 */
void printRule(GrammarRule* rule, FILE* fp) {
    // Print LHS
    fprintf(fp, "Lhs: %s\n", nonTerminalToString[rule->lhs->value.nt]);
    
    // Print RHS
    fprintf(fp, "Rhs: ");
    SymbolNode* tmp;
    for (tmp = rule->rhs->head; tmp; tmp = tmp->next) {
        if (tmp->symbol->isNonTerminal)
            fprintf(fp, "%s\t", nonTerminalToString[tmp->symbol->value.nt]);
        else    
            fprintf(fp, "%s\t", tokenToString[tmp->symbol->value.t]);
    }
    fprintf(fp, "\n");
}

/**
 * Prints the entire parse table to a file for debugging
 */
void printParseTable() {
    FILE* fParseOut = fopen("parseTable.txt", "w");
    if (!fParseOut) {
        fprintf(stderr, "Could not open file for printing parse table\n");
        return;
    }
    
    // Print each entry in the parse table
    for (int i = 0; i < NT_NOT_FOUND; i++) {
        fprintf(fParseOut, "------------------------------Non Term %d -----------------------------\n", i);
        for (int j = 0; j < TK_NOT_FOUND; j++) {
            fprintf(fParseOut, "******Term %d ******\n", j);
            GrammarRule* tmpRule = parseTable[i][j];
            if (tmpRule)
                printRule(tmpRule, fParseOut);
            else
                fprintf(fParseOut, "Error Entry\n");
        }
    }
    
    fclose(fParseOut);
}

/* ========================== PARSE TREE PRINTING FUNCTIONS ========================== */

/**
 * Prints information about a parse tree node in a formatted manner
 *
 * @param curr The current node to print
 * @param par The parent of the current node
 * @param fp The file to print to
 */
void printTreeNode(ParseNode* curr, ParseNode* par, FILE* fp) {
    // Print lexeme (or ----- for non-terminals)
    fprintf(fp, "%*s ", 32, !(curr->symbol->isNonTerminal) ? curr->ste->lexeme : "-----");
    
    // Print line number
    fprintf(fp, "%*d ", 12, curr->lineNumber);
    
    // Print token name (or ----- for non-terminals)
    fprintf(fp, "%*s ", 16, curr->symbol->isNonTerminal ? "-----" : tokenToString[curr->ste->tokenType]);
    
    // Print numeric value for numbers, or "Not number" otherwise
    if (!(curr->symbol->isNonTerminal) && (curr->ste->tokenType == NUM || curr->ste->tokenType == RNUM)) {
        if (curr->ste->tokenType == NUM)
            fprintf(fp, "%*d ", 20, (int)(curr->ste->numericValue));
        else    
            fprintf(fp, "%20.2lf ", curr->ste->numericValue);
    } else {
        fprintf(fp, "%*s ", 20, "Not number ");
    }
    
    // Print parent node symbol
    fprintf(fp, "%*s ", 30, par ? nonTerminalToString[par->symbol->value.nt] : "ROOT");
    
    // Print whether it's a leaf node
    fprintf(fp, "%*s ", 12, curr->symbol->isNonTerminal ? "NO" : "YES");
    
    // Print node symbol
    fprintf(fp, "%*s ", 30, curr->symbol->isNonTerminal ? nonTerminalToString[curr->symbol->value.nt] : "-----");
    fprintf(fp, "\n");
}

/**
 * Performs an inorder traversal of the parse tree and prints each node
 *
 * @param curr The current node to traverse
 * @param par The parent of the current node
 * @param fp The file to print to
 */
void inorderTraverse(ParseNode* curr, ParseNode* par, FILE* fp) {
    if (!curr)
        return;
    
    // Traverse left subtree
    if (curr->size)
        inorderTraverse(curr->children[0], curr, fp);
    
    // Print current node
    printTreeNode(curr, par, fp);
    
    // Traverse right subtree
    for (int chi = 1; chi < curr->size; ++chi)
        inorderTraverse(curr->children[chi], curr, fp);
}

/**
 * Prints the entire parse tree to a file
 *
 * @param PT The parse tree to print
 * @param outFile The file to print to
 */
void printParseTree(ParseTree* PT, char* outFile) {
    FILE* fp = fopen(outFile, "w");
    if (!fp) {
        fprintf(stderr, "Could not open file for printing parse tree\n");
        return;
    }
    
    if (!PT) {
        fprintf(stderr, "Given parse tree is NULL. Cannot print\n");
        return;
    }
    
    if (debugPrint)
        printf("Printing Parse Tree in the specified file...\n");
    
    // Print header
    fprintf(fp, "%*s %*s %*s %*s %*s %*s %*s\n\n", 32, "lexeme", 12, "lineNum", 16, "tokenName", 20, "valueIfNumber", 30, "parentNodeSymbol", 12, "isLeafNode", 30, "nodeSymbol");
    
    // Traverse and print the tree
    inorderTraverse(PT->root, NULL, fp);
    
    fclose(fp);
    
    if (debugPrint)
        printf("Printing parse tree completed...\n");
}

/* ========================== PARSING FUNCTIONS ========================== */

/**
 * Parses the token list using the parse table and builds the corresponding parse tree
 * Reports syntax errors if any
 *
 * @param tokensFromLexer The list of tokens from the lexer
 * @param hasSyntaxError Pointer to a boolean flag indicating if syntax errors were found
 * @return The constructed parse tree, or NULL if parsing failed
 */
ParseTree* parseTokens(TokenList* tokensFromLexer, bool* hasSyntaxError) {
    if (!tokensFromLexer) {
        fprintf(stderr, "Tokens list from lexer is NULL. Parsing failed\n");
        return NULL;
    }
    
    // Initialize input pointer and parse tree
    TokenNode* inputPtr = tokensFromLexer->head;
    ParseTree* theParseTree = createParseTree();
    ParseNode* currentNode = theParseTree->root;
    
    // Create and set up the root symbol
    SymbolUnit* su = (SymbolUnit*)malloc(sizeof(SymbolUnit));
    su->isNonTerminal = true;
    su->value.nt = program;
    currentNode->symbol = su;
    
    // Initialize stack and push the root node
    Stack* theStack = initializeStack();
    pushStack(theStack, currentNode);
    
    int cln = 1;  // Current line number
    
    if (debugPrint)
        printf("Parsing starting...\n"), fflush(stdout);
    
    // Main parsing loop
    while (!isStackEmpty(theStack) && inputPtr) {
        cln = inputPtr->lineNum;
        currentNode = peekStack(theStack);
        
        // Skip comments and lexical errors
        if (inputPtr->entry->tokenType == COMMENT || inputPtr->entry->tokenType >= LEXICAL_ERROR) {
            if (inputPtr->entry->tokenType == LEXICAL_ERROR) {
                if (debugPrint)
                    printf("Line %*d \tError: Unrecognized pattern: \"%s\"\n", 5, inputPtr->lineNum, inputPtr->entry->lexeme);
            }
            else if (inputPtr->entry->tokenType == ID_LENGTH_EXC) {
                if (debugPrint)
                    printf("Line %*d \tError: Too long identifier: \"%s\"\n", 5, inputPtr->lineNum, inputPtr->entry->lexeme);
            }
            else if (inputPtr->entry->tokenType == FUN_LENGTH_EXC) {
                if (debugPrint)
                    printf("Line %*d \tError: Too long function name: \"%s\"\n", 5, inputPtr->lineNum, inputPtr->entry->lexeme);
            }
            if (inputPtr->entry->tokenType != COMMENT)
                *hasSyntaxError = true;
            inputPtr = inputPtr->next;
            continue;
        }
        
        // Handle epsilon transitions
        if (!(currentNode->symbol->isNonTerminal) && currentNode->symbol->value.t == EPS) {
            currentNode->lineNumber = inputPtr->lineNum;
            SymbolTableEntry* tste = (SymbolTableEntry*)malloc(sizeof(SymbolTableEntry));
            strcpy(tste->lexeme, "EPSILON");
            tste->numericValue = 0;
            currentNode->ste = tste;
            tste->tokenType = EPS;
            popStack(theStack);
            continue;
        }
        
        // Handle terminal matches
        if (!(currentNode->symbol->isNonTerminal) && currentNode->symbol->value.t == inputPtr->entry->tokenType) {
            currentNode->lineNumber = inputPtr->lineNum;
            currentNode->ste = inputPtr->entry;
            popStack(theStack);
            inputPtr = inputPtr->next;
        }
        // Handle terminal mismatches
        else if (!(currentNode->symbol->isNonTerminal)) {
            *hasSyntaxError = true;
            if (debugPrint)
                printf("Line %*d \tError: The token %s for lexeme \"%s\" does not match the expected token %s\n", 
                    5, inputPtr->lineNum, tokenToString[inputPtr->entry->tokenType], 
                    inputPtr->entry->lexeme, tokenToString[currentNode->symbol->value.t]);
            currentNode->lineNumber = inputPtr->lineNum;
            popStack(theStack);
        }
        // Handle non-terminal mismatches
        else if (parseTable[currentNode->symbol->value.nt][inputPtr->entry->tokenType] == NULL) {
            *hasSyntaxError = true;
            if (debugPrint)
                printf("Line %*d \tError: Invalid token %s encountered with value \"%s\". Stack top is: %s\n", 
                    5, inputPtr->lineNum, tokenToString[inputPtr->entry->tokenType], 
                    inputPtr->entry->lexeme, nonTerminalToString[currentNode->symbol->value.nt]);
            if (existsInFirstFollow(AutoFollow[currentNode->symbol->value.nt], inputPtr->entry->tokenType)) {
                currentNode->lineNumber = inputPtr->lineNum;
                popStack(theStack);
            } else {
                inputPtr = inputPtr->next;
                if (!inputPtr) {
                    popStack(theStack);
                    currentNode = peekStack(theStack);
                }
            }
        }
        // Handle valid non-terminal transitions
        else {
            GrammarRule* tmpRule = parseTable[currentNode->symbol->value.nt][inputPtr->entry->tokenType];
            popStack(theStack);
            currentNode->lineNumber = inputPtr->lineNum;
            SymbolNode* trItr = tmpRule->rhs->head;
            ParseNode* pn;
            while (trItr) {
                pn = createParseNode();
                pn->symbol = (SymbolUnit*)malloc(sizeof(SymbolUnit));
                pn->symbol->isNonTerminal = trItr->symbol->isNonTerminal;
                if (pn->symbol->isNonTerminal)
                    pn->symbol->value.nt = trItr->symbol->value.nt;
                else 
                    pn->symbol->value.t = trItr->symbol->value.t;
                insertChild(currentNode, pn);
                trItr = trItr->next;
            }
            for (int chi = currentNode->size - 1; chi >= 0; chi--) {
                pushStack(theStack, currentNode->children[chi]);
            }
        }
    }
    
    // Check for successful parsing
    if (!(*hasSyntaxError) && isStackEmpty(theStack) && (!inputPtr || inputPtr->entry->tokenType == DOLLAR)) {
        if (debugPrint)
            printf("\nParsing successful! No syntax errors! The input is syntactically correct!\n");
    } else {
        *hasSyntaxError = true;
        while (!isStackEmpty(theStack)) {
            currentNode = peekStack(theStack);
            if (currentNode->symbol->isNonTerminal) {
                if (debugPrint)
                    printf("Line %*d \tError: Invalid token TK_DOLLAR encountered. Stack top is: %s\n", 
                        5, cln, nonTerminalToString[currentNode->symbol->value.nt]);
            } else {
                if (debugPrint)
                    printf("Line %*d \tError: The token TK_DOLLAR for lexeme \"\" does not match the expected token %s\n", 
                        5, cln, tokenToString[currentNode->symbol->value.t]);
            }
            popStack(theStack);
        }
        while (inputPtr && inputPtr->entry->tokenType != DOLLAR) {
            if (debugPrint)
                printf("Line %*d \tError: Invalid token %s encountered with value \"%s\". Stack top is: TK_DOLLAR\n", 
                    5, inputPtr->lineNum, tokenToString[inputPtr->entry->tokenType], inputPtr->entry->lexeme);
            inputPtr = inputPtr->next;
        }
        if (debugPrint)
            printf("\nThe input file has syntactic errors!\n");
    }
    
    return theParseTree;
}

/**
 * Computes the FIRST sets for all non-terminals
 */
void computeFirstSets() {
    bool modified = true;
    
    // Iterate until no more changes are made
    while (modified) {
        modified = false;
        
        // For each grammar rule
        for (int gri = 0; gri < numOfRules; ++gri) {
            NonTerminal currLhs = Grammar[gri]->lhs->value.nt;
            SymbolNode* rhsItr = Grammar[gri]->rhs->head;
            bool hasEps = true;
            
            // Process symbols on the RHS
            while (rhsItr && hasEps) {
                hasEps = false;
                
                // If it's a terminal, add it to FIRST and we're done
                if (!(rhsItr->symbol->isNonTerminal)) {
                    if (!existsInFirstFollow(AutoFirst[currLhs], rhsItr->symbol->value.t)) {
                        FirstFollowNode* vtmp = (FirstFollowNode*)malloc(sizeof(FirstFollowNode));
                        vtmp->next = NULL;
                        vtmp->tk = rhsItr->symbol->value.t;
                        insertFirstFollowNode(AutoFirst[currLhs], vtmp);
                        modified = true;
                    }
                    break;
                }
                
                // For non-terminals, add all tokens from its FIRST set
                FirstFollowNode* feaItr;
                for (feaItr = AutoFirst[rhsItr->symbol->value.nt]->head; feaItr; feaItr = feaItr->next) {
                    if (feaItr->tk == EPS) {
                        hasEps = true;
                        continue;
                    }
                    if (!existsInFirstFollow(AutoFirst[currLhs], feaItr->tk)) {
                        FirstFollowNode* cpytmp = (FirstFollowNode*)malloc(sizeof(FirstFollowNode));
                        cpytmp->next = NULL;
                        cpytmp->tk = feaItr->tk;
                        insertFirstFollowNode(AutoFirst[currLhs], cpytmp);
                        modified = true;
                    }
                }
                rhsItr = rhsItr->next;
            }
            
            // If all symbols derive epsilon, add epsilon to FIRST
            if (!rhsItr && hasEps) {
                if (!existsInFirstFollow(AutoFirst[currLhs], EPS)) {
                    FirstFollowNode* vvtmp = (FirstFollowNode*)malloc(sizeof(FirstFollowNode));
                    vvtmp->next = NULL;
                    vvtmp->tk = EPS;
                    insertFirstFollowNode(AutoFirst[currLhs], vvtmp);
                    modified = true;
                }
            }
        }
    }
}

/**
 * Computes the FOLLOW sets for all non-terminals
 */
void computeFollowSets() {
    // Add $ to FOLLOW(program)
    FirstFollowNode* thdl = (FirstFollowNode*)malloc(sizeof(FirstFollowNode));
    thdl->next = NULL;
    thdl->tk = DOLLAR;
    insertFirstFollowNode(AutoFollow[program], thdl);

    bool modified = true;
    
    // Iterate until no more changes are made
    while (modified) {
        modified = false;
        
        // For each grammar rule
        for (int gri = 0; gri < numOfRules; ++gri) {
            NonTerminal currLhs = Grammar[gri]->lhs->value.nt;
            SymbolNode* rhsItr = Grammar[gri]->rhs->head;
            
            // Process symbols on the RHS
            while (rhsItr) {
                if (!(rhsItr->symbol->isNonTerminal)) {
                    rhsItr = rhsItr->next;
                    continue;
                }
                
                // Create a temporary symbol list starting from the next node
                SymbolList* tgl = (SymbolList*)malloc(sizeof(SymbolList));
                tgl->head = rhsItr->next;
                tgl->tail = NULL;
                tgl->count = 0;
                
                // Get FIRST of the remaining symbols
                FirstFollowSet* fOfNxtT = getFirstOfRhs(tgl);
                FirstFollowNode* felItr;
                
                // Add tokens from FIRST to FOLLOW
                for (felItr = fOfNxtT->head; felItr; felItr = felItr->next) {
                    if (felItr->tk == EPS) continue;
                    if (!existsInFirstFollow(AutoFollow[rhsItr->symbol->value.nt], felItr->tk)) {
                        FirstFollowNode* vvvt = (FirstFollowNode*)malloc(sizeof(FirstFollowNode));
                        vvvt->next = NULL;
                        vvvt->tk = felItr->tk;
                        insertFirstFollowNode(AutoFollow[rhsItr->symbol->value.nt], vvvt);
                        modified = true;
                    }
                }
                
                // If epsilon is in FIRST, add FOLLOW(LHS) to FOLLOW(current)
                if (existsInFirstFollow(fOfNxtT, EPS) || !(rhsItr->next)) {
                    FirstFollowNode* flwItr;
                    for (flwItr = AutoFollow[currLhs]->head; flwItr; flwItr = flwItr->next) {
                        if (!existsInFirstFollow(AutoFollow[rhsItr->symbol->value.nt], flwItr->tk)) {
                            FirstFollowNode* avt = (FirstFollowNode*)malloc(sizeof(FirstFollowNode));
                            avt->next = NULL;
                            avt->tk = flwItr->tk;
                            insertFirstFollowNode(AutoFollow[rhsItr->symbol->value.nt], avt);
                            modified = true;
                        }
                    }
                }
                
                // Free temporary FIRST set
                freeUpFirstFollow(fOfNxtT);
                free(tgl);
                rhsItr = rhsItr->next;
            }
        }
    }
}

/**
 * Allocates data structures for FIRST and FOLLOW sets and computes them
 */
void initializeAndComputeFirstAndFollow() {
    // Skip if already computed
    if (firstFollowComputed)
        return;
        
    firstFollowComputed = true;
    
    // Allocate memory for FIRST and FOLLOW sets
    AutoFirst = (FirstFollowSet**)malloc(NT_NOT_FOUND * sizeof(FirstFollowSet*));
    AutoFollow = (FirstFollowSet**)malloc(NT_NOT_FOUND * sizeof(FirstFollowSet*));
    if (!AutoFirst || !AutoFollow) {
        fprintf(stderr, "Could not allocate memory for auto FIRST and FOLLOW sets array\n");
        return;
    }
    
    // Initialize each set
    for (int i = 0; i < NT_NOT_FOUND; i++) {
        AutoFirst[i] = (FirstFollowSet*)malloc(sizeof(FirstFollowSet));
        if (!(AutoFirst[i])) {
            fprintf(stderr, "Could not allocate memory for auto FIRST sets\n");
            return;
        }
        AutoFirst[i]->head = NULL;
        AutoFirst[i]->tail = NULL;
        
        AutoFollow[i] = (FirstFollowSet*)malloc(sizeof(FirstFollowSet));
        if (!(AutoFollow[i])) {
            fprintf(stderr, "Could not allocate memory for auto FOLLOW sets\n");
            return;
        }
        AutoFollow[i]->head = NULL;
        AutoFollow[i]->tail = NULL;
    }
    
    // Compute FIRST and FOLLOW sets
    computeFirstSets();
    computeFollowSets();
}

/**
 * Wrapper function for the parser. Initializes data structures, reads the grammar,
 * computes FIRST/FOLLOW sets, builds the parse table, and parses the input source code.
 *
 * @param inpFile The input source code file
 * @param opFile The output file for the parse tree
 */
void parseInputSourceCode(char* inpFile, char* opFile) {
    // Open input file
    FILE* ifp = fopen(inpFile, "r");
    if (!ifp) {
        fprintf(stderr, "Could not open input file for parsing\n");
        return;
    }
    
    // Lex the input file
    TokenList* tokensFromLexer = lexInput(ifp, opFile);
    fclose(ifp);
    
    // Initialize data structures
    initializeNonTerminalToString();
    readGrammar();
    initializeAndComputeFirstAndFollow();
    // printComputedFirstAndFollow();  // Uncomment if needed
    initializeParseTable();
    // printParseTable();  // Uncomment if needed

    // Parse the tokens and build the parse tree
    bool hasSyntaxError = false;
    ParseTree* parseTree = parseTokens(tokensFromLexer, &hasSyntaxError);
    
    // Print the parse tree if no syntax errors
    if (!hasSyntaxError)
        printParseTree(parseTree, opFile);
    else {
        FILE* foptp = fopen(opFile, "w");
        if (!foptp) {
            fprintf(stderr, "Could not open file for printing parser output\n");
            return;
        }
        fprintf(foptp, "There were syntax errors in the input file. Not printing the parse tree!\nCheck the console for error details.");
        fclose(foptp);
    }
}

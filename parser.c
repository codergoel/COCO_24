#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "../lexer/lexer.h"
#include "../lexer/lexerDef.h"
#include "parser.h"
#include "parserDef.h"
#include "stack.h"

/* Global Variables */
char* nonTerminalToString[NT_NOT_FOUND];
GrammarRule* Grammar[MAX_GRAMMAR_RULES];
int numOfRules = 0;
FirstFollowSet** First;
FirstFollowSet** Follow;
FirstFollowSet** AutoFirst;
FirstFollowSet** AutoFollow;
GrammarRule*** parseTable;

bool grammarLoaded = false;
bool nonTerminalsInitialized = false;
bool firstFollowComputed = false;
bool parseTreeInitialized = false;

/* Stack ADT */
Stack* initializeStack() {
    // Creates a new stack (linked list implementation)
    Stack* newStack = (Stack*)malloc(sizeof(Stack));
    if (newStack == NULL) {
        printf("Memory allocation failure for new stack\n");
        exit(-1);
    }
    newStack->top = NULL;
    return newStack;
}

void pushStack(Stack* stack, ParseNode* node) {
    // Pushes an element onto the top of the stack.
    StackItem* newItem = (StackItem*)malloc(sizeof(StackItem));
    if (newItem == NULL) {
        printf("Memory allocation failure while creating new stack item\n");
        exit(-1);
    }
    newItem->data = node;
    newItem->next = stack->top;
    stack->top = newItem;
}

void popStack(Stack* stack) {
    // Pops the top element of the stack if not empty.
    if (!isStackEmpty(stack)) {
        StackItem* temp = stack->top;
        stack->top = stack->top->next;
        free(temp);
    }
}

ParseNode* peekStack(Stack* stack) {
    // Returns the top element of the stack if not empty, and NULL otherwise.
    if (!isStackEmpty(stack))
        return stack->top->data;
    else
        return NULL;
}

bool isStackEmpty(Stack* stack) { 
    return (stack->top == NULL);
}

/* Symbol List and Grammar Rule Functions */
SymbolNode* createSymbolNode(SymbolUnit* su) {
    // Creates a new node for the linked list of symbols
    SymbolNode* newNode = (SymbolNode*)malloc(sizeof(SymbolNode));
    if (!newNode) {
        printf("Could not allocate memory for symbol node\n");
        return NULL;
    }
    newNode->prev = NULL;
    newNode->next = NULL;
    newNode->symbol = su;
    return newNode;
}

SymbolList* createSymbolList() {
    // Creates a new symbol list for a rule's RHS
    SymbolList* newList = (SymbolList*)malloc(sizeof(SymbolList));
    if (!newList) {
        printf("Could not allocate memory for Symbol List\n");
        return NULL;
    }
    newList->count = 0;
    newList->head = NULL;
    newList->tail = NULL;
    return newList;
}

void insertSymbolNode(SymbolList* symList, SymbolNode* node) { 
    // Inserts a node into the symbol list
    if (symList->tail == NULL)
        symList->head = node;
    else {
        symList->tail->next = node;
        node->prev = symList->tail;
    }
    symList->tail = node;
    ++(symList->count);
}

/* Parse Tree Functions */
ParseNode* createParseNode() {
    // Creates a new node for the parse tree.
    ParseNode* newNode = (ParseNode*)malloc(sizeof(ParseNode));
    if (!newNode) {
        printf("Could not allocate memory for parse tree node\n");
        return NULL;
    }
    newNode->children = (ParseNode**)malloc(INIT_CHILD_CAPACITY * sizeof(ParseNode*));
    if (!(newNode->children)) {
        printf("Could not allocate memory for parse tree node's children\n");
        return newNode;
    }
    newNode->capacity = INIT_CHILD_CAPACITY;
    newNode->size = 0;
    newNode->symbol = NULL;
    newNode->ste = NULL;
    newNode->lineNumber = -1;
    for (int i = 0; i < INIT_CHILD_CAPACITY; i++) {
        newNode->children[i] = NULL;
    }
    return newNode;
}

ParseTree* createParseTree() {
    // Creates the parse tree structure.
    ParseTree* newTree = (ParseTree*)malloc(sizeof(ParseTree));
    if (!newTree) {
        printf("Could not allocate memory for creating parse tree\n");
        return NULL;
    }
    newTree->root = createParseNode();
    return newTree;
}

void insertChild(ParseNode* parent, ParseNode* child) {
    // Inserts 'child' as a child of 'parent'
    if (!parent) {
        printf("Parent node is NULL. Cannot add child.\n");
        return;
    }
    if (!child) {
        printf("Child node is NULL. Cannot add child.\n");
        return;
    }
    if (parent->size == parent->capacity) { // double capacity if needed
        parent->capacity *= 2;
        parent->children = (ParseNode**)realloc(parent->children, parent->capacity * sizeof(ParseNode*));
        if (!(parent->children)) {
            printf("Could not allocate memory for resizing children array\n");
            return;
        }
    }
    parent->children[(parent->size)++] = child;
}

/* Initialization of Non-Terminal Strings */
void initializeNonTerminalToString() {
    /*
        Initializes the nonTerminalToString array to fetch the string corresponding to a non-terminal.
    */
    if (nonTerminalsInitialized)
        return;
    nonTerminalsInitialized = true;
    // Allocate space for each string (assuming NON_TERM_LENGTH is defined elsewhere)
    for (int i = 0; i < NT_NOT_FOUND; i++) {
        nonTerminalToString[i] = malloc(NON_TERMINAL_COUNT);
    }
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

/* Utility Functions */
Token getTokenFromString(char* str) {
    // Given a string, returns the corresponding Token enum.
    for (int i = 0; i < TK_NOT_FOUND; i++) {
        if (!strcmp(tokenToString[i], str))
            return (Token)i;
    }
    return TK_NOT_FOUND;
}

NonTerminal getNonTerminalFromString(char* str) {
    // Given a string, returns the corresponding NonTerminal enum.
    for (int i = 0; i < NT_NOT_FOUND; i++) {
        if (!strcmp(nonTerminalToString[i], str))
            return (NonTerminal)i;
    }
    return NT_NOT_FOUND;
}

void readGrammar() {
    // Reads the grammar from the file grammar.txt and stores it in the Grammar array.
    if (grammarLoaded)
        return;
    grammarLoaded = true;
    char buff[MAX_RULE_LENGTH];
    FILE* fp = fopen("src/parser/grammar.txt", "r");
    if (!fp) {
        printf("Could not open grammar file\n");
        return;
    }
    char* oneTok;
    while (fgets(buff, MAX_RULE_LENGTH, fp)) {
        oneTok = strtok(buff, " \t\n\r");

        GrammarRule* gRule = (GrammarRule*)malloc(sizeof(GrammarRule));
        if (!gRule) {
            printf("Could not allocate memory for rule\n");
            return;
        }
        gRule->lhs = (SymbolUnit*)malloc(sizeof(SymbolUnit));
        if (!(gRule->lhs)) {
            printf("Could not allocate memory for grammar symbol\n");
            return;
        }
        gRule->lhs->isNonTerminal = true;
        gRule->lhs->value.nt = getNonTerminalFromString(oneTok);
        gRule->rhs = createSymbolList();

        oneTok = strtok(NULL, " \t\n\r");
        while (oneTok) {
            SymbolUnit* su = (SymbolUnit*)malloc(sizeof(SymbolUnit));
            if (!su) {
                printf("Could not allocate memory for RHS grammar symbol\n");
                break;
            }
            if (oneTok[0] == '<') {
                su->isNonTerminal = true;
                su->value.nt = getNonTerminalFromString(oneTok);
            } else {
                char tmp[TOKEN_NAME_LENGTH] = "TK_";
                strcat(tmp, oneTok);
                su->isNonTerminal = false;
                su->value.t = getTokenFromString(tmp);
            }
            SymbolNode* rhsNode = createSymbolNode(su);
            insertSymbolNode(gRule->rhs, rhsNode);
            oneTok = strtok(NULL, " \t\n\r");
        }
        Grammar[numOfRules++] = gRule;
    }
    fclose(fp);
}

void printGrammar() {
    FILE* fp = fopen("readgrm.txt", "w");
    for (int i = 0; i < numOfRules; i++) {
        GrammarRule* tmpRule = Grammar[i];
        fprintf(fp, "%d. %s: ", i + 1, nonTerminalToString[tmpRule->lhs->value.nt]);
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

/* First & Follow Set Functions */
void insertFirstFollowNode(FirstFollowSet* lst, FirstFollowNode* nd) {
    // Inserts a node into a first/follow set list.
    if (!(lst->tail)) {
        lst->head = nd;
        lst->tail = nd;
    } else {
        lst->tail->next = nd;
        lst->tail = nd;
    }
}

void printComputedFirstAndFollow() {
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

bool existsInFirstFollow(FirstFollowSet* lst, Token tmpkey) {
    // Checks if a token exists in the given first/follow set.
    if (!lst) return false;
    FirstFollowNode* current = lst->head;
    for (; current; current = current->next) {
        if (current->tk == tmpkey)
            return true;
    }
    return false;
}

FirstFollowSet* getFirstOfRhs(SymbolList* grhs) {
    // Returns a FirstFollowSet containing the FIRST of a grammar rule's RHS.
    FirstFollowSet* res = (FirstFollowSet*)malloc(sizeof(FirstFollowSet));
    res->head = NULL;
    res->tail = NULL;
    
    SymbolNode* ritr = grhs->head;
    bool hasEps = true;
    for (; ritr && hasEps; ritr = ritr->next) {
        FirstFollowNode* tmp;  
        hasEps = false;  
        if (!(ritr->symbol->isNonTerminal)) {
            if (!existsInFirstFollow(res, ritr->symbol->value.t)) {
                FirstFollowNode* vtmp = (FirstFollowNode*)malloc(sizeof(FirstFollowNode));
                vtmp->next = NULL;
                vtmp->tk = ritr->symbol->value.t;
                insertFirstFollowNode(res, vtmp);
            }
            break;
        }
        for (tmp = AutoFirst[ritr->symbol->value.nt]->head; tmp; tmp = tmp->next) {
            if (tmp->tk == EPS) {
                hasEps = true;
                continue;
            }
            if (!existsInFirstFollow(res, tmp->tk)) {
                FirstFollowNode* cpytmp = (FirstFollowNode*)malloc(sizeof(FirstFollowNode));
                cpytmp->next = NULL;
                cpytmp->tk = tmp->tk;
                insertFirstFollowNode(res, cpytmp);
            }
        }
    }
    if (ritr == NULL && hasEps) {
        FirstFollowNode* vvtmp = (FirstFollowNode*)malloc(sizeof(FirstFollowNode));
        vvtmp->next = NULL;
        vvtmp->tk = EPS;
        if (!existsInFirstFollow(res, EPS)) 
            insertFirstFollowNode(res, vvtmp);
    }
    return res;
}

void freeUpFirstFollow(FirstFollowSet* rl) {
    // Frees the memory allocated for a first/follow set list.
    if (!rl) return;
    FirstFollowNode *tmp, *tn;
    tmp = rl->head;
    while (tmp) {
        tn = tmp;
        tmp = tmp->next;
        free(tn);
    }
}

void addRulesToParseTable() {
    // Adds all grammar rules to the parse table.
    for (int gri = 0; gri < numOfRules; ++gri) {
        GrammarRule* tmpRule = Grammar[gri];
        FirstFollowSet* fOfRhs = getFirstOfRhs(tmpRule->rhs);
        FirstFollowNode* tItr;
        for (tItr = fOfRhs->head; tItr; tItr = tItr->next) {
            if (parseTable[tmpRule->lhs->value.nt][tItr->tk] != NULL)
                printf("\nMultiple defined entries in parse table detected! (Overwriting the rule!)\n");
            parseTable[tmpRule->lhs->value.nt][tItr->tk] = tmpRule;
        }
        if (existsInFirstFollow(fOfRhs, EPS)) {
            FirstFollowNode* antItr;
            for (antItr = AutoFollow[tmpRule->lhs->value.nt]->head; antItr; antItr = antItr->next) {
                if (parseTable[tmpRule->lhs->value.nt][antItr->tk] != NULL)
                    printf("\nMultiple defined entries in parse table detected! (Overwriting the rule!)\n");
                parseTable[tmpRule->lhs->value.nt][antItr->tk] = tmpRule;
            }
        }
        freeUpFirstFollow(fOfRhs);
    }
}

void initializeParseTable() {
    // Creates and initializes the parse table.
    if (parseTreeInitialized) return;
    parseTreeInitialized = true;
    parseTable = (GrammarRule***)malloc(NT_NOT_FOUND * sizeof(GrammarRule**));
    if (!parseTable) {
        printf("Couldn't allocate memory for parseTable rows\n");
        return;
    }
    for (int nti = 0; nti < NT_NOT_FOUND; nti++) {
        parseTable[nti] = (GrammarRule**)malloc(TK_NOT_FOUND * sizeof(GrammarRule*));
        if (!parseTable[nti]) {
            printf("Couldn't allocate memory for parseTable columns\n");
            return;
        }
        for (int tki = 0; tki < TK_NOT_FOUND; tki++) {
            parseTable[nti][tki] = NULL;
        }
    }
    addRulesToParseTable();
}

void printRule(GrammarRule* rule, FILE* fp) {
    fprintf(fp, "Lhs: %s\n", nonTerminalToString[rule->lhs->value.nt]);
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

void printParseTable() {
    FILE* fParseOut = fopen("parseTable.txt", "w");
    if (!fParseOut) {
        printf("Could not open file for printing parse table\n");
        return;
    }
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

/* Parsing Functions */
ParseTree* parseTokens(linkedList* tokensFromLexer, bool* hasSyntaxError) {
    /*
        Parses the token list using the parse table and builds the corresponding parse tree.
        Reports syntax errors if any.
    */
    if (!tokensFromLexer) {
        printf("Tokens list from lexer is NULL. Parsing failed\n");
        return NULL;
    }
    tokenInfo* inputPtr = tokensFromLexer->head;
    ParseTree* theParseTree = createParseTree();
    ParseNode* currentNode = theParseTree->root;
    SymbolUnit* su = (SymbolUnit*)malloc(sizeof(SymbolUnit));
    su->isNonTerminal = true;
    su->value.nt = program;
    currentNode->symbol = su;
    Stack* theStack = initializeStack();
    pushStack(theStack, currentNode);
    int cln = 1;

    if (shouldPrint)
        printf("Parsing starting...\n"), fflush(stdout);
    while (!isStackEmpty(theStack) && inputPtr) {
        cln = inputPtr->lineNumber;
        currentNode = peekStack(theStack);
        if (inputPtr->STE->tokenType == COMMENT || inputPtr->STE->tokenType >= LEXICAL_ERROR) {
            if (inputPtr->STE->tokenType == LEXICAL_ERROR) {
                if (shouldPrint)
                    printf("Line %*d \tError: Unrecognized pattern: \"%s\"\n", 5, inputPtr->lineNumber, inputPtr->STE->lexeme);
            }
            else if (inputPtr->STE->tokenType == ID_LENGTH_EXC) {
                if (shouldPrint)
                    printf("Line %*d \tError: Too long identifier: \"%s\"\n", 5, inputPtr->lineNumber, inputPtr->STE->lexeme);
            }
            else if (inputPtr->STE->tokenType == FUN_LENGTH_EXC) {
                if (shouldPrint)
                    printf("Line %*d \tError: Too long function name: \"%s\"\n", 5, inputPtr->lineNumber, inputPtr->STE->lexeme);
            }
            if (inputPtr->STE->tokenType != COMMENT)
                *hasSyntaxError = true;
            inputPtr = inputPtr->next;
            continue;
        }
        if (!(currentNode->symbol->isNonTerminal) && currentNode->symbol->value.t == EPS) {
            currentNode->lineNumber = inputPtr->lineNumber;
            SymbolTableEntry* tste = (SymbolTableEntry*)malloc(sizeof(SymbolTableEntry));
            strcpy(tste->lexeme, "EPSILON");
            tste->valueIfNumber = 0;
            currentNode->ste = tste;
            tste->tokenType = EPS;
            popStack(theStack);
            continue;
        }
        if (!(currentNode->symbol->isNonTerminal) && currentNode->symbol->value.t == inputPtr->STE->tokenType) {
            currentNode->lineNumber = inputPtr->lineNumber;
            currentNode->ste = inputPtr->STE;
            popStack(theStack);
            inputPtr = inputPtr->next;
        }
        else if (!(currentNode->symbol->isNonTerminal)) {
            *hasSyntaxError = true;
            if (shouldPrint)
                printf("Line %*d \tError: The token %s for lexeme \"%s\" does not match the expected token %s\n", 
                    5, inputPtr->lineNumber, tokenToString[inputPtr->STE->tokenType], 
                    inputPtr->STE->lexeme, tokenToString[currentNode->symbol->value.t]);
            currentNode->lineNumber = inputPtr->lineNumber;
            popStack(theStack);
        }
        else if (parseTable[currentNode->symbol->value.nt][inputPtr->STE->tokenType] == NULL) {
            *hasSyntaxError = true;
            if (shouldPrint)
                printf("Line %*d \tError: Invalid token %s encountered with value \"%s\". Stack top is: %s\n", 
                    5, inputPtr->lineNumber, tokenToString[inputPtr->STE->tokenType], 
                    inputPtr->STE->lexeme, nonTerminalToString[currentNode->symbol->value.nt]);
            if (existsInFirstFollow(AutoFollow[currentNode->symbol->value.nt], inputPtr->STE->tokenType)) {
                currentNode->lineNumber = inputPtr->lineNumber;
                popStack(theStack);
            } else {
                inputPtr = inputPtr->next;
                if (!inputPtr) {
                    popStack(theStack);
                    currentNode = peekStack(theStack);
                }
            }
        }
        else {
            GrammarRule* tmpRule = parseTable[currentNode->symbol->value.nt][inputPtr->STE->tokenType];
            popStack(theStack);
            currentNode->lineNumber = inputPtr->lineNumber;
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
    if (!(*hasSyntaxError) && isStackEmpty(theStack) && (!inputPtr || inputPtr->STE->tokenType == DOLLAR)) {
        if (shouldPrint)
            printf("\nParsing successful! No syntax errors! The input is syntactically correct!\n");
    } else {
        *hasSyntaxError = true;
        while (!isStackEmpty(theStack)) {
            currentNode = peekStack(theStack);
            if (currentNode->symbol->isNonTerminal) {
                if (shouldPrint)
                    printf("Line %*d \tError: Invalid token TK_DOLLAR encountered. Stack top is: %s\n", 
                        5, cln, nonTerminalToString[currentNode->symbol->value.nt]);
            } else {
                if (shouldPrint)
                    printf("Line %*d \tError: The token TK_DOLLAR for lexeme \"\" does not match the expected token %s\n", 
                        5, cln, tokenToString[currentNode->symbol->value.t]);
            }
            popStack(theStack);
        }
        while (inputPtr && inputPtr->STE->tokenType != DOLLAR) {
            if (shouldPrint)
                printf("Line %*d \tError: Invalid token %s encountered with value \"%s\". Stack top is: TK_DOLLAR\n", 
                    5, inputPtr->lineNumber, tokenToString[inputPtr->STE->tokenType], inputPtr->STE->lexeme);
            inputPtr = inputPtr->next;
        }
        if (shouldPrint)
            printf("\nThe input file has syntactic errors!\n");
    }
    return theParseTree;
}

void computeFirstSets() {
    // Computes the FIRST sets for all non-terminals.
    bool modified = true;
    while (modified) {
        modified = false;
        for (int gri = 0; gri < numOfRules; ++gri) {
            NonTerminal currLhs = Grammar[gri]->lhs->value.nt;
            SymbolNode* rhsItr = Grammar[gri]->rhs->head;
            bool hasEps = true;
            while (rhsItr && hasEps) {
                hasEps = false;
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

void computeFollowSets() {
    // Computes the FOLLOW sets for all non-terminals.
    FirstFollowNode* thdl = (FirstFollowNode*)malloc(sizeof(FirstFollowNode));
    thdl->next = NULL;
    thdl->tk = DOLLAR;
    insertFirstFollowNode(AutoFollow[program], thdl);

    bool modified = true;
    while (modified) {
        modified = false;
        for (int gri = 0; gri < numOfRules; ++gri) {
            NonTerminal currLhs = Grammar[gri]->lhs->value.nt;
            SymbolNode* rhsItr = Grammar[gri]->rhs->head;
            while (rhsItr) {
                if (!(rhsItr->symbol->isNonTerminal)) {
                    rhsItr = rhsItr->next;
                    continue;
                }
                // Create a temporary symbol list starting from the next node.
                SymbolList* tgl = (SymbolList*)malloc(sizeof(SymbolList));
                tgl->head = rhsItr->next;
                tgl->tail = NULL;
                tgl->count = 0;
                FirstFollowSet* fOfNxtT = getFirstOfRhs(tgl);
                FirstFollowNode* felItr;
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
                freeUpFirstFollow(fOfNxtT);
                free(tgl);
                rhsItr = rhsItr->next;
            }
        }
    }
}

void initializeAndComputeFirstAndFollow() {
    // Allocates data structures for FIRST and FOLLOW sets and computes them.
    if (firstFollowComputed)
        return;
    firstFollowComputed = true;
    AutoFirst = (FirstFollowSet**)malloc(NT_NOT_FOUND * sizeof(FirstFollowSet*));
    AutoFollow = (FirstFollowSet**)malloc(NT_NOT_FOUND * sizeof(FirstFollowSet*));
    if (!AutoFirst || !AutoFollow) {
        printf("Could not allocate memory for auto FIRST and FOLLOW sets array\n");
        return;
    }
    for (int i = 0; i < NT_NOT_FOUND; i++) {
        AutoFirst[i] = (FirstFollowSet*)malloc(sizeof(FirstFollowSet));
        if (!(AutoFirst[i])) {
            printf("Could not allocate memory for auto FIRST sets\n");
            return;
        }
        AutoFirst[i]->head = NULL;
        AutoFirst[i]->tail = NULL;
        AutoFollow[i] = (FirstFollowSet*)malloc(sizeof(FirstFollowSet));
        if (!(AutoFollow[i])) {
            printf("Could not allocate memory for auto FOLLOW sets\n");
            return;
        }
        AutoFollow[i]->head = NULL;
        AutoFollow[i]->tail = NULL;
    }
    computeFirstSets();
    computeFollowSets();
}

/* Parse Tree Printing Functions */
void printTreeNode(ParseNode* curr, ParseNode* par, FILE* fp) {
    // Prints details of a parse tree node in a formatted manner.
    fprintf(fp, "%*s ", 32, !(curr->symbol->isNonTerminal) ? curr->ste->lexeme : "-----");
    fprintf(fp, "%*d ", 12, curr->lineNumber);
    fprintf(fp, "%*s ", 16, curr->symbol->isNonTerminal ? "-----" : tokenToString[curr->ste->tokenType]);
    if (!(curr->symbol->isNonTerminal) && (curr->ste->tokenType == NUM || curr->ste->tokenType == RNUM)) {
        if (curr->ste->tokenType == NUM)
            fprintf(fp, "%*d ", 20, (int)(curr->ste->valueIfNumber));
        else    
            fprintf(fp, "%20.2lf ", curr->ste->valueIfNumber);
    } else {
        fprintf(fp, "%*s ", 20, "Not number ");
    }
    fprintf(fp, "%*s ", 30, par ? nonTerminalToString[par->symbol->value.nt] : "ROOT");
    fprintf(fp, "%*s ", 12, curr->symbol->isNonTerminal ? "NO" : "YES");
    fprintf(fp, "%*s ", 30, curr->symbol->isNonTerminal ? nonTerminalToString[curr->symbol->value.nt] : "-----");
    fprintf(fp, "\n");
}

void inorderTraverse(ParseNode* curr, ParseNode* par, FILE* fp) {
    // Performs an inorder traversal of the parse tree and prints each node.
    if (!curr)
        return;
    if (curr->size)
        inorderTraverse(curr->children[0], curr, fp);
    printTreeNode(curr, par, fp);
    for (int chi = 1; chi < curr->size; ++chi)
        inorderTraverse(curr->children[chi], curr, fp);
}

void printParseTree(ParseTree* PT, char* outFile) {
    FILE* fp = fopen(outFile, "w");
    if (!fp) {
        printf("Could not open file for printing parse tree\n");
        return;
    }
    if (!PT) {
        printf("Given parse tree is NULL. Cannot print\n");
        return;
    }
    if (shouldPrint)
        printf("Printing Parse Tree in the specified file...\n");
    fprintf(fp, "%*s %*s %*s %*s %*s %*s %*s\n\n", 32, "lexeme", 12, "lineNumber", 16, "tokenName", 20, "valueIfNumber", 30, "parentNodeSymbol", 12, "isLeafNode", 30, "nodeSymbol");
    inorderTraverse(PT->root, NULL, fp);
    fclose(fp);
    if (shouldPrint)
        printf("Printing parse tree completed...\n");
}

void parseInputSourceCode(char* inpFile, char* opFile) {
    /*
        Wrapper function for the parser. Initializes data structures, reads the grammar,
        computes FIRST/FOLLOW sets, builds the parse table, and parses the input source code.
    */
    FILE* ifp = fopen(inpFile, "r");
    if (!ifp) {
        printf("Could not open input file for parsing\n");
        return;
    }
    linkedList* tokensFromLexer = LexInput(ifp, opFile);
    fclose(ifp);
    
    initializeNonTerminalToString();
    readGrammar();
    initializeAndComputeFirstAndFollow();
    // printComputedFirstAndFollow();  // Uncomment if needed
    initializeParseTable();
    // printParseTable();  // Uncomment if needed

    bool hasSyntaxError = false;
    ParseTree* parseTree = parseTokens(tokensFromLexer, &hasSyntaxError);
    if (!hasSyntaxError)
        printParseTree(parseTree, opFile);
    else {
        FILE* foptp = fopen(opFile, "w");
        if (!foptp) {
            printf("Could not open file for printing parser output\n");
            return;
        }
        fprintf(foptp, "There were syntax errors in the input file. Not printing the parse tree!\nCheck the console for error details.");
        fclose(foptp);
    }
}

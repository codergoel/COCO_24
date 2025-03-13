// /*
//         Group No. 24
// 2022A7PS0170P : Tanmay Goel
// 2022A7PS0123P : Daksh Ahuja
// 2022A7PS0166P : Pratham Walia
// 2022A7PS0182P : Anant Malhotra
// 2022A7PS1733P : Pratham Prateek Mohanty
// 2022A7PS0073P : Richie Singh
// */

// Contains all definitions for data structures used in syntax analysis.

#ifndef PARSER_CORE_H
#define PARSER_CORE_H

#include "lexer.h"


#define NON_TERMINAL_COUNT 30
#define MAX_GRAMMAR_RULES 100
#define MAX_RULE_LENGTH 512
#define INIT_CHILD_CAPACITY 10

typedef enum NonTerminal{
    program,
    otherFunctions,
    mainFunction,
    stmts,
    stmt,
    function,
    input_par,
    output_par,
    parameter_list,
    dataType,
    remaining_list,
    primitiveDatatype,
    constructedDatatype,
    typeDefinitions,
    typeDefinition,
    declarations,
    declaration,
    otherStmts,
    returnStmt,
    definetypestmt,
    fieldDefinition,
    fieldDefinitions, 
    fieldType,
    moreFields,
    global_or_not,
    assignmentStmt,
    iterativeStmt,
    conditionalStmt,
    elsePart,
    ioStmt,
    funCallStmt,
    option_single_constructed,
    outputParameters,
    inputParameters,
    highPrecedenceOperators,
    lowPrecedenceOperators,
    oneExpansion,
    moreExpansions,
    expPrime,
    term,
    termPrime,
    factor,
    more_ids,
    A,
    idList,
    relationalOp,
    optionalReturn,
    var,
    logicalOp,
    arithmeticExpression,
    SingleOrRecId,
    booleanExpression,
    NT_NOT_FOUND,
    actualOrRedefined
} NonTerminal;

extern char* nonTerminalToString[NT_NOT_FOUND];

typedef struct SymbolUnit{
    bool isNonTerminal;
    union{
        NonTerminal nt;
        Token t;
    } value;
} SymbolUnit;

typedef struct SymbolNode {
    SymbolUnit* symbol;
    struct SymbolNode* prev;
    struct SymbolNode* next;
} SymbolNode;

typedef struct SymbolList{
    SymbolNode* head;
    SymbolNode* tail;
    int count;
} SymbolList;

typedef struct GrammarRule{
    SymbolUnit* lhs;
    SymbolList* rhs;
} GrammarRule;

extern GrammarRule* Grammar[MAX_GRAMMAR_RULES];
extern int numOfRules;

typedef struct FirstFollowNode{
    Token tk;
    struct FirstFollowNode* next;
} FirstFollowNode;

typedef struct FirstFollowSet{
    FirstFollowNode* head;
    FirstFollowNode* tail;
} FirstFollowSet;

extern FirstFollowSet** First;
extern FirstFollowSet** Follow;
extern FirstFollowSet** AutoFirst;
extern FirstFollowSet** AutoFollow;

extern GrammarRule*** parseTable;

extern bool grammarLoaded;
extern bool nonTerminalsInitialized;
extern bool firstFollowComputed;
extern bool parseTreeInitialized;

typedef struct ParseNode{
    SymbolUnit* symbol;
    SymbolTableEntry* ste;
    struct ParseNode** children;
    int capacity, size, lineNumber;
} ParseNode;

typedef struct ParseTree{
    ParseNode* root;
} ParseTree;

#endif

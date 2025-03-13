/*
   ====================================================================
   Lexical Analyzer - Definitions
   --------------------------------------------------------------------
   This header contains all the essential data types and constants
   used in the lexical analyzer implementation.
   ====================================================================
*/

#ifndef LEXER_DEFS_H
#define LEXER_DEFS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Constant Definitions */
#define ALPHABET_COUNT            26     // Number of lowercase letters (for trie)
#define INIT_SYMBOL_TABLE_CAP     10     // Initial capacity for the symbol table
#define BUFFER_SZ                 256    // Size of each half of the twin buffer
#define TOKEN_STR_LEN             50     // Maximum length for token string names

/* Token Enumeration - DO NOT change token names */
typedef enum Token {
    ASSIGNOP,
    COMMENT,
    FIELDID,
    ID,
    NUM,
    RNUM,
    FUNID,
    RUID,
    WITH,
    PARAMETERS,
    END,
    WHILE,
    UNION,
    ENDUNION,
    DEFINETYPE,
    AS,
    TYPE,
    MAIN,
    GLOBAL,
    PARAMETER,
    LIST,
    SQL,
    SQR,
    INPUT,
    OUTPUT,
    INT,
    REAL,
    COMMA,
    SEM,
    COLON,
    DOT,
    ENDWHILE,
    OP,
    CL,
    IF,
    THEN,
    ENDIF,
    READ,
    WRITE,
    RETURN,
    PLUS,
    MINUS,
    MUL,
    DIV,
    CALL,
    RECORD,
    ENDRECORD,
    ELSE,
    AND,
    OR,
    NOT,
    LT,
    LE,
    EQ,
    GT,
    GE,
    NE,
    EPS,
    DOLLAR,         // End-of-File marker
    LEXICAL_ERROR,
    ID_LENGTH_EXC,
    FUN_LENGTH_EXC,
    TK_NOT_FOUND
} Token;

/* Global variables for token-to-string mapping and debugging */
extern char* tokenToString[TK_NOT_FOUND];
extern bool tkStrInitialized;  // Flag to avoid reinitializing token strings
extern bool debugPrint;        // Flag to control debug/verbose output

/* ------------------ Trie Structures ------------------ */
// TrieNode: Represents a single node in the keyword trie.
typedef struct TrieNode {
    struct TrieNode* children[ALPHABET_COUNT]; // Pointers to child nodes
    int isEnd;         // Flag: non-zero if this node marks the end of a valid word
    Token tokenType;   // Associated token if node is end-of-word
} TrieNode;

// Trie: Wrapper structure that holds the root of the trie.
typedef struct Trie {
    TrieNode* root;
} Trie;

/* ---------------- Symbol Table Structures ---------------- */
// SymbolTableEntry: Holds details for a lexeme and its token.
typedef struct SymbolTableEntry {
    char lexeme[BUFFER_SZ];   // The lexeme string
    Token tokenType;          // Token type as defined in the enum
    double numericValue;      // Stores numeric value for numbers (if applicable)
} SymbolTableEntry;

// SymbolTable: A dynamic array of pointers to SymbolTableEntry.
typedef struct SymbolTable {
    int capacity;                    // Maximum number of entries allocated
    int size;                        // Current number of entries
    SymbolTableEntry** entries;      // Array of pointers to entries
} SymbolTable;

/* -------------- Token Linked List Structures -------------- */
// TokenNode: Node for storing token information in a linked list.
typedef struct TokenNode {
    SymbolTableEntry* entry;   // Pointer to the symbol table entry for the token
    int lineNum;               // Line number in the source code where token was found
    struct TokenNode* next;    // Pointer to the next token node
} TokenNode;

// TokenList: Linked list to maintain the order of tokens.
typedef struct TokenList {
    int count;           // Total number of tokens in the list
    TokenNode* head;     // Pointer to the first token node
    TokenNode* tail;     // Pointer to the last token node
} TokenList;

#endif

/*
   ====================================================================
   Lexical Analyzer - Function Prototypes
   --------------------------------------------------------------------
   This header declares all the functions used in the lexical analyzer.
   The functions are grouped by functionality for clarity.
   ====================================================================
*/

#ifndef LEXER_PROTOTYPES_H
#define LEXER_PROTOTYPES_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "lexerDef.h"

/* ----------- Trie Utility Functions ----------- */
// Create a new trie node.
TrieNode* newTrieNode();

// Initialize a new trie with a root node.
Trie* initTrie();

// Insert a keyword into the trie with its corresponding token.
void addKeyword(Trie* trie, const char* word, Token tkType);

// Search for a keyword in the trie; returns the token if found.
Token findKeyword(Trie* trie, const char* word);

/* --------- Symbol Table Utility Functions --------- */
// Create and initialize a new symbol table.
SymbolTable* newSymbolTable();

// Insert a symbol table entry into the symbol table.
void addToken(SymbolTable* table, SymbolTableEntry* entry);

// Create a new symbol table entry for a lexeme.
SymbolTableEntry* newSymbolTableEntry(char* lexeme, Token tkType, double numericVal);

// Search for a lexeme in the symbol table.
SymbolTableEntry* lookupToken(SymbolTable* table, char* lexeme);

/* ----------- Token List Functions ----------- */
// Create a new token list for storing tokens.
TokenList* createTokenList();

// Create a new token node containing the given symbol table entry and line number.
TokenNode* newTokenNode(SymbolTableEntry* entry, int lineNum);

// Append a token node to the token list.
void appendTokenNode(TokenList* list, TokenNode* node);

/* -------- Lexical Analysis Core & Utilities -------- */
// Wrapper function: reads input and returns a list of tokens.
TokenList* lexInput(FILE* fp, char* outputPath);

// Remove comments from the source file and optionally write to a clean file.
void removeComments(char* sourceFile, char* cleanFile);

// Print the contents of the clean file (post-comment removal).
void printCleanFile(const char* cleanFile);

// Generate the complete token list from the input file.
TokenList* getAllTokens(FILE* fp);

// Populate the trie with all reserved keywords.
void setupKeywordTrie(Trie* keywordTrie);

// Core DFA function: retrieves the next token from the input.
TokenNode* getNextToken(FILE* fp, char *buffer, int *forwardPtr, int *lineNumber, Trie* keywordTrie, SymbolTable* symTable);

// Extract the lexeme from the twin buffer between specified pointers.
void extractLexeme(int beginPtr, int* forwardPtr, char* lexeme, char* buffer);

// Fetch the next character from the twin buffer, handling buffer refills.
char fetchNextChar(FILE* fp, char *buffer, int *forwardPtr);

// Initialize the token-to-string mapping array.
void initTokenStrings();

// Print the token list on the console for debugging.
void displayTokenList(TokenList* list);

#endif

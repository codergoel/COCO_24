/*
   ====================================================================
   Lexical Analyzer Implementation
   --------------------------------------------------------------------
   This file implements the lexer using a twin-buffer strategy,
   a DFA for tokenizing input, and supporting data structures such as
   a trie for keyword lookup, a symbol table, and a token list.
   ====================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "lexer.h"
#include "lexerDef.h"

/* Global variables for token-string mapping and flags */
bool retractFlag = false;  
char* tokenToString[TK_NOT_FOUND];         
bool tkStrInitialized = false;     
bool debugPrint = false;

// ========================= SECTION 1: DATA STRUCTURE IMPLEMENTATIONS =========================

// ------------------------- TRIE IMPLEMENTATION (FOR KEYWORDS) -------------------------

TrieNode* newTrieNode() {
    /*
       Creates and initializes a new trie node with all children set to NULL.
       Uses calloc for zero initialization of the node memory.
    */
    // Use calloc for automatic zero initialization
    TrieNode* node = (TrieNode*) calloc(1, sizeof(TrieNode));
    if (!node) {
        fprintf(stderr, "Memory allocation failed for TrieNode\n");
        exit(EXIT_FAILURE);
    }
    // No need to manually initialize since calloc already sets everything to 0
    return node;
}

Trie* initTrie() {
    /*
       Initializes a trie data structure with a root node.
       The trie is used to store keywords for efficient lookup.
    */
    Trie* trie = (Trie*) malloc(sizeof(Trie));
    if (!trie) {
        fprintf(stderr, "Memory allocation failed for Trie\n");
        exit(EXIT_FAILURE);
    }
    
    // Create the root node
    trie->root = newTrieNode();
    return trie;
}

void addKeyword(Trie* trie, const char* word, Token tkType) {
    /*
       Adds a keyword to the trie with its associated token type.
       Each character in the word corresponds to a node in the trie.
    */
    // Validation
    if (!trie || !trie->root || !word) {
        fprintf(stderr, "Invalid arguments to addKeyword\n");
        return;
    }

    TrieNode* current = trie->root;
    
    // Traverse the trie, creating nodes as needed
    while (*word) {
        int index = *word - 'a';
        
        // Check for invalid characters (only lowercase letters expected)
        if (index < 0 || index >= ALPHABET_COUNT) {
            fprintf(stderr, "Invalid character in keyword: %c\n", *word);
            return;
        }
        
        // Create new node if needed
        if (!current->children[index]) {
            current->children[index] = newTrieNode();
        }
        
        // Move to the next node
        current = current->children[index];
        word++;
    }
    
    // Mark end of word and set token type
    current->isEnd = 1;
    current->tokenType = tkType;
}

Token findKeyword(Trie* trie, const char* word) {
    /*
       Looks up a word in the keyword trie and returns its token type if found.
       Returns TK_NOT_FOUND if the word is not in the trie.
    */
    // Validation
    if (!trie || !trie->root || !word) {
        return TK_NOT_FOUND;
    }

    TrieNode* current = trie->root;
    
    // Traverse the trie according to the characters in the word
    while (*word) {
        int index = *word - 'a';
        
        // Check for invalid characters or missing node
        if (index < 0 || index >= ALPHABET_COUNT || !current->children[index]) {
            return TK_NOT_FOUND;
        }
        
        current = current->children[index];
        word++;
    }
    
    // Return the token type if this is a valid end of word, otherwise not found
    return (current && current->isEnd) ? current->tokenType : TK_NOT_FOUND;
}

// ------------------------- SYMBOL TABLE IMPLEMENTATION -------------------------

SymbolTable* newSymbolTable() {
    /*
       Creates and initializes a new symbol table with initial capacity.
       The symbol table stores all tokens found during lexical analysis.
    */
    // Allocate the symbol table structure
    SymbolTable* table = (SymbolTable*) malloc(sizeof(SymbolTable));
    if (!table) {
        fprintf(stderr, "Memory allocation failed for Symbol Table\n");
        return NULL;
    }
    
    // Allocate the entries array
    table->entries = (SymbolTableEntry**) calloc(INIT_SYMBOL_TABLE_CAP, sizeof(SymbolTableEntry*));
    if (!table->entries) {
        fprintf(stderr, "Memory allocation failed for Symbol Table entries\n");
        free(table);  // Clean up the previously allocated memory
        return NULL;
    }
    
    // Initialize the fields
    table->capacity = INIT_SYMBOL_TABLE_CAP;
    table->size = 0;
    
    return table;
}

void addToken(SymbolTable* table, SymbolTableEntry* entry) {
    /*
       Adds a token entry to the symbol table, growing the table if needed.
       Handles dynamic resizing of the table when capacity is reached.
    */
    // Validate parameters
    if (!table || !entry) {
        fprintf(stderr, "Invalid parameters to addToken\n");
        return;
    }
    
    // Check if we need to grow the table
    if (table->size >= table->capacity) {
        size_t newCapacity = table->capacity * 2;
        SymbolTableEntry** newEntries = (SymbolTableEntry**) realloc(
            table->entries, 
            newCapacity * sizeof(SymbolTableEntry*)
        );
        
        if (!newEntries) {
            fprintf(stderr, "Failed to resize Symbol Table (current size: %d)\n", table->size);
            return;
        }
        
        table->entries = newEntries;
        table->capacity = newCapacity;
    }
    
    // Add the new entry and increment the size
    table->entries[table->size] = entry;
    table->size++;
}

SymbolTableEntry* newSymbolTableEntry(char* lexeme, Token tkType, double numVal) {
    /*
       Creates a new symbol table entry with the provided token information.
       Includes storage for the lexeme string, token type, and numeric value.
    */
    // Validate parameter
    if (!lexeme) {
        fprintf(stderr, "NULL lexeme passed to newSymbolTableEntry\n");
        return NULL;
    }
    
    // Allocate the entry
    SymbolTableEntry* entry = (SymbolTableEntry*) malloc(sizeof(SymbolTableEntry));
    if (!entry) {
        fprintf(stderr, "Memory allocation failed for Symbol Table Entry\n");
        return NULL;
    }
    
    // Initialize the fields with safe string copy
    strncpy(entry->lexeme, lexeme, BUFFER_SZ - 1);
    entry->lexeme[BUFFER_SZ - 1] = '\0';  // Ensure null termination
    
    entry->tokenType = tkType;
    entry->numericValue = numVal;
    
    return entry;
}

SymbolTableEntry* lookupToken(SymbolTable* table, char* lexeme) {
    /*
       Searches for a lexeme in the symbol table.
       Returns the entry if found, NULL otherwise.
    */
    // Validate parameters
    if (!table || !lexeme) {
        return NULL;
    }
    
    // Sequential search through the entries
    for (int i = 0; i < table->size; i++) {
        if (table->entries[i] && strcmp(lexeme, table->entries[i]->lexeme) == 0) {
            return table->entries[i];
        }
    }
    
    // Not found
    return NULL;
}

// ------------------------- TOKEN LIST IMPLEMENTATION -------------------------

TokenList* createTokenList() {
    /*
       Creates and initializes a new empty token list structure.
       The token list stores tokens in the order they appear in the input.
    */
    // Allocate memory for the list structure
    TokenList* list = (TokenList*) malloc(sizeof(TokenList));
    if (!list) {
        fprintf(stderr, "Memory allocation failed for TokenList\n");
        return NULL;
    }
    
    // Initialize all fields
    list->count = 0;
    list->head = NULL;
    list->tail = NULL;
    
    return list;
}

TokenNode* newTokenNode(SymbolTableEntry* entry, int lineNum) {
    /*
       Creates a new token node containing the given entry and line number.
       Each node represents one token in the input.
    */
    // Validate parameter
    if (!entry) {
        fprintf(stderr, "NULL entry passed to newTokenNode\n");
        return NULL;
    }
    
    // Allocate memory for the node
    TokenNode* node = (TokenNode*) malloc(sizeof(TokenNode));
    if (!node) {
        fprintf(stderr, "Memory allocation failed for TokenNode\n");
        return NULL;
    }
    
    // Initialize node fields
    node->entry = entry;
    node->lineNum = lineNum;
    node->next = NULL;
    
    return node;
}

void appendTokenNode(TokenList* list, TokenNode* node) {
    /*
       Adds a token node to the end of the token list.
       Maintains both head and tail pointers for efficient operations.
    */
    // Validate parameters
    if (!list || !node) {
        fprintf(stderr, "NULL parameter passed to appendTokenNode\n");
        return;
    }
    
    // Handle the first node case
    if (list->count == 0) {
        list->head = node;
        list->tail = node;
    }
    // Normal append operation
    else {
        list->tail->next = node;
        list->tail = node;
    }
    
    // Ensure the node is properly terminated
    node->next = NULL;
    
    // Update count
    list->count++;
}

// ========================= SECTION 2: BUFFER HANDLING =========================

// ------------------------- TWIN BUFFER MANAGEMENT -------------------------

char fetchNextChar(FILE* fp, char *buffer, int *forwardPtr) {
    /*
       Retrieves the next character from the twin buffer and handles buffer reload.
       
       The twin buffer has two segments:
       - First half: buffer[0] to buffer[BUFFER_SZ-1]
       - Second half: buffer[BUFFER_SZ] to buffer[2*BUFFER_SZ-1]
       
       When the forward pointer reaches the end of one segment, we reload the 
       other segment from the input file.
    */
    // Check if we're at the end of first segment and need to reload second segment
    if (*forwardPtr == BUFFER_SZ - 1 && !retractFlag) {
        if (feof(fp)) {
            // End of file reached, mark the second segment with a null terminator
            buffer[BUFFER_SZ] = '\0';
        } else {
            // Load the next chunk into the second segment
            int bytesRead = fread(buffer + BUFFER_SZ, sizeof(char), BUFFER_SZ, fp);
            
            // Set null terminator if we read less than the full buffer size
            if (bytesRead < BUFFER_SZ) {
                buffer[BUFFER_SZ + bytesRead] = '\0';
            }
        }
    } 
    // Check if we're at the end of second segment and need to reload first segment
    else if (*forwardPtr == (2 * BUFFER_SZ - 1) && !retractFlag) {
        if (feof(fp)) {
            // End of file reached, mark the first segment with a null terminator
            buffer[0] = '\0';
        } else {
            // Load the next chunk into the first segment
            int bytesRead = fread(buffer, sizeof(char), BUFFER_SZ, fp);
            
            // Set null terminator if we read less than the full buffer size
            if (bytesRead < BUFFER_SZ) {
                buffer[bytesRead] = '\0';
            }
        }
    }
    
    // Reset retract flag if it was set
    if (retractFlag) {
        retractFlag = false;
    }
    
    // Advance the forward pointer with wraparound
    *forwardPtr = (*forwardPtr + 1) % (2 * BUFFER_SZ);
    
    // Return the character at the new position
    return buffer[*forwardPtr];
}

void extractLexeme(int beginPtr, int* forwardPtr, char* lexeme, char* buffer) {
    /*
       Extracts characters between beginPtr and forwardPtr from the twin buffer into lexeme.
       Handles two cases:
       1. When the lexeme is contained within a single continuous region
       2. When the lexeme wraps around the end of the buffer
    */
    // Ensure destination buffer is valid
    if (!lexeme) {
        fprintf(stderr, "NULL lexeme buffer passed to extractLexeme\n");
        return;
    }
    
    // Case 1: Lexeme is in a continuous segment (no wrap-around)
    if (*forwardPtr >= beginPtr) {
        int lexemeLength = (*forwardPtr - beginPtr) + 1;
        
        // Safety check to prevent buffer overrun
        if (lexemeLength >= BUFFER_SZ) {
            fprintf(stderr, "Warning: Lexeme length exceeds buffer size\n");
            lexemeLength = BUFFER_SZ - 1;
        }
        
        // Copy characters from beginPtr to forwardPtr
        for (int i = 0; i < lexemeLength; i++) {
            lexeme[i] = buffer[beginPtr + i];
        }
        
        // Null-terminate the lexeme
        lexeme[lexemeLength] = '\0';
    }
    // Case 2: Lexeme wraps around the buffer
    else {
        int lexemeLength = (*forwardPtr + 2 * BUFFER_SZ - beginPtr) + 1;
        
        // Safety check to prevent buffer overrun
        if (lexemeLength >= BUFFER_SZ) {
            fprintf(stderr, "Warning: Wrapped lexeme length exceeds buffer size\n");
            lexemeLength = BUFFER_SZ - 1;
        }
        
        // Copy characters with wraparound
        for (int i = 0; i < lexemeLength; i++) {
            lexeme[i] = buffer[(beginPtr + i) % (2 * BUFFER_SZ)];
        }
        
        // Null-terminate the lexeme
        lexeme[lexemeLength] = '\0';
    }
}

int getLexemeLength(int begPtr, int forwardPtr) {
    /*
       Calculates the length of a lexeme between begPtr and forwardPtr in the twin buffer.
       Handles both continuous and wrap-around cases.
    */
    // Case 1: forwardPtr is ahead of or equal to begPtr (continuous segment)
    if (forwardPtr >= begPtr) {
        return (forwardPtr - begPtr) + 1;
    }
    // Case 2: forwardPtr has wrapped around to the beginning of the buffer
    else {
        // Calculate length considering the wrap-around
        return (2 * BUFFER_SZ - begPtr) + forwardPtr + 1;
    }
}

// ========================= SECTION 3: LEXICAL ANALYZER CORE =========================

// ------------------------- DFA FOR TOKEN RECOGNITION -------------------------

TokenNode* getNextToken(FILE* fp, char *buffer, int *forwardPtr, int *lineNum, 
                          Trie* keywordTrie, SymbolTable* symTable) {
    /*
       Implements the DFA for tokenizing the input.
       Reads characters from the twin buffer, transitions through states,
       and returns a TokenNode when a valid token is recognized.
    */
    TokenNode* tokenNode;
    int beginPtr = (*forwardPtr + 1) % (2 * BUFFER_SZ);
    int state = 0;
    char ch;
    char lexeme[BUFFER_SZ];

    while (true) {
        switch (state) {
            // ------------------------- INITIAL STATE -------------------------
            case 0:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                // Whitespace handling
                if (ch == '\n') {
                    ++(*lineNum);
                    beginPtr = (beginPtr + 1) % (2 * BUFFER_SZ);
                } else if (ch == ' ' || ch == '\t' || ch == '\r') {
                    beginPtr = (beginPtr + 1) % (2 * BUFFER_SZ);
                } 
                // Numbers
                else if (isdigit(ch)) {
                    state = 17;
                } 
                // Comments
                else if (ch == '%') {
                    state = 8;
                } 
                // Operators
                else if (ch == '>') {
                    state = 59;
                } else if (ch == '<') {
                    state = 1;
                } else if (ch == '=') {
                    state = 48;
                } else if (ch == '_') {
                    state = 29;
                } else if (ch == '/') {
                    state = 47;
                } else if (ch == '@') {
                    state = 53;
                } else if (ch == '*') {
                    state = 46;
                } else if (ch == '#') {
                    state = 33;
                } else if (ch == '[') {
                    state = 36;
                } else if (ch == ']') {
                    state = 37;
                } else if (ch == ',') {
                    state = 38;
                } else if (ch == ':') {
                    state = 40;
                } else if (ch == ';') {
                    state = 39;
                } else if (ch == '+') {
                    state = 44;
                } else if (ch == ')') {
                    state = 42;
                } else if (ch == '-') {
                    state = 45;
                } else if (ch == '(') {
                    state = 41;
                } else if (ch == '&') {
                    state = 50;
                } else if (ch == '~') {
                    state = 58;
                } else if (ch == '!') {
                    state = 56;
                } 
                // Identifiers and keywords
                else if (ch == 'a' || (ch > 'd' && ch <= 'z')) {
                    state = 15;
                } else if (ch >= 'b' && ch <= 'd') {
                    state = 10;
                } else if (ch == '.') {
                    state = 43;
                } 
                // End of file
                else if (ch == '\0') {
                    lexeme[0] = ch;
                    SymbolTableEntry* eofEntry = newSymbolTableEntry(lexeme, DOLLAR, 0);
                    tokenNode = newTokenNode(eofEntry, *lineNum);
                    return tokenNode;
                } 
                // Unrecognized character - lexical error
                else {
                    lexeme[0] = ch;
                    lexeme[1] = '\0';
                    SymbolTableEntry* errEntry = lookupToken(symTable, lexeme);
                    if (!errEntry) {
                        errEntry = newSymbolTableEntry(lexeme, LEXICAL_ERROR, 0);
                        addToken(symTable, errEntry);
                    }
                    tokenNode = newTokenNode(errEntry, *lineNum);
                    return tokenNode;
                }
                break;

            // ------------------------- SYMBOL & OPERATOR STATES -------------------------
            
            // Assignment operator <---
            case 1: // Starting with <
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '-') 
                    state = 2;
                else if (ch == '=')
                    state = 6;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* ltEntry = lookupToken(symTable, lexeme);
                    if (!ltEntry) {
                        ltEntry = newSymbolTableEntry(lexeme, LT, 0);
                        addToken(symTable, ltEntry);
                    }
                    tokenNode = newTokenNode(ltEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 2: // Found <-
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '-') 
                    state = 3;
                else {
                    *forwardPtr -= 2;
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == BUFFER_SZ - 2 ||
                        *forwardPtr == 2 * BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 2)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* ltEntry = lookupToken(symTable, lexeme);
                    if (!ltEntry) {
                        ltEntry = newSymbolTableEntry(lexeme, LT, 0);
                        addToken(symTable, ltEntry);
                    }
                    tokenNode = newTokenNode(ltEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 3: // Found <--
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '-') 
                    state = 4;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* errEntry = lookupToken(symTable, lexeme);
                    if (!errEntry) {
                        errEntry = newSymbolTableEntry(lexeme, LEXICAL_ERROR, 0);
                        addToken(symTable, errEntry);
                    }
                    tokenNode = newTokenNode(errEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 4: // Assignment operator <--- complete
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* asgnEntry = lookupToken(symTable, lexeme);
                    if (!asgnEntry) {
                        asgnEntry = newSymbolTableEntry(lexeme, ASSIGNOP, 0);
                        addToken(symTable, asgnEntry);
                    }
                    tokenNode = newTokenNode(asgnEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 6: // Less than or equal <=
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* leEntry = lookupToken(symTable, lexeme);
                    if (!leEntry) {
                        leEntry = newSymbolTableEntry(lexeme, LE, 0);
                        addToken(symTable, leEntry);
                    }
                    tokenNode = newTokenNode(leEntry, *lineNum);
                    return tokenNode;
                }
                break;
                
            case 43: // Dot '.'
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* dotEntry = lookupToken(symTable, lexeme);
                    if (!dotEntry) {
                        dotEntry = newSymbolTableEntry(lexeme, DOT, 0);
                        addToken(symTable, dotEntry);
                    }
                    tokenNode = newTokenNode(dotEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 48: // Equal sign handling
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '=')
                    state = 49;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* errEntry = lookupToken(symTable, lexeme);
                    if (!errEntry) {
                        errEntry = newSymbolTableEntry(lexeme, LEXICAL_ERROR, 0);
                        addToken(symTable, errEntry);
                    }
                    tokenNode = newTokenNode(errEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 49: // Equal operator ==
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* eqEntry = lookupToken(symTable, lexeme);
                    if (!eqEntry) {
                        eqEntry = newSymbolTableEntry(lexeme, EQ, 0);
                        addToken(symTable, eqEntry);
                    }
                    tokenNode = newTokenNode(eqEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 59: // Greater than handling
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '=') 
                    state = 61;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* gtEntry = lookupToken(symTable, lexeme);
                    if (!gtEntry) {
                        gtEntry = newSymbolTableEntry(lexeme, GT, 0);
                        addToken(symTable, gtEntry);
                    }
                    tokenNode = newTokenNode(gtEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 61: // Greater than or equal >=
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* geEntry = lookupToken(symTable, lexeme);
                    if (!geEntry) {
                        geEntry = newSymbolTableEntry(lexeme, GE, 0);
                        addToken(symTable, geEntry);
                    }
                    tokenNode = newTokenNode(geEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 57: // Not equal !=
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* neEntry = lookupToken(symTable, lexeme);
                    if (!neEntry) {
                        neEntry = newSymbolTableEntry(lexeme, NE, 0);
                        addToken(symTable, neEntry);
                    }
                    tokenNode = newTokenNode(neEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 56: // Not equal operator handling
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '=')
                    state = 57;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* errEntry = lookupToken(symTable, lexeme);
                    if (!errEntry) {
                        errEntry = newSymbolTableEntry(lexeme, LEXICAL_ERROR, 0);
                        addToken(symTable, errEntry);
                    }
                    tokenNode = newTokenNode(errEntry, *lineNum);
                    return tokenNode;
                }
                break;
                
            // Basic operators and symbols
            case 36: // Left square bracket [
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* sqlEntry = lookupToken(symTable, lexeme);
                    if (!sqlEntry) {
                        sqlEntry = newSymbolTableEntry(lexeme, SQL, 0);
                        addToken(symTable, sqlEntry);
                    }
                    tokenNode = newTokenNode(sqlEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 37: // Right square bracket ]
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* sqrEntry = lookupToken(symTable, lexeme);
                    if (!sqrEntry) {
                        sqrEntry = newSymbolTableEntry(lexeme, SQR, 0);
                        addToken(symTable, sqrEntry);
                    }
                    tokenNode = newTokenNode(sqrEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 38: // Comma ,
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* commaEntry = lookupToken(symTable, lexeme);
                    if (!commaEntry) {
                        commaEntry = newSymbolTableEntry(lexeme, COMMA, 0);
                        addToken(symTable, commaEntry);
                    }
                    tokenNode = newTokenNode(commaEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 39: // Semicolon ;
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* semEntry = lookupToken(symTable, lexeme);
                    if (!semEntry) {
                        semEntry = newSymbolTableEntry(lexeme, SEM, 0);
                        addToken(symTable, semEntry);
                    }
                    tokenNode = newTokenNode(semEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 40: // Colon :
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* colonEntry = lookupToken(symTable, lexeme);
                    if (!colonEntry) {
                        colonEntry = newSymbolTableEntry(lexeme, COLON, 0);
                        addToken(symTable, colonEntry);
                    }
                    tokenNode = newTokenNode(colonEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 41: // Left parenthesis (
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* opEntry = lookupToken(symTable, lexeme);
                    if (!opEntry) {
                        opEntry = newSymbolTableEntry(lexeme, OP, 0);
                        addToken(symTable, opEntry);
                    }
                    tokenNode = newTokenNode(opEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 42: // Right parenthesis )
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* clEntry = lookupToken(symTable, lexeme);
                    if (!clEntry) {
                        clEntry = newSymbolTableEntry(lexeme, CL, 0);
                        addToken(symTable, clEntry);
                    }
                    tokenNode = newTokenNode(clEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 44: // Plus +
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* plusEntry = lookupToken(symTable, lexeme);
                    if (!plusEntry) {
                        plusEntry = newSymbolTableEntry(lexeme, PLUS, 0);
                        addToken(symTable, plusEntry);
                    }
                    tokenNode = newTokenNode(plusEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 45: // Minus -
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* minusEntry = lookupToken(symTable, lexeme);
                    if (!minusEntry) {
                        minusEntry = newSymbolTableEntry(lexeme, MINUS, 0);
                        addToken(symTable, minusEntry);
                    }
                    tokenNode = newTokenNode(minusEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 46: // Multiplication *
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* mulEntry = lookupToken(symTable, lexeme);
                    if (!mulEntry) {
                        mulEntry = newSymbolTableEntry(lexeme, MUL, 0);
                        addToken(symTable, mulEntry);
                    }
                    tokenNode = newTokenNode(mulEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 47: // Division /
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* divEntry = lookupToken(symTable, lexeme);
                    if (!divEntry) {
                        divEntry = newSymbolTableEntry(lexeme, DIV, 0);
                        addToken(symTable, divEntry);
                    }
                    tokenNode = newTokenNode(divEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 50: // Logical AND &&
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '&')
                    state = 51;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* errEntry = lookupToken(symTable, lexeme);
                    if (!errEntry) {
                        errEntry = newSymbolTableEntry(lexeme, LEXICAL_ERROR, 0);
                        addToken(symTable, errEntry);
                    }
                    tokenNode = newTokenNode(errEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 51: // Logical AND &&
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '&')
                    state = 52;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* errEntry = lookupToken(symTable, lexeme);
                    if (!errEntry) {
                        errEntry = newSymbolTableEntry(lexeme, LEXICAL_ERROR, 0);
                        addToken(symTable, errEntry);
                    }
                    tokenNode = newTokenNode(errEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 52: // Logical AND &&
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* andEntry = lookupToken(symTable, lexeme);
                    if (!andEntry) {
                        andEntry = newSymbolTableEntry(lexeme, AND, 0);
                        addToken(symTable, andEntry);
                    }
                    tokenNode = newTokenNode(andEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 53: // Logical OR ||
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '@')
                    state = 54;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* errEntry = lookupToken(symTable, lexeme);
                    if (!errEntry) {
                        errEntry = newSymbolTableEntry(lexeme, LEXICAL_ERROR, 0);
                        addToken(symTable, errEntry);
                    }
                    tokenNode = newTokenNode(errEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 54: // Logical OR ||
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '@')
                    state = 55;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* errEntry = lookupToken(symTable, lexeme);
                    if (!errEntry) {
                        errEntry = newSymbolTableEntry(lexeme, LEXICAL_ERROR, 0);
                        addToken(symTable, errEntry);
                    }
                    tokenNode = newTokenNode(errEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 55: // Logical OR ||
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* orEntry = lookupToken(symTable, lexeme);
                    if (!orEntry) {
                        orEntry = newSymbolTableEntry(lexeme, OR, 0);
                        addToken(symTable, orEntry);
                    }
                    tokenNode = newTokenNode(orEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 58: // Logical NOT ~
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* notEntry = lookupToken(symTable, lexeme);
                    if (!notEntry) {
                        notEntry = newSymbolTableEntry(lexeme, NOT, 0);
                        addToken(symTable, notEntry);
                    }
                    tokenNode = newTokenNode(notEntry, *lineNum);
                    return tokenNode;
                }
                break;

            // ------------------------- IDENTIFIER & KEYWORD STATES -------------------------
            
            case 10: // Identifier starting with b, c, or d
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (islower(ch))
                    state = 15;
                else if (ch >= '2' && ch <= '7')
                    state = 11;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* commonEntry = lookupToken(symTable, lexeme);
                    if (commonEntry) {
                        tokenNode = newTokenNode(commonEntry, *lineNum);
                        return tokenNode;
                    }
                    Token tk = findKeyword(keywordTrie, lexeme);
                    if (tk == TK_NOT_FOUND) {
                        SymbolTableEntry* fldEntry = newSymbolTableEntry(lexeme, FIELDID, 0);
                        addToken(symTable, fldEntry);
                        tokenNode = newTokenNode(fldEntry, *lineNum);
                        return tokenNode;
                    } else {
                        SymbolTableEntry* keyEntry = newSymbolTableEntry(lexeme, tk, 0);
                        addToken(symTable, keyEntry);
                        tokenNode = newTokenNode(keyEntry, *lineNum);
                        return tokenNode;
                    }
                }
                break;

            case 11: // Identifier with digits
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch >= '2' && ch <= '7')
                    state = 12;
                else if (ch < 'b' || ch > 'd') {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* commonEntry = lookupToken(symTable, lexeme);
                    if (commonEntry) {
                        tokenNode = newTokenNode(commonEntry, *lineNum);
                        return tokenNode;
                    }
                    SymbolTableEntry* idEntry = newSymbolTableEntry(lexeme, ID, 0);
                    addToken(symTable, idEntry);
                    tokenNode = newTokenNode(idEntry, *lineNum);
                    return tokenNode;
                }
                if (getLexemeLength(beginPtr, *forwardPtr) > 20) {
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    lexeme[20] = '.'; lexeme[21] = '.'; lexeme[22] = '.'; lexeme[23] = '\0';
                    SymbolTableEntry* idLenEntry = lookupToken(symTable, lexeme);
                    if (!idLenEntry) {
                        idLenEntry = newSymbolTableEntry(lexeme, ID_LENGTH_EXC, 0);
                        addToken(symTable, idLenEntry);
                    }
                    tokenNode = newTokenNode(idLenEntry, *lineNum);
                    ch = fetchNextChar(fp, buffer, forwardPtr);
                    for (; ch >= 'b' && ch <= 'd'; ch = fetchNextChar(fp, buffer, forwardPtr));
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    if (ch >= '2' && ch <= '7') {
                        state = 12;
                        break;
                    }
                    return tokenNode;
                }
                break;

            case 12: // Identifier with digits
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch < '2' || ch > '7') {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* idEntry = lookupToken(symTable, lexeme);
                    if (!idEntry) {
                        idEntry = newSymbolTableEntry(lexeme, ID, 0);
                        addToken(symTable, idEntry);
                    }
                    tokenNode = newTokenNode(idEntry, *lineNum);
                    return tokenNode;
                }
                if (getLexemeLength(beginPtr, *forwardPtr) > 20) {
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    lexeme[20] = '.'; lexeme[21] = '.'; lexeme[22] = '.'; lexeme[23] = '\0';
                    SymbolTableEntry* idLenEntry = lookupToken(symTable, lexeme);
                    if (!idLenEntry) {
                        idLenEntry = newSymbolTableEntry(lexeme, ID_LENGTH_EXC, 0);
                        addToken(symTable, idLenEntry);
                    }
                    tokenNode = newTokenNode(idLenEntry, *lineNum);
                    ch = fetchNextChar(fp, buffer, forwardPtr);
                    for (; ch >= 'b' && ch <= 'd'; ch = fetchNextChar(fp, buffer, forwardPtr));
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    if (ch >= '2' && ch <= '7') {
                        state = 4;
                        break;
                    }
                    return tokenNode;
                }
                break;

            case 15: // Identifier or keyword
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (!islower(ch)) {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* entry = lookupToken(symTable, lexeme);
                    if (!entry) {
                        Token tk = findKeyword(keywordTrie, lexeme);
                        if (tk == TK_NOT_FOUND) {
                            SymbolTableEntry* fldEntry = newSymbolTableEntry(lexeme, FIELDID, 0);
                            addToken(symTable, fldEntry);
                            tokenNode = newTokenNode(fldEntry, *lineNum);
                            return tokenNode;
                        } else {
                            SymbolTableEntry* keyEntry = newSymbolTableEntry(lexeme, tk, 0);
                            addToken(symTable, keyEntry);
                            tokenNode = newTokenNode(keyEntry, *lineNum);
                            return tokenNode;
                        }
                    } else {
                        tokenNode = newTokenNode(entry, *lineNum);
                        return tokenNode;
                    }
                }
                break;

            case 29: // Function identifier starting with _
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == 'm')
                    state = 100;
                else if (isalpha(ch))
                    state = 30;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* errEntry = lookupToken(symTable, lexeme);
                    if (!errEntry) {
                        errEntry = newSymbolTableEntry(lexeme, LEXICAL_ERROR, 0);
                        addToken(symTable, errEntry);
                    }
                    tokenNode = newTokenNode(errEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 30: // Function identifier
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (isdigit(ch))
                    state = 31;
                else if (!isalpha(ch)) {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* commonEntry = lookupToken(symTable, lexeme);
                    if (commonEntry) {
                        tokenNode = newTokenNode(commonEntry, *lineNum);
                        return tokenNode;
                    }
                    SymbolTableEntry* funIdEntry = newSymbolTableEntry(lexeme, FUNID, 0);
                    addToken(symTable, funIdEntry);
                    tokenNode = newTokenNode(funIdEntry, *lineNum);
                    return tokenNode;
                }
                if (getLexemeLength(beginPtr, *forwardPtr) > 30) {
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    lexeme[30] = '.'; lexeme[31] = '.'; lexeme[32] = '.'; lexeme[33] = '\0';
                    SymbolTableEntry* funLenEntry = lookupToken(symTable, lexeme);
                    if (!funLenEntry) {
                        funLenEntry = newSymbolTableEntry(lexeme, FUN_LENGTH_EXC, 0);
                        addToken(symTable, funLenEntry);
                    }
                    tokenNode = newTokenNode(funLenEntry, *lineNum);
                    ch = fetchNextChar(fp, buffer, forwardPtr);
                    for (; isdigit(ch); ch = fetchNextChar(fp, buffer, forwardPtr));
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    if (isdigit(ch)) {
                        state = 81;
                        break;
                    }
                    return tokenNode;
                }
                break;

            case 31: // Function identifier with digits
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (!isdigit(ch)) {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* commonEntry = lookupToken(symTable, lexeme);
                    if (commonEntry) {
                        tokenNode = newTokenNode(commonEntry, *lineNum);
                        return tokenNode;
                    }
                    SymbolTableEntry* funIdEntry = newSymbolTableEntry(lexeme, FUNID, 0);
                    addToken(symTable, funIdEntry);
                    tokenNode = newTokenNode(funIdEntry, *lineNum);
                    return tokenNode;
                }
                if (getLexemeLength(beginPtr, *forwardPtr) > 30) {
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    lexeme[30] = '.'; lexeme[31] = '.'; lexeme[32] = '.'; lexeme[33] = '\0';
                    SymbolTableEntry* funLenEntry = lookupToken(symTable, lexeme);
                    if (!funLenEntry) {
                        funLenEntry = newSymbolTableEntry(lexeme, FUN_LENGTH_EXC, 0);
                        addToken(symTable, funLenEntry);
                    }
                    tokenNode = newTokenNode(funLenEntry, *lineNum);
                    ch = fetchNextChar(fp, buffer, forwardPtr);
                    for (; isdigit(ch); ch = fetchNextChar(fp, buffer, forwardPtr));
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    return tokenNode;
                }
                break;

            case 100: // Function identifier starting with _m
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (isdigit(ch))
                    state = 31;
                else if (ch == 'a')
                    state = 101;
                else if (isalpha(ch))
                    state = 30;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* fnEntry = lookupToken(symTable, lexeme);
                    if (!fnEntry) {
                        fnEntry = newSymbolTableEntry(lexeme, FUNID, 0);
                        addToken(symTable, fnEntry);
                    }
                    tokenNode = newTokenNode(fnEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 101: // Function identifier starting with _ma
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (isdigit(ch))
                    state = 31;
                else if (ch == 'i')
                    state = 102;
                else if (isalpha(ch))
                    state = 30;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* fnEntry = lookupToken(symTable, lexeme);
                    if (!fnEntry) {
                        fnEntry = newSymbolTableEntry(lexeme, FUNID, 0);
                        addToken(symTable, fnEntry);
                    }
                    tokenNode = newTokenNode(fnEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 102: // Function identifier starting with _mai
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (isdigit(ch))
                    state = 31;
                else if (ch == 'n')
                    state = 103;
                else if (isalpha(ch))
                    state = 30;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* fnEntry = lookupToken(symTable, lexeme);
                    if (!fnEntry) {
                        fnEntry = newSymbolTableEntry(lexeme, FUNID, 0);
                        addToken(symTable, fnEntry);
                    }
                    tokenNode = newTokenNode(fnEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 103: // Function identifier _main
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (isalpha(ch))
                    state = 30;
                else if (isdigit(ch))
                    state = 31;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* mainEntry = lookupToken(symTable, lexeme);
                    if (!mainEntry) {
                        mainEntry = newSymbolTableEntry(lexeme, MAIN, 0);
                        addToken(symTable, mainEntry);
                    }
                    tokenNode = newTokenNode(mainEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 33: // Record identifier starting with #
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (islower(ch))
                    state = 34;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* errEntry = lookupToken(symTable, lexeme);
                    if (!errEntry) {
                        errEntry = newSymbolTableEntry(lexeme, LEXICAL_ERROR, 0);
                        addToken(symTable, errEntry);
                    }
                    tokenNode = newTokenNode(errEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 34: // Record identifier
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (!islower(ch)) {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* ruidEntry = lookupToken(symTable, lexeme);
                    if (!ruidEntry) {
                        ruidEntry = newSymbolTableEntry(lexeme, RUID, 0);
                        addToken(symTable, ruidEntry);
                    }
                    tokenNode = newTokenNode(ruidEntry, *lineNum);
                    return tokenNode;
                }
                break;

            // ------------------------- NUMERIC LITERAL STATES -------------------------
            
            case 17: // Integer literal
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '.') {
                    state = 19;
                } else if (!isdigit(ch)) {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* numEntry = lookupToken(symTable, lexeme);
                    if (numEntry) {
                        tokenNode = newTokenNode(numEntry, *lineNum);
                        return tokenNode;
                    }
                    double numVal = 0;
                    for (int i = 0; lexeme[i]; i++) {
                        numVal = numVal * 10 + (lexeme[i] - '0');
                    }
                    numEntry = newSymbolTableEntry(lexeme, NUM, numVal);
                    addToken(symTable, numEntry);
                    tokenNode = newTokenNode(numEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 19: // Real number with decimal point
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (isdigit(ch))
                    state = 20;
                else {
                    *forwardPtr -= 2;
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == BUFFER_SZ - 2 ||
                        *forwardPtr == 2 * BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 2)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* numEntry = lookupToken(symTable, lexeme);
                    if (numEntry) {
                        tokenNode = newTokenNode(numEntry, *lineNum);
                        return tokenNode;
                    }
                    double val = 0;
                    for (int i = 0; lexeme[i]; i++) {
                        val = val * 10 + (lexeme[i] - '0');
                    }
                    numEntry = newSymbolTableEntry(lexeme, NUM, val);
                    addToken(symTable, numEntry);
                    tokenNode = newTokenNode(numEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 20: // Real number with decimal digits
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (isdigit(ch))
                    state = 21;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* errEntry = lookupToken(symTable, lexeme);
                    if (!errEntry) {
                        errEntry = newSymbolTableEntry(lexeme, LEXICAL_ERROR, 0);
                        addToken(symTable, errEntry);
                    }
                    tokenNode = newTokenNode(errEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 21: // Real number with exponent
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == 'E')
                    state = 23;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* rnumEntry = lookupToken(symTable, lexeme);
                    if (rnumEntry) {
                        tokenNode = newTokenNode(rnumEntry, *lineNum);
                        return tokenNode;
                    }
                    double val = 0;
                    int i;
                    for (i = 0; lexeme[i] != '.'; i++) {
                        val = val * 10 + (lexeme[i] - '0');
                    }
                    val += (lexeme[i+1] - '0') / 10.0 + (lexeme[i+2] - '0') / 100.0;
                    rnumEntry = newSymbolTableEntry(lexeme, RNUM, val);
                    addToken(symTable, rnumEntry);
                    tokenNode = newTokenNode(rnumEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 23: // Real number with exponent sign
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (isdigit(ch))
                    state = 25;
                else if (ch == '+' || ch == '-')
                    state = 24;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* errEntry = lookupToken(symTable, lexeme);
                    if (!errEntry) {
                        errEntry = newSymbolTableEntry(lexeme, LEXICAL_ERROR, 0);
                        addToken(symTable, errEntry);
                    }
                    tokenNode = newTokenNode(errEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 24: // Real number with exponent digits
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (isdigit(ch))
                    state = 25;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* errEntry = lookupToken(symTable, lexeme);
                    if (!errEntry) {
                        errEntry = newSymbolTableEntry(lexeme, LEXICAL_ERROR, 0);
                        addToken(symTable, errEntry);
                    }
                    tokenNode = newTokenNode(errEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 25: // Real number with exponent digits
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (isdigit(ch))
                    state = 26;
                else {
                    --(*forwardPtr);
                    if (*forwardPtr < 0) *forwardPtr += 2 * BUFFER_SZ;
                    if (*forwardPtr == BUFFER_SZ - 1 || *forwardPtr == 2 * BUFFER_SZ - 1)
                        retractFlag = true;
                    extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                    SymbolTableEntry* errEntry = lookupToken(symTable, lexeme);
                    if (!errEntry) {
                        errEntry = newSymbolTableEntry(lexeme, LEXICAL_ERROR, 0);
                        addToken(symTable, errEntry);
                    }
                    tokenNode = newTokenNode(errEntry, *lineNum);
                    return tokenNode;
                }
                break;

            case 26: // Real number with exponent complete
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* rnumEntry = lookupToken(symTable, lexeme);
                    if (rnumEntry) {
                        tokenNode = newTokenNode(rnumEntry, *lineNum);
                        return tokenNode;
                    }
                    double val = 0;
                    int i;
                    for (i = 0; lexeme[i] != '.'; i++) {
                        val = val * 10 + (lexeme[i] - '0');
                    }
                    val += (lexeme[i+1] - '0') / 10.0 + (lexeme[i+2] - '0') / 100.0;
                    int exp = 0;
                    i += 4;
                    if (isdigit(lexeme[i]))
                        exp = lexeme[i] * 10 + lexeme[i+1];
                    else
                        exp = lexeme[i+1] * 10 + lexeme[i+2];
                    if (lexeme[i] == '-')
                        exp = -exp;
                    val *= pow(10, exp);
                    rnumEntry = newSymbolTableEntry(lexeme, RNUM, val);
                    addToken(symTable, rnumEntry);
                    tokenNode = newTokenNode(rnumEntry, *lineNum);
                    return tokenNode;
                }
                break;

            // ------------------------- COMMENT STATE -------------------------
            
            case 8: // Comment starting with %
                extractLexeme(beginPtr, forwardPtr, lexeme, buffer);
                {
                    SymbolTableEntry* commEntry = lookupToken(symTable, lexeme);
                    if (!commEntry) {
                        commEntry = newSymbolTableEntry(lexeme, COMMENT, 0);
                        addToken(symTable, commEntry);
                    }
                    tokenNode = newTokenNode(commEntry, *lineNum);
                    for (; (ch = fetchNextChar(fp, buffer, forwardPtr)) != '\n'; );
                    ++(*lineNum);
                    return tokenNode;
                }
                break;

            default:
                printf("Invalid state in DFA\n");
                return NULL;
        }
    }
}

// ========================= SECTION 4: INITIALIZATION FUNCTIONS =========================

// ------------------------- KEYWORD & TOKEN STRING INITIALIZATION -------------------------

void setupKeywordTrie(Trie* keywordTrie) {
    /*
       Populates the keyword trie with reserved keywords.
    */
    addKeyword(keywordTrie, "with", WITH);
    addKeyword(keywordTrie, "parameters", PARAMETERS);
    addKeyword(keywordTrie, "end", END);
    addKeyword(keywordTrie, "while", WHILE);
    addKeyword(keywordTrie, "union", UNION);
    addKeyword(keywordTrie, "endunion", ENDUNION);
    addKeyword(keywordTrie, "definetype", DEFINETYPE);
    addKeyword(keywordTrie, "as", AS);
    addKeyword(keywordTrie, "type", TYPE);
    addKeyword(keywordTrie, "global", GLOBAL);
    addKeyword(keywordTrie, "parameter", PARAMETER);
    addKeyword(keywordTrie, "list", LIST);
    addKeyword(keywordTrie, "input", INPUT);
    addKeyword(keywordTrie, "output", OUTPUT);
    addKeyword(keywordTrie, "int", INT);
    addKeyword(keywordTrie, "real", REAL);
    addKeyword(keywordTrie, "endwhile", ENDWHILE);
    addKeyword(keywordTrie, "if", IF);
    addKeyword(keywordTrie, "then", THEN);
    addKeyword(keywordTrie, "endif", ENDIF);
    addKeyword(keywordTrie, "read", READ);
    addKeyword(keywordTrie, "write", WRITE);
    addKeyword(keywordTrie, "return", RETURN);
    addKeyword(keywordTrie, "call", CALL);
    addKeyword(keywordTrie, "record", RECORD);
    addKeyword(keywordTrie, "endrecord", ENDRECORD);
    addKeyword(keywordTrie, "else", ELSE);
}

void initTokenStrings() {
    /*
       Initializes the tokenToString array so that each token enum value maps
       to its corresponding string. This aids in debugging and token printing.
    */
    if (tkStrInitialized) return;
    tkStrInitialized = true;
    for (int i = 0; i < TK_NOT_FOUND; i++) {
        tokenToString[i] = malloc(TOKEN_STR_LEN);
    }
    tokenToString[ASSIGNOP]       = "TK_ASSIGNOP";
    tokenToString[COMMENT]        = "TK_COMMENT";
    tokenToString[FIELDID]        = "TK_FIELDID";
    tokenToString[ID]             = "TK_ID";
    tokenToString[NUM]            = "TK_NUM";
    tokenToString[RNUM]           = "TK_RNUM";
    tokenToString[FUNID]          = "TK_FUNID";
    tokenToString[RUID]           = "TK_RUID";
    tokenToString[WITH]           = "TK_WITH";
    tokenToString[PARAMETERS]     = "TK_PARAMETERS";
    tokenToString[END]            = "TK_END";
    tokenToString[WHILE]          = "TK_WHILE";
    tokenToString[UNION]          = "TK_UNION";
    tokenToString[ENDUNION]       = "TK_ENDUNION";
    tokenToString[DEFINETYPE]     = "TK_DEFINETYPE";
    tokenToString[AS]             = "TK_AS";
    tokenToString[TYPE]           = "TK_TYPE";
    tokenToString[MAIN]           = "TK_MAIN";
    tokenToString[GLOBAL]         = "TK_GLOBAL";
    tokenToString[PARAMETER]      = "TK_PARAMETER";
    tokenToString[LIST]           = "TK_LIST";
    tokenToString[SQL]            = "TK_SQL";
    tokenToString[SQR]            = "TK_SQR";
    tokenToString[INPUT]          = "TK_INPUT";
    tokenToString[OUTPUT]         = "TK_OUTPUT";
    tokenToString[INT]            = "TK_INT";
    tokenToString[REAL]           = "TK_REAL";
    tokenToString[COMMA]          = "TK_COMMA";
    tokenToString[SEM]            = "TK_SEM";
    tokenToString[IF]             = "TK_IF";
    tokenToString[COLON]          = "TK_COLON";
    tokenToString[DOT]            = "TK_DOT";
    tokenToString[ENDWHILE]       = "TK_ENDWHILE";
    tokenToString[OP]             = "TK_OP";
    tokenToString[CL]             = "TK_CL";
    tokenToString[THEN]           = "TK_THEN";
    tokenToString[ENDIF]          = "TK_ENDIF";
    tokenToString[READ]           = "TK_READ";
    tokenToString[WRITE]          = "TK_WRITE";
    tokenToString[RETURN]         = "TK_RETURN";
    tokenToString[PLUS]           = "TK_PLUS";
    tokenToString[MINUS]          = "TK_MINUS";
    tokenToString[MUL]            = "TK_MUL";
    tokenToString[DIV]            = "TK_DIV";
    tokenToString[CALL]           = "TK_CALL";
    tokenToString[RECORD]         = "TK_RECORD";
    tokenToString[ENDRECORD]      = "TK_ENDRECORD";
    tokenToString[ELSE]           = "TK_ELSE";
    tokenToString[AND]            = "TK_AND";
    tokenToString[OR]             = "TK_OR";
    tokenToString[NOT]            = "TK_NOT";
    tokenToString[LT]             = "TK_LT";
    tokenToString[LE]             = "TK_LE";
    tokenToString[EQ]             = "TK_EQ";
    tokenToString[GT]             = "TK_GT";
    tokenToString[GE]             = "TK_GE";
    tokenToString[NE]             = "TK_NE";
    tokenToString[EPS]            = "TK_EPS";
    tokenToString[DOLLAR]         = "TK_DOLLAR";
    tokenToString[LEXICAL_ERROR]  = "LEXICAL_ERROR";
    tokenToString[ID_LENGTH_EXC]  = "IDENTIFIER_LENGTH_EXCEEDED";
    tokenToString[FUN_LENGTH_EXC] = "FUNCTION_NAME_LENGTH_EXCEEDED";
}

// ========================= SECTION 5: TOKEN LIST GENERATION =========================

// ------------------------- TOKEN LIST GENERATION -------------------------

TokenList* getAllTokens(FILE* fp) {
    /*
       Retrieves all tokens from the input file by repeatedly invoking the DFA.
       Returns a TokenList containing the ordered tokens.
    */
    char twinBuffer[BUFFER_SZ * 2];
    int fwdPtr = 2 * BUFFER_SZ - 1;
    int lineNumber = 1;

    Trie* keywordTrie = initTrie();
    setupKeywordTrie(keywordTrie);
    initTokenStrings();

    SymbolTable* symTable = newSymbolTable();
    TokenList* tokenList = createTokenList();

    while (true) {
        TokenNode* tkNode = getNextToken(fp, twinBuffer, &fwdPtr, &lineNumber, keywordTrie, symTable);
        if (!tkNode) {
            printf("No token retrieved\n");
            break;
        }
        appendTokenNode(tokenList, tkNode);
        if (tkNode->entry->tokenType == DOLLAR)
            break;
    }
    return tokenList;
}

// ========================= SECTION 6: COMMENT HANDLING & TOKEN DISPLAY =========================

// ------------------------- COMMENT HANDLING & TOKEN DISPLAY -------------------------

void removeComments(char* sourceFile, char* cleanFile) {
    /*
       Reads the source file and removes comments.
       A comment starts with '%' and is terminated by a newline.
    */
    FILE* inFile = fopen(sourceFile, "r");
    if (inFile == NULL) {
        printf("Error: Cannot open file %s\n", sourceFile);
        return;
    }
    char lineBuf[4096];
    char* commentPtr;
    while (fgets(lineBuf, sizeof(lineBuf), inFile) != NULL) {
        commentPtr = strchr(lineBuf, '%');
        if (commentPtr != NULL) {
            *commentPtr = '\n';
            *(commentPtr + 1) = '\0';
        }
        printf("%s", lineBuf);
    }
    fclose(inFile);
}

void displayTokenList(TokenList* list) {
    /*
       Prints the token list to the console.
       Uses the tokenToString mapping to display token names.
    */
    TokenNode* current = list->head;
    while (current != NULL) {
        char* tokenStr;
        if (current->entry->tokenType < LEXICAL_ERROR)
            tokenStr = strdup(tokenToString[current->entry->tokenType]);
        else if (current->entry->tokenType == LEXICAL_ERROR)
            tokenStr = strdup("Unrecognized pattern");
        else if (current->entry->tokenType == ID_LENGTH_EXC)
            tokenStr = strdup("Identifier length exceeded 20");
        else if (current->entry->tokenType == FUN_LENGTH_EXC)
            tokenStr = strdup("Function name length exceeded 30");
        else
            tokenStr = strdup("");
        printf("Line No: %5d \t Lexeme: %35s \t Token: %35s\n",
               current->lineNum, current->entry->lexeme, tokenStr);
        current = current->next;
    }
}

TokenList* lexInput(FILE* fp, char* outputPath) {
    /*
       Wrapper function for the lexical analysis phase.
       Verifies the input file and returns the generated token list.
    */
    if (!fp) {
        printf("Error: Input file not found for lexical analysis\n");
        exit(-1);
    }
    TokenList* tokens = getAllTokens(fp);
    if (!tokens) {
        printf("Error: Failed to retrieve token list\n");
        exit(-1);
    }
    return tokens;
}

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
char* tokenToString[TK_NOT_FOUND];
bool retractFlag = false;          // renamed from "specialThing"
bool tkStrInitialized = false;     // renamed from "tkSInit"
bool debugPrint = false;

// ------------------------- TRIE FUNCTIONS -------------------------

TrieNode* newTrieNode() {
    /* 
       Allocates and initializes a new trie node.
       All children pointers are set to NULL and the end-of-word flag is 0.
    */
    TrieNode* node = (TrieNode*) malloc(sizeof(TrieNode));
    if (node) {
        node->isEnd = 0;
        for (int i = 0; i < ALPHABET_COUNT; i++) {
            node->children[i] = NULL;
        }
    } else {
        printf("Error: Unable to allocate memory for Trie node\n");
    }
    return node;
}

Trie* initTrie() {
    /*
       Creates a new Trie with a newly allocated root node.
    */
    Trie* trie = (Trie*) malloc(sizeof(Trie));
    if (trie) {
        trie->root = newTrieNode();
    } else {
        printf("Error: Unable to allocate memory for Trie\n");
    }
    return trie;
}

void addKeyword(Trie* trie, const char* word, Token tkType) {
    /*
       Inserts a keyword into the trie.
       Traverses or creates nodes corresponding to each character in 'word'.
    */
    TrieNode* curr = trie->root;
    for (int i = 0; word[i] != '\0'; i++) {
        int idx = word[i] - 'a';
        if (!curr->children[idx]) {
            curr->children[idx] = newTrieNode();
        }
        curr = curr->children[idx];
    }
    curr->isEnd = 1;
    curr->tokenType = tkType;
}

Token findKeyword(Trie* trie, const char* word) {
    /*
       Searches the trie for 'word'.
       Returns the corresponding token if found; otherwise, TK_NOT_FOUND.
    */
    TrieNode* curr = trie->root;
    for (int i = 0; word[i] != '\0'; i++) {
        int idx = word[i] - 'a';
        if (!curr->children[idx]) {
            return TK_NOT_FOUND;
        }
        curr = curr->children[idx];
    }
    return (curr && curr->isEnd) ? curr->tokenType : TK_NOT_FOUND;
}

// ---------------------- SYMBOL TABLE FUNCTIONS ----------------------

SymbolTable* newSymbolTable() {
    /*
       Allocates and initializes a new symbol table.
       Begins with an initial capacity defined by INIT_SYMBOL_TABLE_CAP.
    */
    SymbolTable* table = (SymbolTable*) malloc(sizeof(SymbolTable));
    if (!table) {
        printf("Error: Unable to allocate memory for Symbol Table\n");
        return NULL;
    }
    table->entries = (SymbolTableEntry**) malloc(INIT_SYMBOL_TABLE_CAP * sizeof(SymbolTableEntry*));
    if (!table->entries) {
        printf("Error: Unable to allocate memory for Symbol Table entries\n");
    }
    table->capacity = INIT_SYMBOL_TABLE_CAP;
    table->size = 0;
    return table;
}

void addToken(SymbolTable* table, SymbolTableEntry* entry) {
    /*
       Inserts a new token entry into the symbol table.
       Resizes the table if capacity is reached.
    */
    if (table->size == table->capacity) {
        table->capacity *= 2;
        table->entries = (SymbolTableEntry**) realloc(table->entries, table->capacity * sizeof(SymbolTableEntry*));
        if (!table->entries) {
            printf("Error: Resizing Symbol Table failed\n");
            return;
        }
    }
    table->entries[table->size++] = entry;
}

SymbolTableEntry* newSymbolTableEntry(char* lexeme, Token tkType, double numVal) {
    /*
       Creates a new symbol table entry for the given lexeme, token type, and numeric value.
    */
    SymbolTableEntry* entry = (SymbolTableEntry*) malloc(sizeof(SymbolTableEntry));
    if (!entry) {
        printf("Error: Unable to allocate memory for Symbol Table Entry\n");
        return NULL;
    }
    strcpy(entry->lexeme, lexeme);
    entry->tokenType = tkType;
    entry->numericValue = numVal;
    return entry;
}

SymbolTableEntry* lookupToken(SymbolTable* table, char* lexeme) {
    /*
       Searches for 'lexeme' in the symbol table.
       Returns the corresponding entry if found; otherwise, NULL.
    */
    for (int i = 0; i < table->size; i++) {
        if (!strcmp(lexeme, table->entries[i]->lexeme))
            return table->entries[i];
    }
    return NULL;
}

// ----------------------- TOKEN LIST FUNCTIONS -----------------------

TokenList* createTokenList() {
    /*
       Allocates and initializes a new token list.
    */
    TokenList* list = (TokenList*) malloc(sizeof(TokenList));
    if (!list) {
        printf("Error: Unable to allocate memory for Token List\n");
        return NULL;
    }
    list->count = 0;
    list->head = list->tail = NULL;
    return list;
}

TokenNode* newTokenNode(SymbolTableEntry* entry, int lineNum) {
    /*
       Creates a new token node with the given symbol table entry and source line number.
    */
    TokenNode* node = (TokenNode*) malloc(sizeof(TokenNode));
    if (!node) {
        printf("Error: Unable to allocate memory for Token Node\n");
        return NULL;
    }
    node->entry = entry;
    node->lineNum = lineNum;
    node->next = NULL;
    return node;
}

void appendTokenNode(TokenList* list, TokenNode* node) {
    /*
       Appends a token node to the end of the token list.
    */
    if (list->count == 0) {
        list->head = list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }
    list->count++;
}

// ------------------- TWIN BUFFER & LEXEME EXTRACTION -------------------

char fetchNextChar(FILE* fp, char *buffer, int *forwardPtr) {
    /*
       Advances the forward pointer in the twin buffer and returns the next character.
       If the pointer reaches the end of one half, refills the other half from file.
    */
    if (*forwardPtr == BUFFER_SZ - 1 && !retractFlag) {
        if (feof(fp))
            buffer[BUFFER_SZ] = '\0';
        else {
            int bytesRead = fread(buffer + BUFFER_SZ, sizeof(char), BUFFER_SZ, fp);
            if (bytesRead < BUFFER_SZ)
                buffer[BUFFER_SZ + bytesRead] = '\0';
        }
    } else if (*forwardPtr == (2 * BUFFER_SZ - 1) && !retractFlag) {
        if (feof(fp))
            buffer[0] = '\0';
        else {
            int bytesRead = fread(buffer, sizeof(char), BUFFER_SZ, fp);
            if (bytesRead < BUFFER_SZ)
                buffer[bytesRead] = '\0';
        }
    }
    if (retractFlag)
        retractFlag = false;
    
    *forwardPtr = (*forwardPtr + 1) % (2 * BUFFER_SZ);
    return buffer[*forwardPtr];
}

void extractLexeme(int beginPtr, int* forwardPtr, char* lexeme, char* buffer) {
    /*
       Copies characters from the twin buffer (from beginPtr to the current forward pointer)
       into the lexeme array. Handles wrap-around in the twin buffer.
    */
    if (*forwardPtr >= beginPtr) {
        int i;
        for (i = 0; i <= (*forwardPtr - beginPtr); i++) {
            lexeme[i] = buffer[beginPtr + i];
        }
        lexeme[i] = '\0';
    } else {
        int i;
        for (i = 0; i <= (*forwardPtr + 2 * BUFFER_SZ - beginPtr); i++) {
            lexeme[i] = buffer[(beginPtr + i) % (2 * BUFFER_SZ)];
        }
        lexeme[i] = '\0';
    }
}

int getLexemeLength(int begPtr, int forwardPtr) {
    /*
       Returns the length of the lexeme between begPtr and forwardPtr in the twin buffer.
    */
    return (forwardPtr >= begPtr) ? (forwardPtr - begPtr + 1)
                                  : (2 * BUFFER_SZ + forwardPtr - begPtr + 1);
}

// ---------------------- DFA: TOKEN EXTRACTION -----------------------

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
            case 0:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '\n') {
                    ++(*lineNum);
                    beginPtr = (beginPtr + 1) % (2 * BUFFER_SZ);
                } else if (ch == ' ' || ch == '\t' || ch == '\r') {
                    beginPtr = (beginPtr + 1) % (2 * BUFFER_SZ);
                } else if (isdigit(ch)) {
                    state = 5;
                } else if (ch == '%') {
                    state = 65;
                } else if (ch == '>') {
                    state = 56;
                } else if (ch == '<') {
                    state = 53;
                } else if (ch == '=') {
                    state = 68;
                } else if (ch == '_') {
                    state = 69;
                } else if (ch == '/') {
                    state = 45;
                } else if (ch == '@') {
                    state = 73;
                } else if (ch == '*') {
                    state = 44;
                } else if (ch == '#') {
                    state = 75;
                } else if (ch == '[') {
                    state = 23;
                } else if (ch == ']') {
                    state = 24;
                } else if (ch == ',') {
                    state = 29;
                } else if (ch == ':') {
                    state = 31;
                } else if (ch == ';') {
                    state = 30;
                } else if (ch == '+') {
                    state = 42;
                } else if (ch == ')') {
                    state = 35;
                } else if (ch == '-') {
                    state = 43;
                } else if (ch == '(') {
                    state = 34;
                } else if (ch == '&') {
                    state = 76;
                } else if (ch == '~') {
                    state = 52;
                } else if (ch == '!') {
                    state = 78;
                } else if (ch == 'a' || (ch > 'd' && ch <= 'z')) {
                    state = 3;
                } else if (ch >= 'b' && ch <= 'd') {
                    state = 79;
                } else if (ch == '.') {
                    state = 59;
                } else if (ch == '\0') {
                    lexeme[0] = ch;
                    SymbolTableEntry* eofEntry = newSymbolTableEntry(lexeme, DOLLAR, 0);
                    tokenNode = newTokenNode(eofEntry, *lineNum);
                    return tokenNode;
                } else {
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

            case 1:
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

            case 2:
                // This state is unused (commented out in original code)
                break;

            case 3:
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

            case 4:
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

            case 5:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '.') {
                    state = 60;
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

            case 6:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == 'E')
                    state = 62;
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

            case 7:
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

            case 8:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (isdigit(ch))
                    state = 81;
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

            case 9:
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

            case 19:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (isalpha(ch))
                    state = 8;
                else if (isdigit(ch))
                    state = 81;
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

            case 23:
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

            case 24:
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

            case 29:
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

            case 30:
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

            case 31:
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

            case 34:
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

            case 35:
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

            case 42:
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

            case 43:
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

            case 44:
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

            case 45:
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

            case 50:
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

            case 51:
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

            case 52:
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

            case 53:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '-') 
                    state = 66;
                else if (ch == '=')
                    state = 54;
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

            case 54:
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

            case 55:
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

            case 56:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '=') 
                    state = 57;
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

            case 57:
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

            case 58:
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

            case 59:
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

            case 60:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (isdigit(ch))
                    state = 61;
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

            case 61:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (isdigit(ch))
                    state = 6;
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

            case 62:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (isdigit(ch))
                    state = 64;
                else if (ch == '+' || ch == '-')
                    state = 63;
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

            case 63:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (isdigit(ch))
                    state = 64;
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

            case 64:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (isdigit(ch))
                    state = 7;
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

            case 65:
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

            case 66:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '-') 
                    state = 67;
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

            case 67:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '-') 
                    state = 1;
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

            case 68:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '=')
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

            case 69:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == 'm')
                    state = 70;
                else if (isalpha(ch))
                    state = 8;
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

            case 70:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (isdigit(ch))
                    state = 81;
                else if (ch == 'a')
                    state = 71;
                else if (isalpha(ch))
                    state = 8;
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

            case 71:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (isdigit(ch))
                    state = 81;
                else if (ch == 'i')
                    state = 72;
                else if (isalpha(ch))
                    state = 8;
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

            case 72:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (isdigit(ch))
                    state = 81;
                else if (ch == 'n')
                    state = 19;
                else if (isalpha(ch))
                    state = 8;
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

            case 73:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '@')
                    state = 74;
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

            case 74:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '@')
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

            case 75:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (islower(ch))
                    state = 9;
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

            case 76:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '&')
                    state = 77;
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

            case 77:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '&')
                    state = 50;
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

            case 78:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch == '=')
                    state = 58;
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

            case 79:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (islower(ch))
                    state = 3;
                else if (ch >= '2' && ch <= '7')
                    state = 80;
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

            case 80:
                ch = fetchNextChar(fp, buffer, forwardPtr);
                if (ch >= '2' && ch <= '7')
                    state = 4;
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
                        state = 4;
                        break;
                    }
                    return tokenNode;
                }
                break;

            case 81:
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

            default:
                printf("Invalid state in DFA\n");
                return NULL;
        }
    }
}

// ------------------- KEYWORD & TOKEN STRING INITIALIZATION -------------------

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

// ------------------- TOKEN LIST GENERATION -------------------

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

// ------------------- COMMENT HANDLING & TOKEN DISPLAY -------------------

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

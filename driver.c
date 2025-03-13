
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "lexer.h"
#include "lexerDef.h"

// Function to display usage instructions
void displayUsage(char* programName) {
    printf("Usage: %s <input_source_file>\n", programName);
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
    // Ensure the correct number of arguments
    if (argc != 2) {
        printf("Error: Incorrect number of arguments provided.\n");
        displayUsage(argv[0]);
    }

    char* inputFilePath = argv[1];
    FILE* inputFile = fopen(inputFilePath, "r");

    // Check if the input file can be opened
    if (!inputFile) {
        printf("Error: Unable to open input file '%s'.\n", inputFilePath);
        exit(EXIT_FAILURE);
    }

    printf("\n========= LEXICAL ANALYSIS STARTED =========\n\n");

    // Generate token list from the input file
    TokenList* tokenList = lexInput(inputFile, "output_tokens.txt");

    if (!tokenList) {
        printf("Error: Lexer failed to generate tokens.\n");
        fclose(inputFile);
        exit(EXIT_FAILURE);
    }

    // Display the generated tokens
    printf("%-10s %-20s %-20s\n", "Line No.", "Token", "Lexeme");
    printf("---------------------------------------------------\n");

    TokenNode* current = tokenList->head;
    while (current != NULL) {
        printf("%-10d %-20s %-20s\n",
               current->lineNum,
               tokenToString[current->entry->tokenType],
               current->entry->lexeme);
        current = current->next;
    }

    printf("\n========= LEXICAL ANALYSIS COMPLETED =========\n\n");


    return 0;
}


// gcc driver.c lexer.c -o compiler
// ./compiler input.txt
// ./compiler input.txt > output.txt
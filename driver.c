
// #include <stdio.h>
// #include <stdlib.h>
// #include <stdbool.h>
// #include "lexer.h"
// #include "parser.h"
// // Function to display usage instructions
// void displayUsage(char* programName) {
//     printf("Usage: %s <input_source_file>\n", programName);
//     exit(EXIT_FAILURE);
// }

// int main(int argc, char* argv[]) {
//     // Ensure the correct number of arguments
//     if (argc != 2) {
//         printf("Error: Incorrect number of arguments provided.\n");
//         displayUsage(argv[0]);
//     }

//     char* inputFilePath = argv[1];
//     FILE* inputFile = fopen(inputFilePath, "r");

//     // Check if the input file can be opened
//     if (!inputFile) {
//         printf("Error: Unable to open input file '%s'.\n", inputFilePath);
//         exit(EXIT_FAILURE);
//     }

//     printf("\n========= LEXICAL ANALYSIS STARTED =========\n\n");

//     // Generate token list from the input file
//     TokenList* tokenList = lexInput(inputFile, "output_tokens.txt");

//     if (!tokenList) {
//         printf("Error: Lexer failed to generate tokens.\n");
//         fclose(inputFile);
//         exit(EXIT_FAILURE);
//     }

//     // Display the generated tokens
//     printf("%-10s %-20s %-20s\n", "Line No.", "Token", "Lexeme");
//     printf("---------------------------------------------------\n");

//     TokenNode* current = tokenList->head;
//     while (current != NULL) {
//         printf("%-10d %-20s %-20s\n",
//                current->lineNum,
//                tokenToString[current->entry->tokenType],
//                current->entry->lexeme);
//         current = current->next;
//     }

//     printf("\n========= LEXICAL ANALYSIS COMPLETED =========\n\n");

//     // Close the input file as Lexer has finished processing it
//     fclose(inputFile);

//     printf("\n========= SYNTAX ANALYSIS (PARSER) STARTED =========\n\n");

//     // Call the parser with input and output file paths
//     parseInputSourceCode(argv[1], "parseTree.txt");

//     printf("\n========= SYNTAX ANALYSIS (PARSER) COMPLETED =========\n\n");

//     printf("The parse tree has been saved to 'parseTree.txt'.\n");

//     return 0;
// }


/*
#                    GROUP - 8
# 2020B1A70630P                       Aditya Thakur
# 2021A7PS2001P                       Amal Sayeed
# 2021A7PS2005P                       Ohiduz Zaman
# 2021A7PS2682P                       Priyansh Patel
# 2021A7PS2002P                       Rachoita Das
# 2020B1A70611P                       Subhramit Basu Bhowmick
*/

// Drives the flow of execution to solve the given problem.
// Will contain the driver main()

#include <stdio.h>
#include <stdlib.h>
#include "parserDef.h"
#include "lexer.h"
#include "parser.h"
#include <time.h>

bool shouldPrint=true;

int main(int argc, char* argv[]) {

    if(argc!=3) {
        printf("Wrong execution format. Use: ./stage1exe <input_file.txt> <output_file.txt>\n");
        return -1;
    }

    int choice = -1;

    printf("\nWelcome to our compiler!\n* FIRST and FOLLOW set automated.\n* Both lexical and syntax analysis modules implemented.\n* All modules compile and run without any errors/faults.\n* Our compiler is fully functional!\n");    

    while(choice) {

        printf("\nSelect your option:\n 0: To exit\n 1: To remove comments and print on console\n 2: To print tokens list on console\n 3: To parse and print the parse tree\n 4: To print total time taken on console\n");
        scanf("%d", &choice);
        shouldPrint=true;

        switch(choice) {

            case 0: printf("Thank you!\n");
                    return 0;
                    break;

            case 1: removeComments(argv[1], argv[2]);
                    break;

            case 2: FILE* lexIn = fopen(argv[1], "r");
                    TokenList* tokens = lexInput(lexIn, argv[2]);
                    displayTokenList(tokens);
                    fclose(lexIn);
                    break;

            case 3: parseInputSourceCode(argv[1], argv[2]);
                    break;
            
            case 4: clock_t start_time, end_time;
                    double total_CPU_time, total_CPU_time_in_seconds;

                    start_time = clock();

                    shouldPrint=false;
                    // invoke your lexer and parser here
                    parseInputSourceCode(argv[1], argv[2]);

                    end_time = clock();
                    total_CPU_time = (double) (end_time - start_time);
                    total_CPU_time_in_seconds = total_CPU_time / CLOCKS_PER_SEC;

                    // Print both total_CPU_time and total_CPU_time_in_seconds
                    printf("\nHere is the total time taken for Lexical and Syntactic analysis and printing of parse tree in the file\n");
                    printf("Total CPU time (in clock cycles): %lf\n",total_CPU_time);
                    printf("Total CPU time (in seconds): %lf\n",total_CPU_time_in_seconds);
                    break;
            
            default: printf("Please enter a correct option!\n");
                     break;
        }
    }

    return 0;
}
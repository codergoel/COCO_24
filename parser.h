// /*
//         Group No. 24
// 2022A7PS0170P : Tanmay Goel
// 2022A7PS0123P : Daksh Ahuja
// 2022A7PS0166P : Pratham Walia
// 2022A7PS0182P : Anant Malhotra
// 2022A7PS1733P : Pratham Prateek Mohanty
// 2022A7PS0073P : Richie Singh
// */


#ifndef PARSER_MODULE
#define PARSER_MODULE


#include "../lexer/lexer.h"
#include "parserDef.h"

void parseInputSourceCode(char* inputFile,char* outputFile );


#endif


// //Let's break down **`parser.h`** in **detail**, explaining every line, including the purpose of `#ifndef`, `#define`, and other components.  

// ---

// # **1. Overview of `parser.h`**
// - `parser.h` is a **header file** that contains **function prototypes** for `parser.c`.  
// - It ensures that **function declarations** and **data structures** are accessible to other files, such as `parser.c`.  
// - It also includes necessary **dependencies** from other modules.

// ---

// # **2. Detailed Line-by-Line Explanation**
// ### **1. Header Guard: `#ifndef PARSER_PROTOTYPES`**
// ```c
// #ifndef PARSER_PROTOTYPES
// ```
// #### **What does this do?**
// - `#ifndef` stands for **"if not defined"**.
// - It checks if `PARSER_PROTOTYPES` has **already been defined**.
// - If it **is not defined**, the file is included.

// #### **Why is this needed?**
// - This prevents **multiple inclusions** of the same header file, which could cause **compilation errors**.
// - Without this, if `parser.h` is included **twice**, the compiler may **redefine functions or variables**, causing errors.

// ---

// ### **2. Defining the Header Guard: `#define PARSER_PROTOTYPES`**
// ```c
// #define PARSER_PROTOTYPES
// ```
// #### **What does this do?**
// - It **defines** the macro `PARSER_PROTOTYPES`.
// - If `parser.h` is included again in another file, `#ifndef PARSER_PROTOTYPES` will **fail**, preventing re-inclusion.

// #### **Why is this needed?**
// - This ensures that the code inside the `#ifndef` block **runs only once**.

// ---

// ### **3. Including Dependencies**
// ```c
// #include "../lexer/lexer.h"
// #include "parserDef.h"
// ```
// #### **What does this do?**
// - `#include` brings in **external files** needed for the parser.
// - `"../lexer/lexer.h"`:
//   - Includes **definitions of tokens** and **lexer-related functions**.
//   - The **lexer provides tokens** to the parser.
// - `"parserDef.h"`:
//   - Includes **data structures for the parser**, like `parseTable`, `pTreeNode`, `grammarRule`, etc.

// #### **Why are these needed?**
// - The parser **relies on the lexer** to get tokens.
// - The parser uses **grammar rules** and **parse trees** defined in `parserDef.h`.

// ---

// ### **4. Function Prototype**
// ```c
// void parseInputSourceCode(char* inpFile, char* opFile);
// ```
// #### **What does this do?**
// - **Declares a function** that will be **implemented in `parser.c`**.
// - `parseInputSourceCode`:
//   - **Takes an input file (`inpFile`)** → This is the source code file.
//   - **Takes an output file (`opFile`)** → This stores the parse tree or errors.
//   - **Returns nothing (`void`)**.

// #### **Why is this needed?**
// - Other files (like `driver.c`) **need access** to this function to invoke parsing.

// ---

// ### **5. Ending the Header Guard**
// ```c
// #endif
// ```
// #### **What does this do?**
// - **Closes** the `#ifndef` block.
// - Ensures that everything between `#ifndef` and `#endif` **is included only once**.

// ---

// # **3. Summary**
// | **Line** | **Purpose** |
// |----------|------------|
// | `#ifndef PARSER_PROTOTYPES` | Checks if the header has been included before. |
// | `#define PARSER_PROTOTYPES` | Defines `PARSER_PROTOTYPES` to prevent re-inclusion. |
// | `#include "../lexer/lexer.h"` | Includes lexer functions and token definitions. |
// | `#include "parserDef.h"` | Includes parser-related data structures. |
// | `void parseInputSourceCode(char* inpFile, char* opFile);` | Declares the function that will parse the input source code. |
// | `#endif` | Closes the header guard. |

// ---

// # **4. Why Are Header Guards Important?**
// Consider this example:
// ```c
// #include "parser.h"
// #include "parser.h"  // Included twice accidentally
// ```
// Without header guards, the **second inclusion** would cause **duplicate definitions**, leading to **compilation errors**.  
// With `#ifndef`, the **second inclusion is ignored**, preventing errors.

// Would you like me to explain **parserDef.h** in a similar way? 🚀
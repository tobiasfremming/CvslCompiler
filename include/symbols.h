#ifndef SYMBOLS_H
#define SYMBOLS_H
#include "symbol_table.h"

#include <stddef.h>

typedef enum
{
    SYMBOL_GLOBAL_VAR, SYMBOL_GLOBAL_ARRAY, SYMBOL_FUNCTION, SYMBOL_PARAMETER, SYMBOL_LOCAL_VAR,
} symtype_t;

// Use as a normal array, to get the name of a symbol type: SYMBOL_TYPE_NAMES[symbol->type]
#define SYMBOL_TYPE_NAMES ((const char *[]){      \
        [SYMBOL_GLOBAL_VAR] = "GLOBAL_VAR",       \
        [SYMBOL_GLOBAL_ARRAY] = "GLOBAL_ARRAY",   \
        [SYMBOL_FUNCTION] = "FUNCTION",           \
        [SYMBOL_PARAMETER] = "PARAMETER",         \
        [SYMBOL_LOCAL_VAR] = "LOCAL_VAR"})

typedef struct symbol
{
    char *name;             // Symbol name ( not owned )
    symtype_t type;         // Symbol type
    node_t *node;           // The AST node that defined this symbol ( not owned )
    size_t sequence_number; // Sequence number in the symbol table this symbol belongs to

    /* Global variables and arrays have function_symtable = NULL
     * Functions point to their own symbol tables here, but the function itself is a global symbol
     * Parameters and local variables point to the function_symtable they belong to */
    struct symbol_table *function_symtable;
} symbol_t;

/* Global symbol table and string list */
extern symbol_table_t *global_symbols;
extern char **string_list;
extern size_t string_list_len;

void create_tables ( void );
void print_tables ( void );
void destroy_tables ( void );

#endif // SYMBOLS_H

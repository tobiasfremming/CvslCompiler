#include "vslc.h"

/* Global symbol table and string list */
symbol_table_t *global_symbols;
char **string_list;
size_t string_list_len;
size_t string_list_capacity;

static void find_globals ( void );
static void bind_names ( symbol_table_t *local_symbols, node_t *root );
static void push_local_scope ( symbol_table_t *local_symbols );
static void pop_local_scope ( symbol_table_t *local_symbols );
static void print_symbol_table ( symbol_table_t *table, int nesting );
static void destroy_symbol_tables ( void );

static size_t add_string ( char* string );
static void print_string_list ( void );
static void destroy_string_list ( void );

/* External interface */

/* Creates a global symbol table, and local symbol tables for each function.
 * While building the symbol tables:
 *  - All usages of symbols are bound to their symbol table entries.
 *  - All strings are entered into the string_list
 */
void create_tables ( void )
{
    // Create a global symbol table, and make symbols for all globals
    find_globals ();

    // For all functions, we want to fill their local symbol tables,
    // and bind all names found in the function body
    for ( int i = 0; i < global_symbols->n_symbols; i++ )
    {
        symbol_t *symbol = global_symbols->symbols[i];
        if ( symbol->type == SYMBOL_FUNCTION )
            bind_names ( symbol->function_symtable, symbol->node->children[2] );
    }
}

/* Prints the global symbol table, and the local symbol tables for each function.
 * Also prints the global string list.
 * Finally prints out the AST again, with bound symbols.
 */
void print_tables ( void )
{
    print_symbol_table ( global_symbols, 0 );
    printf ( "\n == STRING LIST == \n" );
    print_string_list ( );
    printf ( "\n == BOUND SYNTAX TREE == \n" );
    print_syntax_tree ( );
}

/* Destroys all symbol tables and the global string list */
void destroy_tables ( void )
{
    destroy_symbol_tables ( );
    destroy_string_list ( );
}

/* Internal matters */

#define CREATE_AND_INSERT_SYMBOL(table, ...) do {                        \
    symbol_t *symbol = malloc(sizeof(symbol_t));                         \
    *symbol = (symbol_t) {                                               \
    __VA_ARGS__                                                          \
    };                                                                   \
    if ( symbol_table_insert ( (table), symbol ) == INSERT_COLLISION ) { \
        fprintf ( stderr, "error: symbol '%s' already defined\n", symbol->name ); \
        exit ( EXIT_FAILURE );                                           \
    }                                                                    \
    } while(false)

/* Goes through all global declarations in the syntax tree, adding them to the global symbol table.
 * When adding functions, local symbol tables are created, and symbols for the functions parameters are added.
 */
static void find_globals ( void )
{
    global_symbols = symbol_table_init ( );
    for ( int i = 0; i < root->n_children; i++ )
    {
        node_t *node = root->children[i];
        if ( node->type == GLOBAL_DECLARATION )
        {
            node_t *global_variable_list = node->children[0];
            for ( int j = 0; j < global_variable_list->n_children; j++ )
            {
                node_t *var = global_variable_list->children[j];
                char* name;
                symtype_t symtype;

                // The global variable list can both contain arrays and normal variables.
                if ( var->type == ARRAY_INDEXING )
                {
                    name = var->children[0]->data;
                    symtype = SYMBOL_GLOBAL_ARRAY;
                }
                else
                {
                    assert ( var->type == IDENTIFIER_DATA );
                    name = var->data;
                    symtype = SYMBOL_GLOBAL_VAR;
                }

                CREATE_AND_INSERT_SYMBOL( global_symbols,
                                          .name = name,
                                          .type = symtype,
                                          .node = var,
                                          .function_symtable = NULL );
            }
        }
        else if ( node->type == FUNCTION )
        {
            // Functions have their own local symbol table. We make it now, and add the function parameters
            symbol_table_t *function_symtable = symbol_table_init ( );
            // We let the global hashmap be the backup of the local scope
            function_symtable->hashmap->backup = global_symbols->hashmap;

            node_t *parameters = node->children[1];
            for ( int j = 0; j < parameters->n_children; j++ ) {
                CREATE_AND_INSERT_SYMBOL( function_symtable,
                                          .name = parameters->children[j]->data,
                                          .type = SYMBOL_PARAMETER,
                                          .node = parameters->children[j],
                                          .function_symtable = NULL );
            }

            CREATE_AND_INSERT_SYMBOL( global_symbols,
                                      .name = node->children[0]->data,
                                      .type = SYMBOL_FUNCTION,
                                      .node = node,
                                      .function_symtable = function_symtable );
        }
        else
        {
            assert ( false && "Unknown global node type" );
        }
    }
}

/* A recursive function that traverses the body of a function, and:
 *  - Adds variable declarations to the function's local symbol table.
 *  - Pushes and pops local variable scopes when entering blocks.
 *  - Binds identifiers to the symbol it references.
 *  - Moves STRING_DATA nodes' data into the global string list,
 *    and replaces the node with a STRING_LIST_REFERENCE node.
 *    This node's data is the string's position in the list casted to a void*
 */
static void bind_names ( symbol_table_t *local_symbols, node_t *node )
{
    switch ( node->type )
    {
        // Can either be a variable in an expression, or the name of a function in a function call
        // Either way, we wish to associate it with its symbol
        case IDENTIFIER_DATA: {
            symbol_t* symbol = symbol_hashmap_lookup ( local_symbols->hashmap, node->data );
            if ( symbol == NULL ) {
                fprintf ( stderr, "error: unrecognized symbol '%s'\n", (char*)node->data );
                exit ( EXIT_FAILURE );
            }
            node->symbol = symbol;
            break;
        }

        // Blocks may contain a list of declarations.
        // In such cases, a scope gets pushed, the declarations get added, and the name binding continues in the body
        case BLOCK:
            if ( node->n_children == 2 )
            {
                push_local_scope ( local_symbols );
                // Iterate through all declarations in the delcaration list
                node_t *decl_list = node->children[0];
                for (int i = 0; i < decl_list->n_children; i++ )
                {
                    // Each declaration can have one or more IDENTIFIER_DATA nodes
                    node_t *declaration = decl_list->children[i];
                    for (int j = 0; j < declaration->n_children; j++ )
                    {
                        CREATE_AND_INSERT_SYMBOL( local_symbols,
                                          .name = declaration->children[j]->data,
                                          .type = SYMBOL_LOCAL_VAR,
                                          .node = declaration->children[j],
                                          .function_symtable = local_symbols );
                    }
                }
                bind_names ( local_symbols, node->children[1] );
                pop_local_scope ( local_symbols );
            } else {
                // If the block only contains statements, and no declaration list, there is no need to make a scope
                bind_names ( local_symbols, node->children[0] );
            }
            break;

        // Strings get inserted into the global string list
        // The STRING_DATA node gets replaced by a STRING_LIST_REFERENCE node
        case STRING_DATA: {
            size_t position = add_string ( node->data );
            node->type = STRING_LIST_REFERENCE;
            node->data = (void*) position;
            break;
        }

        // For all other nodes, recurse through its children
        default:
            for (int i = 0; i < node->n_children; i++)
                bind_names ( local_symbols, node->children[i] );
            break;
    }
}

/* Creates a new empty hashmap for the symbol table, using the outer scope's hashmap as backup */
static void push_local_scope ( symbol_table_t *table )
{
    symbol_hashmap_t *hashmap = symbol_hashmap_init ( );
    hashmap->backup = table->hashmap;
    table->hashmap = hashmap;
}

/* Destroys the hashmap, and replaces it with the outer scope's hashmap */
static void pop_local_scope ( symbol_table_t *table )
{
    symbol_hashmap_t *hashmap = table->hashmap;
    table->hashmap = hashmap->backup;
    symbol_hashmap_destroy ( hashmap );
}

/* Prints the given symbol table, with sequence number, symbol names and types.
 * When printing function symbols, its local symbol table is recursively printed, with indentation.
 */
static void print_symbol_table ( symbol_table_t *table, int nesting )
{
    for ( int i = 0; i < table->n_symbols; i++ )
    {
        symbol_t *symbol = table->symbols[i];

        printf ( "%*s%ld: %s(%s)\n", nesting*4, "",
                 symbol->sequence_number, SYMBOL_TYPE_NAMES[symbol->type], symbol->name );

        if ( symbol->type == SYMBOL_FUNCTION )
            print_symbol_table ( symbol->function_symtable, nesting + 1 );
    }
}

/* Frees up the memory used by the global symbol table, all local symbol tables, and their symbols */
static void destroy_symbol_tables ( void )
{
    // First destory all local symbol tables, by looking for functions among the globals
    for ( int i = 0; i < global_symbols->n_symbols; i++ )
    {
        if ( global_symbols->symbols[i]->type == SYMBOL_FUNCTION )
            symbol_table_destroy ( global_symbols->symbols[i]->function_symtable );
    }
    // Then destroy the global symbol table
    symbol_table_destroy ( global_symbols );
}

/* Adds the given string to the global string list, resizing if needed.
 * Takes ownership of the string, and returns its position in the string list.
 */
static size_t add_string ( char *string )
{
    if ( string_list_len + 1 >= string_list_capacity ) {
        string_list_capacity = string_list_capacity * 2 + 8;
        string_list = realloc ( string_list, string_list_capacity * sizeof(char*) );
    }
    string_list[string_list_len] = string;
    return string_list_len++;
}

/* Prints all strings added to the global string list */
static void print_string_list ( void )
{
    for ( size_t i = 0; i < string_list_len; i++ )
        printf ( "%ld: %s\n", i, string_list[i] );
}

/* Frees all strings in the global string list, and the string list itself */
static void destroy_string_list ( void )
{
    for ( int i = 0; i < string_list_len; i++ )
        free ( string_list[i] );
    free ( string_list );
}

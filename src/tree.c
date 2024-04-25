#define NODETYPES_IMPLEMENTATION
#include "vslc.h"

// Global root for abstract syntax tree
node_t *root;

// Declarations of internal functions, defined further down
static void node_print ( node_t *node, int nesting );
static void destroy_subtree ( node_t *discard );
static node_t* simplify_subtree ( node_t *node );

// Outputs the entire syntax tree to the terminal
void print_syntax_tree ( void )
{
    if ( getenv("GRAPHVIZ_OUTPUT") != NULL )
        graphviz_node_print ( root );
    else
        node_print ( root, 0 );
}

// Cleans up the entire syntax tree
void destroy_syntax_tree ( void )
{
    destroy_subtree ( root );
    root = NULL;
}

// Modifies the syntax tree, performing constant folding where possible
void simplify_tree ( void )
{
    root = simplify_subtree( root );
}

// Initialize a node with type, data, and children
node_t* node_create ( node_type_t type, void *data, size_t n_children, ... )
{
    node_t* result = malloc ( sizeof ( node_t ) );

    // Initialize every field in the struct
    *result = (node_t) {
        .type = type,
        .n_children = n_children,
        .children = (node_t **) malloc ( n_children * sizeof ( node_t * ) ),

        .data = data,
        .symbol = NULL,
    };

    // Read each child node from the va_list
    va_list child_list;
    va_start ( child_list, n_children );
    for ( size_t i = 0; i < n_children; i++ )
        result->children[i] = va_arg ( child_list, node_t * );
    va_end ( child_list );

    return result;
}

// Append an element to the given LIST node, returns the list node
node_t* append_to_list_node ( node_t* list_node, node_t* element )
{
    assert ( list_node->type == LIST );

    // Calculate the minimum size of the new allocation
    size_t min_allocation_size = list_node->n_children + 1;

    // Round up to the next power of two
    size_t new_allocation_size = 1;
    while ( new_allocation_size < min_allocation_size )
        new_allocation_size *= 2;

    // Resize the allocation
    list_node->children = realloc ( list_node->children, new_allocation_size * sizeof(node_t *) );

    // Insert the new element and increase child count by 1
    list_node->children[list_node->n_children] = element;
    list_node->n_children++;

    return list_node;
}

// Prints out the given node and all its children recursively
static void node_print ( node_t *node, int nesting )
{
    printf ( "%*s", nesting, "" );

    if ( node == NULL )
    {
        printf ( "(NULL)\n");
        return;
    }

    printf ( "%s", node_strings[node->type] );

    // For nodes with extra data, print the data with the correct type
    if ( node->type == IDENTIFIER_DATA ||
         node->type == EXPRESSION ||
         node->type == RELATION ||
         node->type == STRING_DATA)
    {
        printf ( "(%s)", (char *) node->data );
    }
    else if ( node->type == NUMBER_DATA )
    {
        printf ( "(%ld)", *(int64_t *) node->data );
    }
    else if ( node->type == STRING_LIST_REFERENCE )
    {
        // Prints the index of the string in the string_list
        printf ( "(%zu)", (size_t) node->data );
    }

    // If the node has a symbol, print that as well
    if ( node->symbol )
    {
        printf ( " %s(%zu)", SYMBOL_TYPE_NAMES[node->symbol->type], node->symbol->sequence_number );
    }

    putchar ( '\n' );

    // Recursively print children, with some more indentation
    for ( size_t i = 0; i < node->n_children; i++ )
        node_print ( node->children[i], nesting + 1 );
}

// Frees the memory owned by the given node, but does not touch its children
static void node_finalize ( node_t *discard )
{
    if ( discard == NULL )
        return;

    // Only free data if the data field is owned by the node
    switch ( discard->type )
    {
        case IDENTIFIER_DATA:
        case NUMBER_DATA:
        case STRING_DATA:
            free ( discard->data );
        default:
            break;
    }
    free ( discard->children );
    free ( discard );
}

// Recursively frees the memory owned by the given node, and all its children
static void destroy_subtree ( node_t *discard )
{
    if ( discard == NULL )
        return;

     for ( size_t i = 0; i < discard->n_children; i++ )
        destroy_subtree ( discard->children[i] );
     node_finalize ( discard );
}

// Recursively replaces EXPRESSION nodes representing mathematical operations
// where all operands are known integer constants
static node_t* constant_fold_node ( node_t *node )
{
    // Only continue if the node is an expression
    if ( node->type != EXPRESSION )
        return node;

    // Only continue if all children are NUMBER_DATA
    for ( size_t i = 0; i < node->n_children; i++ ) {
        if ( node->children[i]->type != NUMBER_DATA )
            return node;
    }

    char* op = node->data;
    int64_t* result = malloc ( sizeof(int64_t) );

    if ( node->n_children == 1 ) {
        int64_t operand = *(int64_t*) node->children[0]->data;

        if ( strcmp ( op, "-" ) == 0 )
            *result = -operand;
        else
            assert ( false && "Unknown unary operator" );
    }
    else if ( node->n_children == 2 ) {
        int64_t lhs = *(int64_t*) node->children[0]->data;
        int64_t rhs = *(int64_t*) node->children[1]->data;

        if ( strcmp ( op, "+" ) == 0 )
            *result = lhs + rhs;
        else if ( strcmp ( op, "-" ) == 0 )
            *result = lhs - rhs;
        else if ( strcmp ( op, "*" ) == 0 )
            *result = lhs * rhs;
        else if ( strcmp ( op, "/" ) == 0 )
            *result = lhs / rhs;
        else if ( strcmp ( op, "<<" ) == 0 )
            *result = lhs << rhs;
        else if ( strcmp ( op, ">>" ) == 0 )
            *result = lhs >> rhs;
        else
            assert ( false && "Unknown binary operator" );
    }
    else
        assert ( false && "Unknown expression type" );

    // Clean up the old subtree
    destroy_subtree ( node );

    return node_create ( NUMBER_DATA, result, 0);
}

// Recursively replaces multiplication and division by powers of two, with bitshifts
static node_t* peephole_optimize_node ( node_t* node )
{
    if ( node->type != EXPRESSION ||
         node->n_children != 2 ||
         node->children[1]->type != NUMBER_DATA )
        return node;

    char* op = node->data;
    char* new_op;

    if ( strcmp ( op, "*" ) == 0 )
        new_op = "<<";
    else if ( strcmp ( op, "/" ) == 0 )
        new_op = ">>";
    else
        return node;

    int64_t rhs = *(int64_t *) node->children[1]->data;

    // Multiplication and division by 1 is a no-op, return the LHS and destroy the rest
    if ( rhs == 1 ) {
        node_t* lhs_node = node->children[0];
        node->children[0] = NULL;
        destroy_subtree ( node );
        return lhs_node;
    }

    // Only works for positive powers of two
    if ( rhs <= 0 || __builtin_popcount(rhs) != 1 )
        return node;

    int powerOfTwo = 1;
    while (rhs >> powerOfTwo != 1)
        powerOfTwo += 1;

    node->data = new_op;
    *(int64_t*)node->children[1]->data = powerOfTwo;
    return node;
}

static node_t* simplify_subtree( node_t* node )
{
    if ( node == NULL )
        return node;

    // First visit all children
    for ( size_t i = 0; i < node->n_children; i++ )
        node->children[i] = simplify_subtree ( node->children[i] );

    node = constant_fold_node ( node );
    node = peephole_optimize_node ( node );

    return node;
}

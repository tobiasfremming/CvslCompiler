#ifndef TREE_H
#define TREE_H
#include "nodetypes.h"

#include <stdlib.h>

/* This is the tree node structure for the abstract syntax tree */
typedef struct node
{
    node_type_t type;
    struct node** children; // An owned list of pointers to child nodes
    size_t n_children; // The length of the list of child nodes

    void* data; // Extra data, only owned if type ends in _DATA
    struct symbol* symbol;
} node_t;

/* Global root for parse tree and abstract syntax tree */
extern node_t *root;

// The node creation function, needed by the parser
node_t* node_create ( node_type_t type, void *data, size_t n_children, ... );
// Append an element to the given LIST node, returns the list node
node_t* append_to_list_node( node_t* list_node, node_t* element );

void print_syntax_tree ( void );
void destroy_syntax_tree ( void );
void simplify_tree ( void );

// Special function used when syntax trees are output as graphviz graphs.
// Implemented in graphviz_output.c
void graphviz_node_print ( node_t *root );

#endif // TREE_H

#include "vslc.h"

static void graphviz_node_print_internal ( node_t *node ) {
    printf ( "node%p [label=\"%s", node, node_strings[node->type] );
    if ( node->type == IDENTIFIER_DATA || node->type == STRING_DATA || node->type == EXPRESSION || node->type == RELATION ) {
        printf ( "\\n" );
        if ( node->data == NULL ) {
            printf ( "NULL" );
        } else {
            for ( char* c = (char*)node->data; *c != '\0'; c++ ) {
                switch(*c) {
                    case '\\': printf ( "\\\\" ); break;
                    case '"': printf ( "\\\"" ); break;
                    default: putchar ( *c ); break;
                }
            }
        }
    } else if ( node->type == NUMBER_DATA ) {
        printf ( "\\n%ld", *(int64_t*)node->data );
    }
    printf ( "\"];\n" );
    for ( int i = 0; i < node->n_children; i++ ) {
        node_t *child = node->children[i];
        if ( child == NULL )
            printf ( "node%p -- node%pNULL%d ;\n", node, node, i );
        else {
            printf ( "node%p -- node%p ;\n", node, child );
            graphviz_node_print_internal(child);
        }
    }
}

void graphviz_node_print ( node_t *root ) {
    printf ( "graph \"\" {\n node[shape=box];\n" );
    graphviz_node_print_internal ( root );
    printf( "}\n" );
}

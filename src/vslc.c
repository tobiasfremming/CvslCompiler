#include "vslc.h"

#include <getopt.h>

/* Command line option parsing for the main function */
static void options ( int argc, char **argv );
static bool
    print_full_tree = false,
    print_tree_after_simplify = false,
    print_symbol_table_contents = false,
    print_generated_program = false;

/* Entry point */
int main ( int argc, char **argv )
{
    options ( argc, argv );

    yyparse ();       // Generated from grammar/bison, constructs syntax tree
    yylex_destroy (); // Free buffers used by flex

    // Operations in tree.c
    if ( print_full_tree )
        print_syntax_tree ();

    simplify_tree ();
    if ( print_tree_after_simplify )
        print_syntax_tree ();

    // Operations in symbols.c
    create_tables ();
    if ( print_symbol_table_contents )
        print_tables ();

    // Operations in generator.c
    if ( print_generated_program )
        generate_program ();

    destroy_tables ();          // In symbols.c
    destroy_syntax_tree ();     // In tree.c
}

static const char *usage =
"Usage vslc [OPTION...]\n"
"\n"
"Input is read from stdin, output is printed to stdout.\n"
"\n"
"\t-h\tOutput this text and halt\n\n"
"\t-t\tOutput the abstract syntax tree\n"
"\t-T\tOutput the abstract syntax tree after simplification\n"
"\t-s\tOutput the symbol table contents\n"
"\t-c\tCompile and generate assembly output\n";


static void options ( int argc, char **argv )
{
    int o;
    while ( (o=getopt(argc,argv,"htTsc")) != -1 )
    {
        switch ( o )
        {
            case '?': // getopt automatically prints "invalid option"
                      // fallthrough
            case 'h':
                printf ( "%s", usage );
                exit ( EXIT_SUCCESS );
                break;
            case 't':   print_full_tree = true;             break;
            case 'T':   print_tree_after_simplify  = true;  break;
            case 's':   print_symbol_table_contents = true; break;
            case 'c':   print_generated_program = true;     break;
        }
    }

    if ( optind != argc )
    {
        fprintf ( stderr, "%s: invalid positional argument '%s'\n", argv[0], argv[optind] );
        exit ( EXIT_FAILURE );
    }
}

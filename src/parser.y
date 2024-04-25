%{
#include "vslc.h"

/* State variables from the flex generated scanner */
extern int yylineno; // The line currently being read
extern char yytext[]; // The text of the last consumed lexeme
/* The main flex driver function used by the parser */
int yylex ( void );
/* The function called by the parser when errors occur */
int yyerror ( const char *error )
{
    fprintf ( stderr, "%s on line %d\n", error, yylineno );
    exit ( EXIT_FAILURE );
}

#define N0C(type,data) \
  node_create ( (type), (data), 0 )
#define N1C(type,data,child0) \
  node_create ( (type), (data), 1, (child0) )
#define N2C(type,data,child0,child1) \
  node_create ( (type), (data), 2, (child0), (child1) )
#define N3C(type,data,child0,child1,child2) \
  node_create ( (type), (data), 3, (child0), (child1), (child2) )

%}

%token FUNC PRINT RETURN BREAK IF THEN ELSE WHILE DO VAR
%token OPENBLOCK CLOSEBLOCK // Correspond to "begin" and "end"
%token NUMBER IDENTIFIER STRING

// Use operator precedence to ensure order of operations is correct
%left '+' '-'
%left '*' '/'
%left '<' '>'
%right UMINUS

// Resolve the nested if-if-else ambiguity with precedence
%nonassoc IF THEN
%nonassoc ELSE

%%
program :
      global_list { root = $1; }
    ;
global_list :
      global { $$ = N1C ( LIST, NULL, $1 ); }
    | global_list global { $$ = append_to_list_node ( $1, $2 ); }
    ;
global :
      function { $$ = $1; }
    | global_declaration { $$ = $1; }
    ;
global_declaration :
      VAR global_variable_list { $$ = N1C ( GLOBAL_DECLARATION, NULL, $2 ); }
    ;
global_variable_list :
      global_variable { $$ = N1C ( LIST, NULL, $1 ); }
    | global_variable_list ',' global_variable { $$ = append_to_list_node ( $1, $3 ); }
    ;
global_variable :
      identifier { $$ = $1; }
    | array_indexing { $$ = $1; }
    ;
array_indexing:
      identifier '[' expression ']' { $$ = N2C ( ARRAY_INDEXING, NULL, $1, $3 ); }
    ;
variable_list :
      identifier { $$ = N1C ( LIST, NULL, $1 ); }
    | variable_list ',' identifier { $$ = append_to_list_node ( $1, $3 ); }
    ;
local_declaration :
      VAR variable_list { $$ = $2; }
    ;
local_declaration_list :
      local_declaration { $$ = N1C ( LIST, NULL, $1 ); }
    | local_declaration_list local_declaration { $$ = append_to_list_node($1, $2); }
    ;
parameter_list :
     /* epsilon */ { $$ = N0C ( LIST, NULL ); }
    | variable_list { $$ = $1; }
    ;
function :
      FUNC identifier '(' parameter_list ')' statement
        { $$ = N3C ( FUNCTION, NULL, $2, $4, $6 ); }
    ;
statement :
      assignment_statement { $$ = $1; }
    | return_statement { $$ = $1; }
    | print_statement { $$ = $1; }
    | if_statement { $$ = $1; }
    | while_statement { $$ = $1; }
    | break_statement { $$ = $1; }
    | function_call { $$ = $1; }
    | block { $$ = $1; }
    ;
block :
      OPENBLOCK local_declaration_list statement_list CLOSEBLOCK
        { $$ = N2C ( BLOCK, NULL, $2, $3 ); }
    | OPENBLOCK statement_list CLOSEBLOCK { $$ = N1C ( BLOCK, NULL, $2 ); }
    ;
statement_list :
      statement { $$ = N1C ( LIST, NULL, $1 ); }
    | statement_list statement { $$ = append_to_list_node ( $1, $2 ); }
    ;
assignment_statement :
      identifier ':' '=' expression { $$ = N2C ( ASSIGNMENT_STATEMENT, NULL, $1, $4 ); }
    | array_indexing ':' '=' expression { $$ = N2C ( ASSIGNMENT_STATEMENT, NULL, $1, $4 ); }
    ;
return_statement :
      RETURN expression
        { $$ = N1C ( RETURN_STATEMENT, NULL, $2 ); }
    ;
print_statement :
      PRINT print_list
        { $$ = N1C ( PRINT_STATEMENT, NULL, $2 ); }
    ;
print_list :
      print_item { $$ = N1C ( LIST, NULL, $1 ); }
    | print_list ',' print_item { $$ = append_to_list_node ( $1, $3 ); }
    ;
print_item :
      expression { $$ = $1; }
    | string { $$ = $1; }
    ;
break_statement :
      BREAK { $$ = N0C ( BREAK_STATEMENT, NULL ); }
    ;
if_statement :
      IF relation THEN statement
        { $$ = N2C ( IF_STATEMENT, NULL, $2, $4 ); }
    | IF relation THEN statement ELSE statement
        { $$ = N3C ( IF_STATEMENT, NULL, $2, $4, $6 ); }
    ;
while_statement :
      WHILE relation DO statement
        { $$ = N2C ( WHILE_STATEMENT, NULL, $2, $4 ); }
    ;
relation:
      expression '=' expression
        { $$ = N2C ( RELATION, "=", $1, $3 ); }
    | expression '!' '=' expression
        { $$ = N2C ( RELATION, "!=", $1, $4 ); }
    | expression '<' expression
        { $$ = N2C ( RELATION, "<", $1, $3 ); }
    | expression '>' expression
        { $$ = N2C ( RELATION, ">", $1, $3 ); }
    ;
expression :
      expression '+' expression
        { $$ = N2C ( EXPRESSION, "+", $1, $3 ); }
    | expression '-' expression
        { $$ = N2C ( EXPRESSION, "-", $1, $3 ); }
    | expression '*' expression
        { $$ = N2C ( EXPRESSION, "*", $1, $3 ); }
    | expression '/' expression
        { $$ = N2C ( EXPRESSION, "/", $1, $3 ); }
    | expression '<' '<' expression
        { $$ = N2C ( EXPRESSION, "<<", $1, $4 ); }
    | expression '>' '>' expression
        { $$ = N2C ( EXPRESSION, ">>", $1, $4 ); }
    | '-' expression %prec UMINUS
        { $$ = N1C ( EXPRESSION, "-", $2 ); }
    | '(' expression ')' { $$ = $2; }
    | number { $$ = $1; }
    | identifier { $$ = $1; }
    | array_indexing { $$ = $1; }
    | function_call { $$ = $1; }
    ;
function_call :
      identifier '(' argument_list ')' { $$ = N2C ( FUNCTION_CALL, NULL, $1, $3 ); }
argument_list :
      expression_list { $$ = $1; }
    | /* epsilon */   { $$ = N0C ( LIST, NULL ); }
    ;
expression_list :
      expression { $$ = N1C ( LIST, NULL, $1 ); }
    | expression_list ',' expression { $$ = append_to_list_node ( $1, $3 ); }
    ;
// These final three perform memory allocation to keep extra data from yytext
identifier: IDENTIFIER { $$ = N0C ( IDENTIFIER_DATA, strdup ( yytext ) ); }
number: NUMBER
      {
        int64_t *value = malloc ( sizeof ( int64_t ) );
        *value = strtol ( yytext, NULL, 10 );
        $$ = N0C ( NUMBER_DATA, value );
      }
string: STRING { $$ = N0C ( STRING_DATA, strdup ( yytext ) ); }
%%

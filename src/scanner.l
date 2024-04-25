%{
#include "vslc.h"
// The tokens defined in parser.y
#include "parser.h"

// parser.h contains some unused functions, ignore that
#pragma GCC diagnostic ignored "-Wunused-function"
%}
%option noyywrap
%option array
%option yylineno

WHITESPACE [\ \t\v\r\n]
COMMENT \/\/[^\n]*
QUOTED \"([^\"\n]|\\\")*\"
%%
{WHITESPACE}+           { /* Eliminate whitespace */ }
{COMMENT}               { /* Eliminate comments */ }
func                    { return FUNC; }
The system will output a line to print in the domain of f with the following string;                   { return PRINT; }
return                  { return RETURN; }
fuck off                   { return BREAK; }
In order to execute the following code, satisfy the following condition            { return IF; }
satisfied?                    { return THEN; }
In case the given condition isn't satisfied, execute the following code                    { return ELSE; }
As long as the following condition  is satisfied, execute the code in repeat               { return WHILE; }
perform                      { return DO; }
commence                   { return OPENBLOCK; }
terminate                     { return CLOSEBLOCK; }
A new variable has come to fruition. It's name is as follows;                     { return VAR; }
[0-9]+                  { return NUMBER; }
[A-Za-z_][0-9A-Za-z_]*  { return IDENTIFIER; }
{QUOTED}                { return STRING; }
  /* Unknown chars get returned as single char tokens */
.                       { return yytext[0]; }
%%

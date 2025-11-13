> Lexical Analysis with Flex: Split input data into a set of tokens (identifiers, keywords, numbers, brackets, braces, etc.)
> Semantic Parsing with Bison: Generate an AST while parsing the tokens. Bison will do most of the legwork here, we just need to define our AST.
> Assembly with LLVM: This is where we walk over our AST and generate byte/machine code for each node. As crazy as it sounds, this is probably the easiest step.

## Flex
- 3 sections
```
Definitions section
%%
Rules section
%%
User code section
```

## Bison
- 4 main sections
```
%{
Prologue section
%}
Declarations section
%% 
Grammar rules section
%%
Epilogue section
```

## Examples
- exp 1
```
# token.l:

%{
#include <stdio.h>
#include <stdlib.h>
%}

%%
[ \t]+                  ;                       /* 忽略空格与制表符 */
\n                      { printf("EOL\n"); }
"print"                 { printf("KW_PRINT\n"); } /* 关键字需在标识符规则之前 */
[0-9]+                  { printf("NUMBER(%s)\n", yytext); }
[a-zA-Z_][a-zA-Z0-9_]*  { printf("ID(%s)\n", yytext); }
"="                     { printf("ASSIGN(=)\n"); }
[+\-*/]                 { printf("OP(%s)\n", yytext); }
"\("                    { printf("LPAREN\n"); }
"\)"                    { printf("RPAREN\n"); }
.                       { printf("UNKNOWN(%s)\n", yytext); }
%%

int main(void) {
    yylex();    /* 逐个读取并打印所有记号 */
    return 0;
}

int yywrap(void) { return 1; }

# compile:
flex -o token.yy.c token.l
gcc -o token token.yy.c -lfl

# usage:
echo "1 + 2 * 3" | ./token
NUMBER(1)
OP(+)
NUMBER(2)
OP(*)
NUMBER(3)
EOL
```
- exp 2
```
# calc.l:
%{
#include "calc.tab.h"
#include <string.h>
%}

%%
[0-9]+              { yylval.num = atoi(yytext); return NUMBER; }
[a-zA-Z_][a-zA-Z0-9_]* {
                        strncpy(yylval.str, yytext, sizeof(yylval.str));
                        yylval.str[sizeof(yylval.str)-1] = '\0';
                        return IDENTIFIER;
                      }
[ \t]               ;
\n                  return '\n';
.                   return yytext[0];
%%

int yywrap(void) { return 1; }


# calc.y:
%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int yylex(void);
void yyerror(const char *s);

typedef struct {
    char name[64];
    int value;
} symbol_t;

symbol_t symtab[100];
int symcount = 0;

int get_symbol_value(const char *name);
void set_symbol_value(const char *name, int value);
%}

%union {
    int num;
    char str[64];
}

%token <num> NUMBER
%token <str> IDENTIFIER
%type <num> expr assignment

%right '='
%left '+' '-'
%left '*' '/'
%left UMINUS

%%
input:
    /* empty */
  | input line
  ;

line:
    '\n'
  | expr '\n'         { printf("= %d\n", $1); }
  | assignment '\n'   { printf("= %d\n", $1); }
  ;

assignment:
    IDENTIFIER '=' expr   { set_symbol_value($1, $3); $$ = $3; }
  ;

expr:
    NUMBER                              { $$ = $1; }
  | IDENTIFIER                          { $$ = get_symbol_value($1); }
  | expr '+' expr                       { $$ = $1 + $3; }
  | expr '-' expr                       { $$ = $1 - $3; }
  | expr '*' expr                       { $$ = $1 * $3; }
  | expr '/' expr                       { $$ = $1 / $3; }
  | '-' expr %prec UMINUS               { $$ = -$2; }
  | '(' expr ')'                        { $$ = $2; }
  ;

%%

void yyerror(const char *s) { fprintf(stderr, "Error: %s\n", s); }

int get_symbol_value(const char *name) {
    for (int i = 0; i < symcount; i++)
        if (strcmp(symtab[i].name, name) == 0)
            return symtab[i].value;
    fprintf(stderr, "Undefined variable '%s'\n", name);
    return 0;
}

void set_symbol_value(const char *name, int value) {
    for (int i = 0; i < symcount; i++) {
        if (strcmp(symtab[i].name, name) == 0) {
            symtab[i].value = value;
            return;
        }
    }
    strcpy(symtab[symcount].name, name);
    symtab[symcount].value = value;
    symcount++;
}

int main(void) {
    yyparse();
    return 0;
}

# compile:
bison -d calc.y
flex calc.l
gcc -o calc calc.tab.c lex.yy.c -lfl

# usage:
echo "1 + 2 * 3" | ./calc
= 7

./calc
1 + 2
= 3
1 + 2 * 4
= 9
...
```

[Practical parsing with Flex and Bison](https://begriffs.com/posts/2021-11-28-practical-parsing.html)  
[What are Flex and Bison?](http://aquamentus.com/flex_bison.html)  
[GNU Bison](https://www.gnu.org/software/bison/manual/)  
[Writing Your Own Toy Compiler Using Flex, Bison and LLVM](https://gnuu.org/2009/09/18/writing-your-own-toy-compiler/)  
[Flex Bison](https://www.skenz.it/compilers/flex_bison)  
[Parse JSON with Flex and Bison](https://lloydrochester.com/post/flex-bison/json-parse/)  
[Using flex](https://web.mit.edu/gnu/doc/html/flex_toc.html#SEC1)  
[Flex, version 2.5](https://www.cs.princeton.edu/~appel/modern/c/software/flex/flex.html)  
[Bison: The YACC-compatible Parser Generator](https://web.mit.edu/gnu/doc/html/bison_toc.html#SEC7)  
[]()  
[]()  
[]()  
[]()  
[]()  
%{


#include "syntax.tab.h"
#include "node.h"
#include "cmm_strtab.h"
#include <stdio.h>
#include <stdlib.h>

#define Type enum yytokentype

Type lex_handle(Type type);
void ErrorMsg(char *desc, char *lexeme, int lineno);
int yycolumn = 1;
int token_on_line = 0;
extern int is_lex_error;

//
// Wrapper of lex_handle aimed to use LOG
//
#define HANDLE(type) { return lex_handle(type); }

#define YY_USER_ACTION \
yylloc.first_line = yylloc.last_line = yylineno;\
yylloc.first_column = yycolumn;\
yylloc.last_column = yycolumn + yyleng - 1;\
yycolumn += yyleng;

%}


%option yylineno

digit [0-9]
letter [a-zA-Z]

/* READ START */
SEMI ;
COMMA ,
ASSIGNOP =
RELOP >|<|>=|<=|==|!=
PLUS \+
MINUS -
STAR \*
DIV \/
AND &&
OR \|\|
DOT \.
NOT !
TYPE int|float
LP \(
RP \)
LB \[
RB \]
LC \{
RC \}
STRUCT struct
RETURN return
IF if
ELSE else
WHILE while
ID ({letter}|_)({letter}|{digit}|_)*
/* READ END */
HEX 0(x|X)({digit}|{letter})+
OCT 0[0-9]+
INT 0|[1-9][0-9]*
FLOAT [0-9]+\.[0-9]+
WS [[:blank:]]

%%

{SEMI}      HANDLE(SEMI)
{COMMA}     HANDLE(COMMA)
{ASSIGNOP}  HANDLE(ASSIGNOP)
{RELOP}     HANDLE(RELOP)
{PLUS}      HANDLE(PLUS)
{MINUS}     HANDLE(MINUS)
{STAR}      HANDLE(STAR)
{DIV}       HANDLE(DIV)
{AND}       HANDLE(AND)
{OR}        HANDLE(OR)
{DOT}       HANDLE(DOT)
{NOT}       HANDLE(NOT)
{TYPE}      HANDLE(TYPE)
{LP}        HANDLE(LP)
{RP}        HANDLE(RP)
{LB}        HANDLE(LB)
{RB}        HANDLE(RB)
{LC}        HANDLE(LC)
{RC}        HANDLE(RC)
{STRUCT}    HANDLE(STRUCT)
{RETURN}    HANDLE(RETURN)
{IF}        HANDLE(IF)
{ELSE}      HANDLE(ELSE)
{WHILE}     HANDLE(WHILE)
{ID}        HANDLE(ID)
{FLOAT}     {
    yylval.nd = new_node(FLOAT - FLOAT + 3);
    yylval.nd->lineno = yylineno;
    yylval.nd->val.f = atof(yytext);
    token_on_line++;
    return FLOAT;
            }
{HEX}       {
    if (!check_hex(yytext, yyleng, yylineno)) {
        is_syn_error = 1;
    }
    yylval.nd = new_node(INT - FLOAT + 3);
    yylval.nd->lineno = yylineno;
    yylval.nd->val.i = strtol(yytext, NULL, 16);
    token_on_line++;
    return INT;
            }
{OCT}       {
    if (!check_oct(yytext, yyleng, yylineno)) {
        is_syn_error = 1;
    }
    yylval.nd = new_node(INT - FLOAT + 3);
    yylval.nd->lineno = yylineno;
    yylval.nd->val.i = strtol(yytext, NULL, 8);
    token_on_line++;
    return INT;
            }
{INT}       {
    yylval.nd = new_node(INT - FLOAT + 3);
    yylval.nd->lineno = yylineno;
    yylval.nd->val.i = strtol(yytext, NULL, 10);
    token_on_line++;
    return INT;
            }
"\n" {
    token_on_line = 0;
    yycolumn = 1;
}
{WS} {}
. {
    ErrorMsg("Mysterious character", yytext, yylineno);
    is_syn_error = 1;

    return yylex();
}

%%

int check_oct(char *yytext, int yyleng, int yylineno) {
    int i;
    // all zero here is considered oct
    for (i = 0; i < yyleng; i++) {
        char ch = yytext[i];
        if (ch == '8' || ch == '9') {
            ErrorMsg("Illegal octal number", yytext, yylineno);
            is_syn_error = 1;
            return 0;
        }
    }
    return 1;
}

int check_hex(char *yytext, int yyleng, int yylineno) {
    int i;
    // Jump over '0x'
    for (i = 2; i < yyleng; i++) {
        char ch = yytext[i];
        if ((ch > 'f' && ch <= 'z') || (ch > 'F' && ch <= 'Z')) {
            ErrorMsg("Illegal hexadecimal number", yytext, yylineno);
            is_syn_error = 1;
            return 0;
        }
    }
    return 1;
}

//
// lex_handle : the common handle routine
// Param type : the type of the lexeme
// RETURN     : the type of the lexeme 
//
Type lex_handle(Type type) {
    // Currently FLOAT is the first token defined in syntax.y
    // In lexical phase we don't pay attention to non-terminals
    yylval.nd = new_node(type - FLOAT + 3);
    yylval.nd->lineno = yylineno;
    yylval.nd->val.s = register_str(yytext);
    token_on_line++;
    return type;
}

void ErrorMsg(char *desc, char *lexeme, int lineno) {
    printf("Error type A at Line %d: %s '%s'\n", lineno, desc, lexeme);
}

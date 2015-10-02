%{

#include "node.h"
#include "syntax.tab.h"
#include "token.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>

int yycolumn = 1;

#ifndef DEBUG
#define Type enum yytokentype
#else
#define Type enum TOKEN_Type
#endif

Type lex_handle(Type type);

/*
 * Wrapper of lex_handle aimed to use LOG
 */
#define HANDLE(type) { LOG(type); return lex_handle(type); }

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
{HEX}       {
    yylval.nd = new_node(INT - FLOAT + 3);
    yylval.nd->val.i = strtol(yytext, NULL, 16);
    return INT;
            }
{OCT}       {
    yylval.nd = new_node(INT - FLOAT + 3);
    yylval.nd->val.i = strtol(yytext, NULL, 8);
    return INT;
            }
{INT}       {
    yylval.nd = new_node(INT - FLOAT + 3);
    yylval.nd->val.i = strtol(yytext, NULL, 10);
    return INT;
            }
"\n" { yylineno++; yycolumn = 1; }
{WS} {}
. { ErrorMsg("Mysterious character", yytext, yylineno); return 0; }

%%

int check_oct(char *yytext, int yyleng, int yylineno)
{
    int i;
    // all zero here is considered oct
    for (i = 0; i < yyleng; i++) {
        char ch = yytext[i];
        if (ch == '8' || ch == '9') {
            ErrorMsg("Illegal octal number", yytext, yylineno);
            return 0;
        }
    }
    LOG(OCT);
    yylval.nd = (node_t *)malloc(sizeof(node_t));
    yylval.nd->val.i = strtol(yytext, NULL, 16);
    return INT;
}

int check_hex(char *yytext, int yyleng, int yylineno)
{
    int i;
    // Jump over '0x'
    for (i = 2; i < yyleng; i++) {
        char ch = yytext[i];
        if ((ch > 'f' && ch <= 'z') || (ch > 'F' && ch <= 'Z')) {
            ErrorMsg("Illegal hexadecimal number", yytext, yylineno);
            return 0;
        }
    }
    LOG(HEX);
    yylval.nd = (node_t *)malloc(sizeof(node_t));
    yylval.nd->val.i = strtol(yytext, NULL, 8);
    return INT;
}

/*
 * lex_handle : the common handle routine
 * PARAM type : the type of the lexeme
 * RETURN     : the type of the lexeme 
 */
Type lex_handle(Type type)
{
    // Currently FLOAT is the first token defined in syntax.y
    // In lexical phase we don't pay attention to non-terminals
    yylval.nd = new_node(type - FLOAT + 3);
    switch (type) {
        case TYPE: yylval.nd->val.s = strdup(yytext); break;
        case ID: yylval.nd->val.s = strdup(yytext); break;
    }
    return type;
}

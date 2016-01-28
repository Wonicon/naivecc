%option yylineno
%option noyywrap
%{
#include "node.h"
#include "syntax.tab.h"
#include "cmm-strtab.h"
#include <stdio.h>
#include <stdlib.h>


extern int is_lex_error;
extern int is_syn_error;

void ErrorMsg(char *desc, char *lexeme, int lineno);

int yycolumn = 1;

#define YY_USER_ACTION \
yylloc.first_line = yylloc.last_line = yylineno;\
yylloc.first_column = yycolumn;\
yylloc.last_column = yycolumn + yyleng - 1;\
yycolumn += yyleng;

static enum yytokentype num(int radix);
static enum yytokentype op(enum yytokentype type);
static enum yytokentype sym(enum yytokentype type);
static enum yytokentype node(enum yytokentype type, enum ProductionTag tag);
static int check_oct(char *yytext, int yyleng, int yylineno);
static int check_hex(char *yytext, int yyleng, int yylineno);
%}
digit [0-9]
letter [a-zA-Z]

%%

"=" return sym(ASSIGNOP);
"," return sym(COMMA);
";" return sym(SEMI);
"." return sym(DOT);
"(" return sym(LP);
")" return sym(RP);
"[" return sym(LB);
"]" return sym(RB);
"{" return sym(LC);
"}" return sym(RC);
"if" return sym(IF);
"else" return sym(ELSE);
"while" return sym(WHILE);
"struct" return sym(STRUCT);
"return" return sym(RETURN);

"+" return op(PLUS);
"-" return op(MINUS);
"*" return op(STAR);
"/" return op(DIV);
"!" return op(NOT);
"||" return op(OR);
"&&" return op(AND);
>|<|>=|<=|==|!= return op(RELOP);

"int"|"float" return node(TYPE, TERM_ID);
({letter}|_)({letter}|{digit}|_)* return node(ID, TERM_ID);

[0-9]+\.[0-9]+ {
    yylval.nd = new_node();
    yylval.nd->lineno = yylineno;
    yylval.nd->tag = TERM_FLOAT;
    yylval.nd->val.f = atof(yytext);
    return FLOAT;
}

0(x|X)({digit}|{letter})+ {
    if (!check_hex(yytext, yyleng, yylineno)) {
        is_syn_error = 1;
    }
    return num(16);
}
0[0-9]+ {
    if (!check_oct(yytext, yyleng, yylineno)) {
        is_syn_error = 1;
    }
    return num(8);
}
0|[1-9][0-9]* return num(10);

"\n" yycolumn = 1;
[[:blank:]] ;
. {
    ErrorMsg("Mysterious character", yytext, yylineno);
    is_syn_error = 1;
}

%%

static enum yytokentype num(int radix)
{
    yylval.nd = new_node(INT - FLOAT + 3);
    yylval.nd->lineno = yylineno;
    yylval.nd->tag = TERM_INT;
    yylval.nd->val.i = strtol(yytext, NULL, radix);
    return INT;
}

static enum yytokentype sym(enum yytokentype type)
{
    yylval.lineno = yylineno;
    return type;
}

static enum yytokentype op(enum yytokentype type)
{
    yylval.op.lineno = yylineno;
    yylval.op.s = register_str(yytext);
    return type;
}

static enum yytokentype node(enum yytokentype type, enum ProductionTag tag)
{
    yylval.nd = new_node();
    yylval.nd->lineno = yylineno;
    yylval.nd->val.s = register_str(yytext);
    yylval.nd->tag = tag;
    return type;
}

int check_oct(char *yytext, int yyleng, int yylineno)
{
    // all zero here is considered oct
    int i = 0;
    for (; i < yyleng; i++) {
        char ch = yytext[i];
        if (ch == '8' || ch == '9') {
            ErrorMsg("Illegal octal number", yytext, yylineno);
            is_syn_error = 1;
            return 0;
        }
    }
    return 1;
}

int check_hex(char *yytext, int yyleng, int yylineno)
{
    // Jump over '0x'
    int i = 2;
    for (; i < yyleng; i++) {
        char ch = yytext[i];
        if ((ch > 'f' && ch <= 'z') || (ch > 'F' && ch <= 'Z')) {
            ErrorMsg("Illegal hexadecimal number", yytext, yylineno);
            is_syn_error = 1;
            return 0;
        }
    }
    return 1;
}

void ErrorMsg(char *desc, char *lexeme, int lineno)
{
    printf("Error type A at Line %d: %s '%s'\n", lineno, desc, lexeme);
}


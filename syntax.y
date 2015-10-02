%locations
%{
#include "node.h"
static node_t prog;

#define YYDEBUG 1
#include <stdio.h>

#define YY_USER_ACTION\
yylloc.first_line = yylloc.last_line = yylineno;\
yylloc.first_column = yycolumn;\
yylloc.last_column = yycolumn + yyleng - 1;\
yycolumn += yyleng;

#include "lex.yy.c"
%}


/* declared types */
%union {
    node_t *nd;
}

/* declared tokens */
%token <nd>
    INT
    FLOAT
    ID
    ASSIGNOP
    RELOP
    PLUS MINUS STAR DIV
    AND OR NOT
    DOT
    TYPE
    LB RB
    STRUCT
    RETURN
    IF ELSE
    WHILE
%token
    LP RP
    LC RC
    SEMI COMMA

%type <nd>
    Program
    ExtDefList
    ExtDef
    Specifier
    StructSpecifier
    OptTag
    Tag
    VarDec
    FunDec
    ParamDec
    VarList
    CompSt
    StmtList
    Stmt
    DefList
    DecList
    Dec
    Exp
    Args
%%

/* High-level Definitions */

Program : ExtDefList { printf("You should print something!\n"); }
        ;

ExtDefList : ExtDef ExtDefList
           | /* empty */
           ;


ExtDef : Specifier ExtDecList SEMI
       | Specifier SEMI /* int; ??? */
       | Specifier FunDec CompSt
       ;

ExtDecList : VarDec
           | VarDec COMMA ExtDecList
           ;

/* Specifiers */

Specifier : TYPE
          | StructSpecifier
          ;

StructSpecifier : STRUCT OptTag LC DefList RC
                | STRUCT Tag
                ;


/* Because struct {...} is allowed */
OptTag : ID
       | /* empty */
       ;

Tag : ID
    ;

/* Declarators */

VarDec : ID
       | VarDec LB INT RB /* a[8] */
       ;

FunDec : ID LP VarList RP /* func(...) */
       | ID LP RP /* func() */
       ;

ParamDec : Specifier VarDec
         ;

VarList : ParamDec COMMA VarList /* int,int */
        | ParamDec /* only one */
        ;
/* Statements */

CompSt : LC DefList StmtList RC
       ;

StmtList : Stmt StmtList
         | /* empty */
         ;

Stmt : Exp SEMI
     | CompSt
     | RETURN Exp SEMI
     | IF LP Exp RP Stmt
     | IF LP Exp RP Stmt ELSE Stmt
     | WHILE LP Exp RP Stmt
     ;

/* Local Definitions */

DefList : Def DefList
        |
        ;
Def : Specifier DecList SEMI
    ;

DecList : Dec
        | Dec COMMA DecList
        ;
Dec : VarDec { printf("VarDec %s\n", $1->val.s); }
    | VarDec ASSIGNOP Exp { printf("Assignment!\n"); }
    ;

/* Expressions */
Exp : Exp ASSIGNOP Exp { printf("Exp ASSIGN!\n"); }
    | Exp AND Exp
    | Exp OR Exp
    | Exp RELOP Exp
    | Exp PLUS Exp
    | Exp MINUS Exp
    | Exp STAR Exp
    | Exp DIV Exp
    | LP Exp RP
    | MINUS Exp
    | NOT Exp
    | ID LP Args RP
    | ID LP RP
    | Exp LB Exp RB
    | Exp DOT ID
    | ID  { printf("What is ID? %s\n", $1->val.s); }
    | INT { printf("%s:Found INT %d\n", __FUNCTION__, $1->val.i); }
    | FLOAT
    ;
Args : Exp COMMA Args
     | Exp
     ;
%%

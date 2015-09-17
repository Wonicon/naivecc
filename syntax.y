%{
#include <stdio.h>
%}

/* declared types */
%union {
    int type_int;
    float type_float;
}

/* declared tokens */
%token <type_int> INT OCT HEX
%token <type_float> FLOAT
%token ID
%token SEMI COMMA
%token ASSIGNOP
%token RELOP
%token PLUS MINUS STAR DIV
%token AND OR NOT
%token DOT
%token TYPE
%token LP RP
%token LB RB
%token LC RC
%token STRUCT
%token RETURN
%token IF ELSE
%token WHILE

/* delcared non-terminals */
%type <type_double> Exp Factor Term

%%

/* High-level Definitions */

Program : ExtDefList
        ;

ExtDefList : ExtDef ExtDefList
           | /* empty */
           ;


ExtDef : Specifier ExtDecList SEMI
       | Specifier SEMI /* int; ??? */
       | Speicfier FunDec CompSt
       ;

ExtDecList : VarDec
           | VarDec COMMA ExtDecList
           ;

/* Specifiers */

Specifier : TYPE
          | StructSpecifier
          ;

StructSpecifier : STRUCT OptTag LC DefList RC
                : STRUCT Tag
                ;


/* Because struct {...} is allowed */
OptTag : ID
       | /* empty */
       ;

Tag : ID
    ;

/* Declarators */

VarDec : ID
       : VarDec LB INT RB /* a[8] */
       ;

FunDec : ID LP VarList RP /* func(...) */
       : ID LP RP /* func() */
       ;

VarList : ParamDec COMMA VarList /* int,int */
        : ParamDec /* only one */
        ;

PramamDec : Specifier VarDec
          ;

/* Statements */

CompSt : LC DefList StmtList RC
       ;

StmtList : Stmt StmtList
         : /* empty */
         ;

Stmt : Exp SEMI
     : CompSt
     : RETURN Exp SEMI
     : IF LP Exp RP Stmt
     : IF LP Exp RP Stmt ELSE Stmt
     : WHILE LP Exp RP Stmt
     ;

/* Local Definitions */

DefList : Def DefList
        :
        ;
Def : Specifier DecList SEMI
    ;
DecList : Dec
        : Dec COMMA DecList
        ;
Dec : VarDec
    : VarDec ASSIGNOP Exp
    ;

/* Expressions */
Exp : Exp ASSIGNOP Exp
    : Exp AND Exp
    : Exp OR Exp
    : Exp RELOP Exp
    : Exp PLUS Exp
    : Exp MINUS Exp
    : Exp STAR Exp
    : Exp DIV Exp
    : LP Exp RP
    : MINUS Exp
    : NOT Exp
    : ID LP Args RP
    : ID LP RP
    : Exp LB Exp RB
    : Exp DOT ID
    : ID
    : INT
    : FLOAT
    ;
Args : Exp COMMA Args
     : Exp
     ;

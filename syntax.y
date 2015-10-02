%locations
%{
#include "node.h"
#include "yytname.h"
static node_t *prog;

#define YYDEBUG 1
#include <stdio.h>

#define YY_USER_ACTION\
yylloc .first_line = yylloc.last_line = yylineno;\
yylloc .first_column = yycolumn;\
yylloc .last_column = yycolumn + yyleng - 1;\
yycolumn += yyleng;

#include "lex.yy.c"

#define S(x) # x
#define concat(x, y) x ## y
#define name(x) concat(YY_, x)

#define BODY_1(x, NT, BD1) do {\
    NT = new_node(name(x));\
    NT->child = BD1;\
}while(0)

#define BODY_11(x, NT, BD1, BD2) do {\
    NT = new_node(name(x));\
    NT->child = BD1;\
    BD1->sibling = BD2;\
}while(0)

#define BODY_111(x, NT, BD1, BD2, BD3) do {\
    NT = new_node(name(x));\
    NT->child = BD1;\
    BD1->sibling = BD2;\
    BD2->sibling = BD3;\
}while(0)

#define BODY_101(x, NT, BD1, BD2, BD3) do {\
    NT = new_node(name(x));\
    NT->child = BD1;\
    if (BD2 != NULL) {\
        BD1->sibling = BD2;\
        BD2->sibling = BD3;\
    } else {\
        BD1->sibling = BD3;\
    }\
}while(0)

#define BODY_1111(x, NT, BD1, BD2, BD3, BD4) do {\
    NT = new_node(name(x));\
    NT->child = BD1;\
    BD1->sibling = BD2;\
    BD2->sibling = BD3;\
    BD3->sibling = BD4;\
}while(0)

#define BODY_11111(x, NT, BD1, BD2, BD3, BD4, BD5) do {\
    NT = new_node(name(x));\
    NT->child = BD1;\
    BD1->sibling = BD2;\
    BD2->sibling = BD3;\
    BD3->sibling = BD4;\
    BD4->sibling = BD5;\
}while(0)

#define BODY_10101(x, NT, BD1, BD2, BD3, BD4, BD5) do {\
    NT = new_node(name(x));\
    NT->child = BD1;\
    BD1->sibling = BD2 != NULL ? BD2 : BD3;\
    if (BD2 != NULL) BD2->sibling = BD3;\
    BD3->sibling = BD4 != NULL ? BD4 : BD5;\
    if (BD4 != NULL) BD4->sibling = BD5;\
}while(0)

%}


/* declared types */
%union {
    node_t *nd;
}

%token <nd>
/* token start */
    FLOAT
    INT
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
    LP RP
    LC RC
    SEMI COMMA
/* token end */

%type <nd>
    Program
    ExtDef
    ExtDefList
    ExtDecList
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
    Def
    Dec
    Exp
    Args

%%

/* nonterminal start */
/* High-level Definitions */

Program : ExtDefList { BODY_1(Program, $$, $1); prog = $$; }
        ;

ExtDefList : ExtDef ExtDefList { BODY_11(ExtDefList, $$, $1, $2); }
           |                   { $$ = NULL; }
           ;


ExtDef : Specifier ExtDecList SEMI { BODY_111(ExtDef, $$, $1, $2, $3); }
       | Specifier SEMI            { BODY_11(ExtDef, $$, $1, $2); }
       | Specifier FunDec CompSt   { BODY_111(ExtDef, $$, $1, $2, $3); }
       ;

ExtDecList : VarDec                  { BODY_1(ExtDecList, $$, $1); }
           | VarDec COMMA ExtDecList { BODY_111(ExtDecList, $$, $1, $2, $3); }
           ;

/* Specifiers */

Specifier : TYPE            { BODY_1(Specifier, $$, $1); }
          | StructSpecifier { BODY_1(Specifier, $$, $1); }
          ;

StructSpecifier : STRUCT OptTag LC DefList RC { BODY_10101(StructSpecifier, $$, $1, $2, $3, $4, $5); }
                | STRUCT Tag                  { BODY_11(StructSpecifier, $$, $1, $2); }
                ;


/* Because struct {...} is allowed */
OptTag : ID { BODY_1(OptTag, $$, $1); }
       |    { $$ = NULL; }
       ;

Tag : ID { BODY_1(Tag, $$, $1); }
    ;

/* Declarators */

VarDec : ID               { BODY_1(VarDec, $$, $1); }
       | VarDec LB INT RB { BODY_1111(VarDec, $$, $1, $2, $3, $4); }
       ;

FunDec : ID LP VarList RP { BODY_1111(FunDec, $$, $1, $2, $3, $4); }
       | ID LP RP         { BODY_111(FunDec, $$, $1, $2, $3); }
       ;

ParamDec : Specifier VarDec { BODY_11(ParamDec, $$, $1, $2); }
         ;

VarList : ParamDec COMMA VarList { BODY_111(VarList, $$, $1, $2, $3); }
        | ParamDec               { BODY_1(VarList, $$, $1); }
        ;
/* Statements */

CompSt : LC DefList StmtList RC { BODY_1111(CompSt, $$, $1, $2, $3, $4); }
       ;

StmtList : Stmt StmtList { BODY_11(StmtList, $$, $1, $2); }
         |               { $$ = NULL; }
         ;

Stmt : Exp SEMI                     { BODY_11(Stmt, $$, $1, $2); }
     | CompSt                       { BODY_1(Stmt, $$, $1); }
     | RETURN Exp SEMI              { BODY_111(Stmt, $$, $1, $2, $3); }
     | IF LP Exp RP Stmt            { BODY_1111(Stmt, $$, $1, $2, $3, $4); }
     | IF LP Exp RP Stmt ELSE Stmt  { BODY_11111(Stmt, $$, $1, $2, $3, $4, $5); }
     | WHILE LP Exp RP Stmt         { BODY_1111(Stmt, $$, $1, $2, $3, $4); }
     ;

/* Local Definitions */

DefList : Def DefList { BODY_11(DefList, $$, $1, $2); }
        |             { $$ = NULL; }
        ;
Def : Specifier DecList SEMI { BODY_111(Def, $$, $1, $2, $3); }
    ;

DecList : Dec               { BODY_1(DecList, $$, $1); }
        | Dec COMMA DecList { BODY_111(DecList, $$, $1, $2, $3); }
        ;
Dec : VarDec              { BODY_1(Dec, $$, $1); }
    | VarDec ASSIGNOP Exp { BODY_111(Dec, $$, $1, $2, $3); }
    ;

/* Expressions */
Exp : Exp ASSIGNOP Exp { BODY_111(Exp, $$, $1, $2, $3); }
    | Exp AND Exp      { BODY_111(Exp, $$, $1, $2, $3); }
    | Exp OR Exp       { BODY_111(Exp, $$, $1, $2, $3); }
    | Exp RELOP Exp    { BODY_111(Exp, $$, $1, $2, $3); }
    | Exp PLUS Exp     { BODY_111(Exp, $$, $1, $2, $3); }
    | Exp MINUS Exp    { BODY_111(Exp, $$, $1, $2, $3); }
    | Exp STAR Exp     { BODY_111(Exp, $$, $1, $2, $3); }
    | Exp DIV Exp      { BODY_111(Exp, $$, $1, $2, $3); }
    | LP Exp RP        { BODY_111(Exp, $$, $1, $2, $3); }
    | MINUS Exp        { BODY_11(Exp, $$, $1, $2); }
    | NOT Exp          { BODY_11(Exp, $$, $1, $2); }
    | ID LP Args RP    { BODY_1111(Exp, $$, $1, $2, $3, $4); }
    | ID LP RP         { BODY_111(Exp, $$, $1, $2, $3); }
    | Exp LB Exp RB    { BODY_1111(Exp, $$, $1, $2, $3, $4); }
    | Exp DOT ID       { BODY_111(Exp, $$, $1, $2, $3); }
    | ID               { BODY_1(Exp, $$, $1); }
    | INT              { BODY_1(Exp, $$, $1); }
    | FLOAT            { BODY_1(Exp, $$, $1); }
    ;
Args : Exp COMMA Args  { BODY_111(Args, $$, $1, $2, $3); }
     | Exp             { BODY_1(Args, $$, $1); }
     ;
/* nonterminal end */

%%

void midorder(node_t *nd, int level)
{
    if (nd == NULL) return;
    printf("%*s%s", level * 2, "", yytname[nd->type]);
    if (nd->type == YY_TYPE || nd->type == YY_ID) {
        printf(": %s", nd->val.s);
    }
    else if (nd->type == YY_INT) {
        printf(": %d", nd->val.i);
    }
    putchar('\n');
    midorder(nd->child, level + 1);
    midorder(nd->sibling, level);
}

void ast()
{
   midorder(prog, 0); 
}

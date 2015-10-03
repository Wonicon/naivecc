%locations
%{
#include "node.h"
#include "yytname.h"

#define YYDEBUG 1
#include "lex.yy.c"

#include <stdio.h>


#define S(x) # x
#define concat(x, y) x ## y
#define name(x) concat(YY_, x)

#define LINK(x, n) do {\
    yyval.nd = new_node(name(x));\
    yyval.nd->lineno = yyloc.first_line;\
    yyval.nd->child = yyvsp[1 - n].nd;\
    node_t *cur = yyvsp[1 - n].nd;\
    cur->sibling = NULL;\
    int i = 2;\
    while (i <= n) {\
        cur->sibling = yyvsp[i - n].nd;\
        cur = cur->sibling;\
        i++;\
    }\
}while(0)

#define LINK_NULL(x, n) do {\
    yyval.nd = new_node(name(x));\
    yyval.nd->lineno = yyloc.first_line;\
    yyval.nd->child = yyvsp[1 - n].nd;\
    node_t *cur = yyvsp[1 - n].nd;\
    cur->sibling = NULL;\
    int i = 2;\
    while (i <= n) {\
        if (yyvsp[i - n].nd != NULL) {\
            cur->sibling = yyvsp[i - n].nd;\
            cur = cur->sibling;\
        }\
        i++;\
    }\
}while(0)

static node_t *prog;

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

/* Handle ambiguity according to Appendix A */
%right ASSIGNOP
%left OR
%left AND
%left RELOP
%left PLUS MINUS
%left STAR DIV
%right NOT
/* TODO
 * NEG operation is denoted as MINUS token, but we cannot define MINUS's
 * association again, so how to handle the association of NEG which shares
 * the same token as MINUS?
 */
%left LP RP DOT LB RB

%%

/* nonterminal start */
/* High-level Definitions */

Program : ExtDefList { LINK_NULL(Program, 1); prog = $$; }
        ;

ExtDefList : ExtDef ExtDefList { LINK_NULL(ExtDefList, 2); }
           |                   { $$ = NULL; }
           ;


ExtDef : Specifier ExtDecList SEMI { LINK(ExtDef, 3); }
       | Specifier SEMI            { LINK(ExtDef, 2); }
       | Specifier FunDec CompSt   { LINK(ExtDef, 3); }
       ;

ExtDecList : VarDec                  { LINK(ExtDecList, 1); }
           | VarDec COMMA ExtDecList { LINK(ExtDecList, 3); }
           ;

/* Specifiers */

Specifier : TYPE            { LINK(Specifier, 1); }
          | StructSpecifier { LINK(Specifier, 1); }
          ;

StructSpecifier : STRUCT OptTag LC DefList RC { LINK_NULL(StructSpecifier, 5); }
                | STRUCT Tag                  { LINK(StructSpecifier, 2); }
                ;


/* Because struct {...} is allowed */
OptTag : ID { LINK(OptTag, 1); }
       |    { $$ = NULL; }
       ;

Tag : ID { LINK(Tag, 1); }
    ;

/* Declarators */

VarDec : ID               { LINK(VarDec, 1); }
       | VarDec LB INT RB { LINK(VarDec, 4); } 
       ;

FunDec : ID LP VarList RP { LINK(FunDec, 4); }
       | ID LP RP         { LINK(FunDec, 3); }
       ;

ParamDec : Specifier VarDec { LINK(ParamDec, 2); }
         ;

VarList : ParamDec COMMA VarList { LINK(VarList, 3); }
        | ParamDec               { LINK(VarList, 1); }
        ;
/* Statements */

CompSt : LC DefList StmtList RC { LINK_NULL(CompSt, 4); }
       ;

StmtList : Stmt StmtList { LINK_NULL(StmtList, 2); }
         |               { $$ = NULL; }
         ;

Stmt : Exp SEMI                     { LINK(Stmt, 2); }
     | CompSt                       { LINK(Stmt, 1); }
     | RETURN Exp SEMI              { LINK(Stmt, 3); }
     | IF LP Exp RP Stmt            { LINK(Stmt, 5); }
     | IF LP Exp RP Stmt ELSE Stmt  { LINK(Stmt, 6); }
     | WHILE LP Exp RP Stmt         { LINK(Stmt, 5); }
     ;

/* Local Definitions */

DefList : Def DefList { LINK(DefList, 2); }
        |             { $$ = NULL; }
        ;
Def : Specifier DecList SEMI { LINK(Def, 3); }
    ;

DecList : Dec               { LINK(DecList, 1); }
        | Dec COMMA DecList { LINK(DecList, 3); }
        ;
Dec : VarDec              { LINK(Dec, 1); }
    | VarDec ASSIGNOP Exp { LINK(Dec, 3); }
    ;

/* Expressions */
Exp : Exp ASSIGNOP Exp { LINK(Exp, 3); }
    | Exp AND Exp      { LINK(Exp, 3); }
    | Exp OR Exp       { LINK(Exp, 3); }
    | Exp RELOP Exp    { LINK(Exp, 3); }
    | Exp PLUS Exp     { LINK(Exp, 3); }
    | Exp MINUS Exp    { LINK(Exp, 3); }
    | Exp STAR Exp     { LINK(Exp, 3); }
    | Exp DIV Exp      { LINK(Exp, 3); }
    | LP Exp RP        { LINK(Exp, 3); }
    | MINUS Exp        { LINK(Exp, 2); }
    | NOT Exp          { LINK(Exp, 2); }
    | ID LP Args RP    { LINK(Exp, 4); }
    | ID LP RP         { LINK(Exp, 3); }
    | Exp LB Exp RB    { LINK(Exp, 4); }
    | Exp DOT ID       { LINK(Exp, 3); }
    | ID               { LINK(Exp, 1); }
    | INT              { LINK(Exp, 1); }
    | FLOAT            { LINK(Exp, 1); }
    ;
Args : Exp COMMA Args  { LINK(Args, 3); }
     | Exp             { LINK(Args, 1); }
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
    if (nd->type > YYNTOKENS) {
        printf(" (%d)", nd->lineno);
    }
    putchar('\n');
    midorder(nd->child, level + 1);
    midorder(nd->sibling, level);
}

void ast()
{
   midorder(prog, 0); 
}

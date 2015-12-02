%locations
%error-verbose
%{

#include "yytname.h"
#include "node.h"
#include "cmm-type.h"
#include "cmm-symtab.h"
#include "lib.h"
#include <assert.h>

int is_lex_error = 0;
int is_syn_error = 0;
extern int is_greedy;

#define YYDEBUG 1
#include "lex.yy.c"

#include <stdio.h>
static Node prog;
static union YYSTYPE *YYVSP = NULL;
#define S(x) # x
#define concat(x, y) x ## y
#define name(x) concat(YY_, x)

#define LINK(x, n) do {\
    YYVSP = yyvsp;\
    yyval.nd = new_node(name(x));\
    yyval.nd->lineno = yyloc.first_line;\
    yyval.nd->val.s = yytname[name(x)];\
    yyval.nd->child = yyvsp[1 - n].nd;\
    Node cur = yyvsp[1 - n].nd;\
    cur->sibling = NULL;\
    int i = 2;\
    while (i <= n) {\
        cur->sibling = yyvsp[i - n].nd;\
        cur = cur->sibling;\
        i++;\
    }\
    prog = yyval.nd;\
}while(0)

#define LINK_NULL(x, n) do {\
    YYVSP = yyvsp;\
    yyval.nd = new_node(name(x));\
    yyval.nd->lineno = yyloc.first_line;\
    yyval.nd->val.s = yytname[name(x)];\
    yyval.nd->child = yyvsp[1 - n].nd;\
    Node cur = yyvsp[1 - n].nd;\
    if (cur == NULL) break;\
    cur->sibling = NULL;\
    int i = 2;\
    while (i <= n) {\
        if (yyvsp[i - n].nd != NULL) {\
            cur->sibling = yyvsp[i - n].nd;\
            cur = cur->sibling;\
        }\
        i++;\
    }\
    prog = yyval.nd;\
}while(0)

//#define LINK LINK_NULL

#define _str(x) # x
//#define ERR
#ifdef ERR
#define LOGERR(x) do { printf("Hit " _str(x) "\n"); is_lex_error = 0; if (is_greedy) yyerrok; } while (0)
#else // !ERR
#define LOGERR(x) is_lex_error = 0; if (is_greedy) yyerrok
#endif

//
// Pre-declaration
//
int yyerror(const char *msg);
void midorder(Node, int);

%}

/* declared types */
%union {
    Node nd;
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
    IF ELSE SUB_ELSE
    WHILE
    LP RP
    LC RC
    SEMI COMMA
    UND
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

/* Handle shift/reduce conflict of if-if-else */
%nonassoc SUB_ELSE /* a fake token used to define the precedence of a production */
%nonassoc ELSE     /* use nonassoc to define precedence only */

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
 * the same token with MINUS?
 */
%left LP RP DOT LB RB

%%

/* nonterminal start */
/* High-level Definitions */

Program         : ExtDefList { LINK_NULL(Program, 1); prog = $$; }
                ;

ExtDefList      : ExtDef ExtDefList { LINK_NULL(ExtDefList, 2); }
                |                   { $$ = NULL; }
                ;


ExtDef          : Specifier ExtDecList SEMI  { LINK(ExtDef, 3); }
                | Specifier SEMI             { LINK(ExtDef, 2); }
                | Specifier FunDec CompSt    { LINK(ExtDef, 3); }
                ;

ExtDecList      : VarDec                  { LINK(ExtDecList, 1); }
                | VarDec COMMA ExtDecList { LINK(ExtDecList, 3); }
                ;

/* Specifiers */

Specifier       : TYPE { LINK(Specifier, 1); }
                | StructSpecifier { LINK(Specifier, 1); }
                ;

StructSpecifier : STRUCT OptTag LC DefList RC { LINK_NULL(StructSpecifier, 5); }
                | STRUCT Tag                  { LINK(StructSpecifier, 2); }
                ;

OptTag          : ID { LINK(OptTag, 1); }
                |    { $$ = NULL; }
                ;

Tag             : ID { LINK(Tag, 1); }
                ;

/* Declarators */

VarDec          : ID               { LINK(VarDec, 1); }
                | VarDec LB INT RB { LINK(VarDec, 4); }
                ;

FunDec          : ID LP VarList RP { LINK(FunDec, 4); }
                | ID LP RP         { LINK(FunDec, 3); }
                ;

ParamDec        : Specifier VarDec { LINK(ParamDec, 2); }
                ;

VarList         : ParamDec COMMA VarList { LINK(VarList, 3); }
                | ParamDec               { LINK(VarList, 1); }
                ;

/* Statements */

CompSt          : LC DefList StmtList RC { LINK_NULL(CompSt, 4); }
                ;

StmtList        : Stmt StmtList { LINK_NULL(StmtList, 2); }
                |               { $$ = NULL; }
                ;

Stmt            : Exp SEMI                         { LINK(Stmt, 2); }
                | CompSt                           { LINK(Stmt, 1); }
                | RETURN Exp SEMI                  { LINK(Stmt, 3); }
                | IF LP Exp RP Stmt %prec SUB_ELSE { LINK(Stmt, 5); }
                | IF LP Exp RP Stmt ELSE Stmt      { LINK(Stmt, 7); }
                | WHILE LP Exp RP Stmt             { LINK(Stmt, 5); }
                ;

/* Local Definitions */

DefList         : Def DefList { LINK(DefList, 2); }
                |             { $$ = NULL; }
                ;

Def             : Specifier DecList SEMI  { LINK(Def, 3); }
                | Specifier error SEMI    { LOGERR(def: spec err semi); }
                ;

DecList         : Dec               { LINK(DecList, 1); }
                | Dec COMMA DecList { LINK(DecList, 3); }
                ;

Dec             : VarDec              { LINK(Dec, 1); }
                | VarDec ASSIGNOP Exp { LINK(Dec, 3);  }
                ;

/* Expressions */

Exp             : Exp ASSIGNOP Exp { LINK(Exp, 3); }
                | Exp error ID     {LOGERR(Exp -> Exp error ID);}
                | Exp error INT    {LOGERR(Exp -> Exp error INT);}
                | Exp error FLOAT  {LOGERR(Exp -> Exp error FLOAT);}
                | Exp AND Exp      { LINK(Exp, 3); }
                | Exp OR Exp       { LINK(Exp, 3); }
                | Exp RELOP Exp    { LINK(Exp, 3); }
                | Exp PLUS Exp     { LINK(Exp, 3); }
                | Exp MINUS Exp    { LINK(Exp, 3); }
                | Exp STAR Exp     { LINK(Exp, 3); }
                | Exp DIV Exp      { LINK(Exp, 3); }
                | Exp LB Exp RB    { LINK(Exp, 4); }
                | Exp DOT ID       { LINK(Exp, 3); }
                | LP Exp RP        { LINK(Exp, 3); }
                | MINUS Exp        { LINK(Exp, 2); }  /* Conflict with Exp error ID... */
                | NOT Exp          { LINK(Exp, 2); }
                | ID LP Args RP    { LINK(Exp, 4); }
                | ID LP RP         { LINK(Exp, 3); }
                | ID               { LINK(Exp, 1); }
                | INT              { LINK(Exp, 1); }
                | FLOAT            { LINK(Exp, 1); }
                ;

Args            : Exp COMMA Args  { LINK(Args, 3); }
                | Exp             { LINK(Args, 1); }
                ;

/* nonterminal end */

%%


//
// Get the string store in code generated by bison
// It is a wrapper of `static char *yytname[]'
//
const char *get_token_name(int type_code) {
    return yytname[type_code];
}


//
// if type_code refers to a terminal, then return true(1)
// else return 0
//
int is_terminal(int type_code) {
    return type_code < YYNTOKENS;
}


//
// Print the parsing tree
//
void ast() {
    puts_tree(prog);
}

//
// Analyze the parsing tree
//
void semantic_analysis() {
    analyze_program(prog);
}

//
// 简化语法分析树
//
Node simplify_tree(const Node);
Node ast_tree;
void simplify() {
    ast_tree = simplify_tree(prog);
}
//
// Release the parsing tree
//
void free_ast() {
    free_node(prog);
}


//
// oeverride of yyerror
//
int yyerror(const char *msg) {
    is_syn_error = 1;
    if (is_lex_error) {
        is_lex_error = 0;
        return 0;
    }

    printf("Error type B at line %d: %s.\n", yylineno, msg);
    return 0;
}

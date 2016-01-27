%locations
%error-verbose
%{

#include "yytname.h"
#include "node.h"
#include "cmm-type.h"
#include "cmm-symtab.h"
#include "lib.h"
#include "ast.h"
#include <assert.h>

int is_lex_error = 0;
int is_syn_error = 0;

#define YYDEBUG 1
#include "lex.yy.c"

#include <stdio.h>
Node prog;
static union YYSTYPE *YYVSP = NULL;

#define S(x) # x
#define concat(x, y) x ## y
#define name(x) concat(YY_, x)
#define _str(x) # x

#define LINK(x, y)
#define LINK_NULL(x, y)

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

Program         : ExtDefList { prog = create_tree(PROG_is_EXTDEF, $1->lineno, $1); }
                ;

ExtDefList      : ExtDef ExtDefList { $1->sibling = $2; $$ = $1; }
                |                   { $$ = NULL; }
                ;


ExtDef          : Specifier ExtDecList SEMI  { $$ = create_tree(EXTDEF_is_SPEC_EXTDEC, $1->lineno, $1, $2); }
                | Specifier SEMI             { $$ = create_tree(EXTDEF_is_SPEC, $1->lineno, $1); }
                | Specifier FunDec CompSt    { $$ = create_tree(EXTDEF_is_SPEC_FUNC_COMPST, $1->lineno, $1, $2, $3); }
                ;

ExtDecList      : VarDec                  { $$ = create_tree(EXTDEC_is_VARDEC, $1->lineno, $1); }
                | VarDec COMMA ExtDecList { $1->sibling = $3; $$ = $1; }
                ;

/* Specifiers */

Specifier       : TYPE            { $$ = create_tree(SPEC_is_TYPE, $1->lineno, $1); }
                | StructSpecifier { $$ = create_tree(SPEC_is_STRUCT, $1->lineno, $1); }
                ;

StructSpecifier : STRUCT ID LC DefList RC { $$ = create_tree(STRUCT_is_ID_DEF, $1->lineno, $2, $4); }
                | STRUCT LC DefList RC    { $$ = create_tree(STRUCT_is_DEF, $1->lineno, $3); }
                | STRUCT ID               { $$ = create_tree(STRUCT_is_ID, $1->lineno, $2); }
                ;

/* Declarators */

VarDec          : ID               { $$ = create_tree(VARDEC_is_ID, $1->lineno, $1); }
                | VarDec LB INT RB { $$ = create_tree(VARDEC_is_VARDEC_SIZE, $1->lineno, $1, $3); }
                ;

FunDec          : ID LP VarList RP { $$ = create_tree(FUNC_is_ID_VAR, $1->lineno, $1, $3); }
                | ID LP RP         { $$ = create_tree(FUNC_is_ID_VAR, $1->lineno, $1, NULL); }
                ;

ParamDec        : Specifier VarDec { $$ = create_tree(VAR_is_SPEC_VARDEC, $1->lineno, $1, $2); }
                ;

VarList         : ParamDec COMMA VarList { $1->sibling = $3; $$ = $1; }
                | ParamDec               { $$ = $1; }
                ;

/* Statements */

CompSt          : LC DefList StmtList RC { $$ = create_tree(COMPST_is_DEF_STMT, $1->lineno, $2, $3); }
                ;

StmtList        : Stmt StmtList { $1->sibling = $2; $$ = $1; }
                |               { $$ = NULL; }
                ;

Stmt            : Exp SEMI                         { $$ = create_tree(STMT_is_EXP, $1->lineno, $1); }
                | CompSt                           { $$ = create_tree(STMT_is_COMPST, $1->lineno, $1); }
                | RETURN Exp SEMI                  { $$ = create_tree(STMT_is_RETURN, $1->lineno, $2); }
                | IF LP Exp RP Stmt %prec SUB_ELSE { $$ = create_tree(STMT_is_IF, $1->lineno, $3, $5); }
                | IF LP Exp RP Stmt ELSE Stmt      { $$ = create_tree(STMT_is_IF_ELSE, $1->lineno, $3, $5, $7); }
                | WHILE LP Exp RP Stmt             { $$ = create_tree(STMT_is_WHILE, $1->lineno, $3, $5); }
                ;

/* Local Definitions */

DefList         : Def DefList { $1->sibling = $2; $$ = $1; }
                |             { $$ = NULL; }
                ;

Def             : Specifier DecList SEMI { $$ = create_tree(DEF_is_SPEC_DEC, $1->lineno, $1, $2); }
                ;

DecList         : Dec               { $$ = $1; }
                | Dec COMMA DecList { $1->sibling = $3; $$ = $1; }
                ;

Dec             : VarDec              { $$ = create_tree(DEC_is_VARDEC, $1->lineno, $1); }
                | VarDec ASSIGNOP Exp { $$ = create_tree(DEC_is_VARDEC_INITIALIZATION, $1->lineno, $1, $3); }
                ;

/* Expressions */

Exp             : Exp ASSIGNOP Exp { $$ = create_tree(EXP_is_ASSIGN, $2->lineno, $1, $3); }
                | Exp AND Exp      { $$ = create_tree(EXP_is_AND, $2->lineno, $1, $3); }
                | Exp OR Exp       { $$ = create_tree(EXP_is_OR, $2->lineno, $1, $3); }
                | Exp RELOP Exp    { $$ = create_tree(EXP_is_RELOP, $2->lineno, $1, $3); $$->val.operator = $2->val.s; }
                | Exp PLUS Exp     { $$ = create_tree(EXP_is_BINARY, $2->lineno, $1, $3); $$->val.operator = $2->val.s;}
                | Exp MINUS Exp    { $$ = create_tree(EXP_is_BINARY, $2->lineno, $1, $3); $$->val.operator = $2->val.s;}
                | Exp STAR Exp     { $$ = create_tree(EXP_is_BINARY, $2->lineno, $1, $3); $$->val.operator = $2->val.s;}
                | Exp DIV Exp      { $$ = create_tree(EXP_is_BINARY, $2->lineno, $1, $3); $$->val.operator = $2->val.s;}
                | Exp LB Exp RB    { $$ = create_tree(EXP_is_EXP_IDX, $2->lineno, $1, $3); $$->val.operator = $2->val.s;}
                | Exp DOT ID       { $$ = create_tree(EXP_is_EXP_FIELD, $2->lineno, $1, $3); }
                | LP Exp RP        { $$ = $2; }
                | MINUS Exp        { $$ = create_tree(EXP_is_UNARY, $1->lineno, $2); $$->val.operator = $1->val.s;}
                | NOT Exp          { $$ = create_tree(EXP_is_UNARY, $1->lineno, $2); $$->val.operator = $1->val.s;}
                | ID LP Args RP    { $$ = create_tree(EXP_is_ID_ARG, $1->lineno, $1, $3); }
                | ID LP RP         { $$ = create_tree(EXP_is_ID_ARG, $1->lineno, $1, NULL); }
                | ID               { $$ = create_tree(EXP_is_ID, $1->lineno, $1); }
                | INT              { $$ = create_tree(EXP_is_INT, $1->lineno, $1); }
                | FLOAT            { $$ = create_tree(EXP_is_FLOAT, $1->lineno, $1); }
                ;

Args            : Exp COMMA Args  { $1->sibling = $3; $$ = $1; }
                | Exp             { $$ = $1; }
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


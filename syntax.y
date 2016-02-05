%locations
%error-verbose
%{
#include "node.h"
#include "cmm-symtab.h"
#include "lib.h"
#include "ast.h"
#include <stdio.h>

#define YYDEBUG 1

extern int yylineno;

int is_syn_error = 0;
Node prog;
%}

/* declared types */
%union {
    Node nd;
    int lineno;
    struct {
        int lineno;
        const char *s;
    } op;
}

%token <nd>
    FLOAT
    INT
    ID
    TYPE

%token <op>
    RELOP
    PLUS MINUS STAR DIV
    AND OR NOT

%token <lineno>
    DOT
    LB RB
    LP RP
    LC RC
    FOR WHILE
    STRUCT
    RETURN
    ASSIGNOP
    SEMI COMMA
    IF ELSE SUB_ELSE

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

StructSpecifier : STRUCT ID LC DefList RC { $$ = create_tree(STRUCT_is_ID_DEF, $1, $2, $4); }
                | STRUCT LC DefList RC    { $$ = create_tree(STRUCT_is_DEF, $1, $3); }
                | STRUCT ID               { $$ = create_tree(STRUCT_is_ID, $1, $2); }
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
                | ParamDec
                ;

/* Statements */

CompSt          : LC DefList StmtList RC { $$ = create_tree(COMPST_is_DEF_STMT, $1, $2, $3); }
                ;

StmtList        : Stmt StmtList { $1->sibling = $2; $$ = $1; }
                |               { $$ = NULL; }
                ;

Stmt            : Exp SEMI                             { $$ = create_tree(STMT_is_EXP, $1->lineno, $1); }
                | CompSt                               { $$ = create_tree(STMT_is_COMPST, $1->lineno, $1); }
                | RETURN Exp SEMI                      { $$ = create_tree(STMT_is_RETURN, $1, $2); }
                | IF LP Exp RP Stmt %prec SUB_ELSE     { $$ = create_tree(STMT_is_IF, $1, $3, $5); }
                | IF LP Exp RP Stmt ELSE Stmt          { $$ = create_tree(STMT_is_IF_ELSE, $1, $3, $5, $7); }
                | WHILE LP Exp RP Stmt                 { $$ = create_tree(STMT_is_WHILE, $1, $3, $5); }
                | FOR LP Exp SEMI Exp SEMI Exp RP Stmt { $$ = create_tree(STMT_is_FOR, $1, $3, $5, $7, $9); }
                ;

/* Local Definitions */

DefList         : Def DefList { $1->sibling = $2; $$ = $1; }
                |             { $$ = NULL; }
                ;

Def             : Specifier DecList SEMI { $$ = create_tree(DEF_is_SPEC_DEC, $1->lineno, $1, $2); }
                ;

DecList         : Dec
                | Dec COMMA DecList { $1->sibling = $3; $$ = $1; }
                ;

Dec             : VarDec              { $$ = create_tree(DEC_is_VARDEC, $1->lineno, $1); }
                | VarDec ASSIGNOP Exp { $$ = create_tree(DEC_is_VARDEC_INITIALIZATION, $1->lineno, $1, $3); }
                ;

/* Expressions */

Exp             : Exp ASSIGNOP Exp { $$ = create_tree(EXP_is_ASSIGN, $2, $1, $3); }
                | Exp AND Exp      { $$ = create_tree(EXP_is_AND, $2.lineno, $1, $3); }
                | Exp OR Exp       { $$ = create_tree(EXP_is_OR, $2.lineno, $1, $3); }
                | Exp RELOP Exp    { $$ = create_tree(EXP_is_RELOP, $2.lineno, $1, $3); $$->val.operator = $2.s; }
                | Exp PLUS Exp     { $$ = create_tree(EXP_is_BINARY, $2.lineno, $1, $3); $$->val.operator = $2.s; }
                | Exp MINUS Exp    { $$ = create_tree(EXP_is_BINARY, $2.lineno, $1, $3); $$->val.operator = $2.s; }
                | Exp STAR Exp     { $$ = create_tree(EXP_is_BINARY, $2.lineno, $1, $3); $$->val.operator = $2.s; }
                | Exp DIV Exp      { $$ = create_tree(EXP_is_BINARY, $2.lineno, $1, $3); $$->val.operator = $2.s; }
                | Exp LB Exp RB    { $$ = create_tree(EXP_is_EXP_IDX, $2, $1, $3); }
                | Exp DOT ID       { $$ = create_tree(EXP_is_EXP_FIELD, $2, $1, $3); }
                | LP Exp RP        { $$ = $2; }
                | MINUS Exp        { $$ = create_tree(EXP_is_UNARY, $1.lineno, $2); $$->val.operator = $1.s;}
                | NOT Exp          { $$ = create_tree(EXP_is_UNARY, $1.lineno, $2); $$->val.operator = $1.s;}
                | ID LP Args RP    { $$ = create_tree(EXP_is_ID_ARG, $1->lineno, $1, $3); }
                | ID LP RP         { $$ = create_tree(EXP_is_ID_ARG, $1->lineno, $1, NULL); }
                | ID               { $$ = create_tree(EXP_is_ID, $1->lineno, $1); }
                | INT              { $$ = create_tree(EXP_is_INT, $1->lineno, $1); }
                | FLOAT            { $$ = create_tree(EXP_is_FLOAT, $1->lineno, $1); }
                ;

Args            : Exp COMMA Args  { $1->sibling = $3; $$ = $1; }
                | Exp
                ;

/* nonterminal end */

%%

//
// Analyze the parsing tree
//
void semantic_analysis()
{
    analyze_program(prog);
}

//
// Release the parsing tree
//
void free_ast()
{
    free_node(prog);
}


//
// oeverride of yyerror
//
int yyerror(const char *msg)
{
    is_syn_error = 1;
    printf("Error type B at line %d: %s.\n", yylineno, msg);
    return 0;
}


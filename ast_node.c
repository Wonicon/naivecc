//
// Created by whz on 15-11-18.
//

#include "ast_node.h"
#include <stdlib.h>
#include <string.h>

ExtDef_Type *new_ExtDef_Type() {
    ExtDef_Type *p = (ExtDef_Type *)malloc(sizeof(ExtDef_Type));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_ExtDef_Type;
    return p;
}

Type_Struct *new_Type_Struct() {
    Type_Struct *p = (Type_Struct *)malloc(sizeof(Type_Struct));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_Type_Struct;
    return p;
}

ExtDef_Global *new_ExtDef_Global() {
    ExtDef_Global *p = (ExtDef_Global *)malloc(sizeof(ExtDef_Global));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_ExtDef_Global;
    return p;
}

Func_Param *new_Func_Param() {
    Func_Param *p = (Func_Param *)malloc(sizeof(Func_Param));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_Func_Param;
    return p;
}

ExtDef_Function *new_ExtDef_Function() {
    ExtDef_Function *p = (ExtDef_Function *)malloc(sizeof(ExtDef_Function));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_ExtDef_Function;
    return p;
}

Stmt_Def *new_Stmt_Def() {
    Stmt_Def *p = (Stmt_Def *)malloc(sizeof(Stmt_Def));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_Stmt_Def;
    return p;
}

Stmt_CompSt *new_Stmt_CompSt() {
    Stmt_CompSt *p = (Stmt_CompSt *)malloc(sizeof(Stmt_CompSt));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_Stmt_CompSt;
    return p;
}

Stmt_Return *new_Stmt_Return() {
    Stmt_Return *p = (Stmt_Return *)malloc(sizeof(Stmt_Return));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_Stmt_Return;
    return p;
}

Stmt_If *new_Stmt_If() {
    Stmt_If *p = (Stmt_If *)malloc(sizeof(Stmt_If));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_Stmt_If;
    return p;
}

Stmt_If_Else *new_Stmt_If_Else() {
    Stmt_If_Else *p = (Stmt_If_Else *)malloc(sizeof(Stmt_If_Else));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_Stmt_If_Else;
    return p;
}

Stmt_While *new_Stmt_While() {
    Stmt_While *p = (Stmt_While *)malloc(sizeof(Stmt_While));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_Stmt_While;
    return p;
}

Exp_And *new_Exp_And() {
    Exp_And *p = (Exp_And *)malloc(sizeof(Exp_And));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_Exp_And;
    return p;
}

Exp_Or *new_Exp_Or() {
    Exp_Or *p = (Exp_Or *)malloc(sizeof(Exp_Or));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_Exp_Or;
    return p;
}

Exp_Relop *new_Exp_Relop() {
    Exp_Relop *p = (Exp_Relop *)malloc(sizeof(Exp_Relop));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_Exp_Relop;
    return p;
}

Exp_Calc *new_Exp_Calc() {
    Exp_Calc *p = (Exp_Calc *)malloc(sizeof(Exp_Calc));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_Exp_Calc;
    return p;
}

Exp_Neg *new_Exp_Neg() {
    Exp_Neg *p = (Exp_Neg *)malloc(sizeof(Exp_Neg));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_Exp_Neg;
    return p;
}

Exp_Call *new_Exp_Call() {
    Exp_Call *p = (Exp_Call *)malloc(sizeof(Exp_Call));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_Exp_Call;
    return p;
}

Exp_Index *new_Exp_Index() {
    Exp_Index *p = (Exp_Index *)malloc(sizeof(Exp_Index));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_Exp_Index;
    return p;
}

Exp_Access *new_Exp_Access() {
    Exp_Access *p = (Exp_Access *)malloc(sizeof(Exp_Access));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_Exp_Access;
    return p;
}

Exp_ID *new_Exp_ID() {
    Exp_ID *p = (Exp_ID *)malloc(sizeof(Exp_ID));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_Exp_ID;
    return p;
}

Exp_Const *new_Exp_Const() {
    Exp_Const *p = (Exp_Const *)malloc(sizeof(Exp_Const));
    memset(p, 0, sizeof(*p));
    p->tag = AST_TYPE_Exp_Const;
    return p;
}



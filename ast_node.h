//
// Created by whz on 15-11-18.
//

//
// 这个头文件用于定于抽象语法结点.
// 抽象语法结点是语法分析结点的简化版本,
// 抽象语法结点不包含无用的关键字结点, 并且一个结点类型对应一个产生式.
// 这样可以方便基于产生式的语义分析.
// 语法结点的简化规则如下:
//   1. 关键字和语法标志省略
//   2. List类由二叉树转化为一维链表
//   3. 只是换个名字的产生式直接过滤: Program, Tag
//

#ifndef __AST_NODE_H__
#define __AST_NODE_H__

#include "cmm_type.h"

//
// AstNodeTag 抽象语法树结点标签, 用于在泛型指针中区分不同的抽象语法结点.
//

typedef enum _AstNodeTag {
    AST_TYPE_ExtDef_Type,
    AST_TYPE_Type_Struct,
    AST_TYPE_ExtDef_Global,
    AST_TYPE_Func_Param,
    AST_TYPE_ExtDef_Function,
    AST_TYPE_Stmt_Def,
    AST_TYPE_Stmt_CompSt,
    AST_TYPE_Stmt_Return,
    AST_TYPE_Stmt_If,
    AST_TYPE_Stmt_If_Else,
    AST_TYPE_Stmt_While,
    AST_TYPE_Exp_And,
    AST_TYPE_Exp_Or,
    AST_TYPE_Exp_Relop,
    AST_TYPE_Exp_Calc,
    AST_TYPE_Exp_Neg,
    AST_TYPE_Exp_Call,
    AST_TYPE_Exp_Index,
    AST_TYPE_Exp_Access,
    AST_TYPE_Exp_ID,
    AST_TYPE_Exp_Const,
} AstNodeTag;


//
// AstNode 泛型. 所有的 AstNode 指针强制类型转换该泛型指针以方便统一存储.
// 使用访问者模式对各 AstNode 进行访问时, 首先通过重载路由函数考察 AstNode 的标签, 然后选择正确的访问者函数.
//

typedef struct _AstNode {
    AstNodeTag tag;
    struct _AstNode *link;
} AstNode;

//
// 具体的抽象语法树结点类型
//

// 类型构造
typedef struct _ExtDef_Type {
    AstNodeTag tag;
    AstNode *link;
    AstNode *type;  // 指向具体的类型
} ExtDef_Type;

// 结构体类型构造
typedef struct _Type_Struct {
    AstNodeTag tag;
    AstNode *link;
    const char *name;  // 可以为 NULL, 此时表示匿名结构体
    AstNode *Def;      // 域声明链
} Type_Struct;

// 全局变量定义
typedef struct _ExtDef_Global {
    AstNodeTag tag;
    AstNode *link;
    AstNode *spec;     // 类型
    const char *name;  // 变量名
    int dimention;     // 维度, 普通变量为 0 维
    int *sz;           // 每个维度的元素个数
} ExtDef_Global;

typedef struct _Func_Param {
    AstNodeTag tag;
    AstNode *link;
    AstNode *spec;
    const char *name;
    int dimention;
    int *sz;
} Func_Param;

typedef struct _ExtDef_Function {
    AstNodeTag tag;
    AstNode *link;
    AstNode *spec;       // 返回值类型
    const char *name;    // 函数名
    Func_Param *params;  // 参数列表
} ExtDef_Function;

// 语句块内变量定义
typedef struct _Stmt_Def {
    AstNodeTag tag;
    AstNode *link;
    AstNode *spec;         // 变量类型
    const char *name;      // 变量名
    int dimention;         // 维度, 普通变量为 0
    int *sz;               // 每个维度的大小
    AstNode *initializer;  // 初始化表达式
} Stmt_Def;

// 用于标记作用域
typedef struct _Stmt_CompSt {
    AstNodeTag tag;
    AstNode *link;
    AstNode *statements;
} Stmt_CompSt;

typedef struct _Stmt_Return {
    AstNodeTag tag;
    AstNode *link;
    AstNode *exp;
} Stmt_Return;

typedef struct _Stmt_If {
    AstNodeTag tag;
    AstNode *link;
    AstNode *condition;
    AstNode *true_statement;
} Stmt_If;

typedef struct _Stmt_If_Else {
    AstNodeTag tag;
    AstNode *link;
    AstNode *condition;
    AstNode *true_statement;
    AstNode *false_statement;
} Stmt_If_Else;

typedef struct _Stmt_While {
    AstNodeTag tag;
    AstNode *link;
    AstNode *condition;
    AstNode *statement;
} Stmt_While;

typedef struct _Exp_And {
    AstNodeTag tag;
    AstNode *link;
    AstNode *left;
    AstNode *right;
} Exp_And;

typedef struct _Exp_Or {
    AstNodeTag tag;
    AstNode *link;
    AstNode *left;
    AstNode *right;
} Exp_Or;

typedef struct _Exp_Relop {
    AstNodeTag tag;
    AstNode *link;
    AstNode *left;
    AstNode *right;
    const char *relop;
} Exp_Relop;

typedef struct _Exp_Calc {
    AstNodeTag tag;
    AstNode *link;
    AstNode *left;
    AstNode *right;
    const char *operate;
} Exp_Calc;

typedef struct _Exp_Neg {
    AstNodeTag tag;
    AstNode *link;
    AstNode *exp;
} Exp_Neg;

typedef struct _Exp_Call {
    AstNodeTag tag;
    AstNode *link;
    const char *func_name;
    AstNode *params;
} Exp_Call;

typedef struct _Exp_Index {
    AstNodeTag tag;
    AstNode *link;
    AstNode *array_exp;
    int index;
} Exp_Index;

typedef struct _Exp_Access {
    AstNodeTag tag;
    AstNode *link;
    const char *struct_name;
    const char *field_name;
} Exp_Access;

typedef struct _Exp_ID {
    AstNodeTag tag;
    AstNode *link;
    const char *name;
} Exp_ID;

typedef struct _Exp_Const {
    AstNodeTag tag;
    AstNode *link;
    CmmType type;
    const char *lexeme;
    union {
        int integer;
        float real;
    };
} Exp_Const;


ExtDef_Type *new_ExtDef_Type();
Type_Struct *new_Type_Struct();
ExtDef_Global *new_ExtDef_Global();
Func_Param *new_Func_Param();
ExtDef_Function *new_ExtDef_Function();
Stmt_Def *new_Stmt_Def();
Stmt_CompSt *new_Stmt_CompSt();
Stmt_Return *new_Stmt_Return();
Stmt_If *new_Stmt_If();
Stmt_If_Else *new_Stmt_If_Else();
Stmt_While *new_Stmt_While();
Exp_And *new_Exp_And();
Exp_Or *new_Exp_Or();
Exp_Relop *new_Exp_Relop();
Exp_Calc *new_Exp_Calc();
Exp_Neg *new_Exp_Neg();
Exp_Call *new_Exp_Call();
Exp_Index *new_Exp_Index();
Exp_Access *new_Exp_Access();
Exp_ID *new_Exp_ID();
Exp_Const *new_Exp_Const();

#endif //__AST_NODE_H__

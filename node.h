#ifndef NODE_H
#define NODE_H

#include "cmm-type.h"
#include "yytname.h"

typedef struct Node_ *Node;
typedef struct Operand_ *Operand;

enum ProductionTag {
    UNSIMPLIFIED,
    PROG_is_EXTDEF,
    EXTDEF_is_SPEC_EXTDEC,
    EXTDEF_is_SPEC,
    EXTDEF_is_SPEC_FUNC_COMPST,
    EXTDEC_is_VARDEC,
    SPEC_is_TYPE,
    SPEC_is_STRUCT,
    STRUCT_is_ID_DEF,
    STRUCT_is_DEF,
    STRUCT_is_ID,
    VARDEC_is_ID,
    VARDEC_is_VARDEC_SIZE,
    FUNC_is_ID_VAR,
    VAR_is_SPEC_VARDEC,
    COMPST_is_DEF_STMT,
    STMT_is_EXP,
    STMT_is_COMPST,
    STMT_is_RETURN,
    STMT_is_IF,
    STMT_is_IF_ELSE,
    STMT_is_WHILE,
    DEF_is_SPEC_DEC,
    DEC_is_VARDEC,
    DEC_is_VARDEC_INITIALIZATION,
    EXP_is_ASSIGN,
    EXP_is_AND,
    EXP_is_OR,
    EXP_is_NOT,
    EXP_is_RELOP,
    EXP_is_BINARY,
    EXP_is_UNARY,
    EXP_is_EXP,
    EXP_is_ID,
    EXP_is_ID_ARG,
    EXP_is_EXP_IDX,
    EXP_is_EXP_FIELD,
    EXP_is_INT,
    EXP_is_FLOAT,
    TERM_INT,
    TERM_FLOAT,
    TERM_ID
};


struct Node_ {
    enum YYTNAME_INDEX type;
    enum ProductionTag tag;
    int lineno;
    union {
        int i;
        float f;
        const char *s;
        void *p;
        const char *operator;
    } val;
    Node child, sibling;
    Operand dst;  // 一个表达式可能需要上层提供的目标地址, 值类型可能会将这个字段的值替换 [Feature]
    Operand label_true;
    Operand label_false;
};

Node new_node(enum YYTNAME_INDEX type);
void free_node(Node nd);
void puts_tree(Node nd);
void analyze_program(Node program);

#endif // NODE_H

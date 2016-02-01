#ifndef NODE_H
#define NODE_H

#include "cmm-type.h"
#include "cmm-symtab.h"

typedef struct Node_ *Node;
typedef struct Operand_ *Operand;

struct Node_ {
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

    /////////////////////////////////////////////////////////////////
    // Used for syntax-directed analysis
    /////////////////////////////////////////////////////////////////

    // For semantic analysis
    struct {
        Type *type;
        const char *name;
        int lineno;
        Symbol **symtab;
    } sema;

    // For intermediate code translation
    Operand dst;  // 一个表达式可能需要上层提供的目标地址, 值类型可能会将这个字段的值替换 [Feature]
    Operand base; // Used for array to locate the initial address
    Operand label_true;
    Operand label_false;
};


Node new_node();
void free_node(Node nd);
void puts_tree(Node nd);
void analyze_program(Node program);

#endif // NODE_H

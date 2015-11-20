#ifndef NODE_H
#define NODE_H

#include "cmm_type.h"
#include "yytname.h"
#include "ast.h"
#include "ir.h"

typedef struct _node_t {
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
    struct _node_t *child, *sibling;
    Operand dst;
    Operand lable_true;
    Operand lable_false;
} node_t;

typedef node_t *Node;

node_t *new_node(enum YYTNAME_INDEX type);
void free_node(node_t *nd);
void puts_tree(node_t *nd);
void analyze_program(node_t *program);

#endif // NODE_H

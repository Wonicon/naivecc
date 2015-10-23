#ifndef NODE_H
#define NODE_H

#include "cmm_type.h"

typedef struct _node_t {
    int type;
    int lineno;
    union {
        int i;
        float f;
        const char *s;
        void *p;
    } val;
    struct _node_t *child, *sibling;
} node_t;

node_t *new_node(int type);
void free_node(node_t *nd);
void puts_tree(node_t *nd);
void analyze_program(node_t *program);
#endif // NODE_H

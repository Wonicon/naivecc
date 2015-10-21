#ifndef NODE_H
#define NODE_H

#include "cmm_type.h"

typedef struct Node {
    int type;
    int code;
    int lineno;
    int colno;
    union {
        int i;
        float f;
        char *s;
        void *p;
    } val;
    CmmType *inh_type;
    struct Node *child, *sibling;
} node_t;

node_t *new_node(int type);
void free_node(node_t *nd);

#endif /* NODE_H */

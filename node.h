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
    const CmmType *cmm_type;
    struct _node_t *child, *sibling;
} node_t;

node_t *new_node(int type);
void free_node(node_t *nd);
void puts_tree(node_t *nd);


typedef struct _attr_t {
    const CmmType *type;
    const char *name;
    int lval_flag;
} attr_t;


attr_t analyze_exp_id(const node_t *id);
attr_t analyze_specifier(const node_t *specifier);
attr_t analyze_paramdec(const node_t * paramdec);

#endif // NODE_H

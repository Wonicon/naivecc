#include "node.h"
#include <stdlib.h>

node_t *new_node(int type) {
    node_t *p = (node_t *)malloc(sizeof(node_t));
    p->type = type;
    p->val.s = NULL; /* Avoid unexpected free */
    return p;
}

void free_node(node_t *nd)
{
    if (nd == NULL) return;

    free_node(nd->child);
    free_node(nd->sibling);
    free(nd);
}

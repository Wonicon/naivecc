#ifndef __NODE_H__
#define __NODE_H__

typedef enum {
    NODE_VALUE_INT,
    NODE_VALUE_DOUBLE,
    NODE_VALUE_ID,
} node_val_t;

typedef struct Node {
    node_val_t type;
    union {
        int i;
        float f;
        char *s;
        void *p;
    } val;
    struct Node *child, *sibling;
} node_t;

int add_node(node_t *src, int n, ...);

#endif /* __NODE_H__ */

#ifndef NODE_H
#define NODE_H

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
    struct Node *child, *sibling;
} node_t;

int add_node(node_t *src, int n, ...);
node_t *new_node(int type);
#endif /* NODE_H */

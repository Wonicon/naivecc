#include "node.h"
#include "yytname.h"
#include "syntax.tab.h"
#include <stdlib.h>
#include <memory.h>


//
// node constructor, wrapping some initialization
//
node_t *new_node(int type) {
    node_t *p = (node_t *)malloc(sizeof(node_t));
    memset(p, 0, sizeof(p));
    p->type = type;
    return p;
}


//
// release the space the node occupies
// the cmm_type is stored in the symbol table
// the string is stored in the string table
// don't free them here.
//
void free_node(node_t *nd) {
    if (nd == NULL) {
        return;
    }
    free_node(nd->child);
    free_node(nd->sibling);
    free(nd);
}


//
// print the content of this node, indented according to level
// then print the child and sibling recursively in the newline
//
void midorder(node_t *nd, int level) {
    if (nd == NULL) {
        return;
    }

    // print the indent and the token name
    // one indent level means two blanks
    printf("%*s%s", level * 2, "", get_token_name(nd->type));

    switch (nd->type) {
        case YY_TYPE:
        case YY_ID:
            printf(": %s\n", nd->val.s);
            break;
        case YY_INT:
            printf(": %d\n", nd->val.i);
            break;
        case YY_FLOAT:
            printf(": %f\n", nd->val.f);
            break;
        case YY_RELOP:
            printf(": %s\n", nd->val.s);
            break;
        default:
            if (!is_terminal(nd->type)) {
                printf(" (%d)\n", nd->lineno);
            } else {
                printf("\n");
            }
    }
    midorder(nd->child, level + 1);
    midorder(nd->sibling, level);
}


//
// Print the ast of the program
//
void puts_tree(node_t *nd) {
    midorder(nd, 0);
}

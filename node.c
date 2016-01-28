#include "node.h"
#include "cmm-symtab.h"
#include <stdlib.h>
#include <assert.h>
#include <memory.h>


//
// node constructor, wrapping some initialization
//
Node new_node()
{
    Node p = (Node)malloc(sizeof(struct Node_));
    memset(p, 0, sizeof(*p));
    return p;
}


//
// release the space the node occupies
// the cmm_type is stored in the symbol table
// the string is stored in the string table
// don't free them here.
//
void free_node(Node nd)
{
    if (nd == NULL) {
        return;
    }
    free_node(nd->child);
    free_node(nd->sibling);
    free(nd);
}


//
// Created by whz on 15-11-18.
//

#include "ast.h"
#include "node.h"
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

static int body_size[] = {
    [PROG_is_EXTDEF] = 1,
    [EXTDEF_is_SPEC_EXTDEC] = 2,
    [EXTDEF_is_SPEC] = 1,
    [EXTDEF_is_SPEC_FUNC_COMPST] = 3,
    [EXTDEC_is_VARDEC] = 1,
    [SPEC_is_TYPE] = 1,
    [SPEC_is_STRUCT] = 1,
    [STRUCT_is_ID_DEF] = 2,
    [STRUCT_is_DEF] = 1,
    [STRUCT_is_ID] = 1,
    [VARDEC_is_ID] = 1,
    [VARDEC_is_VARDEC_SIZE] = 2,
    [FUNC_is_ID_VAR] = 2,
    [VAR_is_SPEC_VARDEC] = 2,
    [COMPST_is_DEF_STMT] = 2,
    [STMT_is_EXP] = 1,
    [STMT_is_COMPST] = 1,
    [STMT_is_RETURN] = 1,
    [STMT_is_IF] = 2,
    [STMT_is_IF_ELSE] = 3,
    [STMT_is_FOR]   = 4,
    [STMT_is_WHILE] = 2,
    [DEF_is_SPEC_DEC] = 2,
    [DEC_is_VARDEC] = 1,
    [DEC_is_VARDEC_INITIALIZATION] = 2,
    [EXP_is_ASSIGN] = 2,
    [EXP_is_AND] = 2,
    [EXP_is_OR] = 2,
    [EXP_is_NOT] = 1,
    [EXP_is_RELOP] = 2,
    [EXP_is_BINARY] = 2,
    [EXP_is_UNARY] = 1,
    [EXP_is_EXP] = 1,
    [EXP_is_ID] = 1,
    [EXP_is_ID_ARG] = 2,
    [EXP_is_EXP_IDX] = 2,
    [EXP_is_EXP_FIELD] = 2,
    [EXP_is_INT] = 1,
    [EXP_is_FLOAT] = 1
};


Node create_tree(enum ProductionTag tag, int lineno, ...)
{
    int n_body_symbol = body_size[tag];

    if (n_body_symbol <= 0) {
        return NULL;
    }

    Node root = malloc(sizeof(*root));
    memset(root, 0, sizeof(*root));
    root->tag = tag;
    root->lineno = lineno;

    va_list nodes;
    va_start(nodes, lineno);
        
    Node *curr = &(root->child);
    for (; n_body_symbol > 0; n_body_symbol--) {
        Node arg = va_arg(nodes, Node);
        if (arg == NULL) continue;
        *curr = arg;
        while (*curr != NULL) curr = &((*curr)->sibling);
    }

    return root;
}


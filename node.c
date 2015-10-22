#include "node.h"
#include "yytname.h"
#include "cmm_symtab.h"
#include <stdlib.h>
#include <assert.h>
#include <memory.h>


//
// node constructor, wrapping some initialization
//
node_t *new_node(int type) {
    node_t *p = (node_t *)malloc(sizeof(node_t));
    memset(p, 0, sizeof(*p));
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


static attr_t error_attr = {NULL, NULL, 0};

#define SEMA_ERROR_MSG(type, lineno, fmt, ...) \
fprintf(stderr, "Error type %d at Line %d: " fmt "\n", type, lineno, ## __VA_ARGS__)

attr_t analyze_id(const node_t *id) {
    const sym_ent_t *var = query(id->val.s, -1);
    if (var == NULL) {
        SEMA_ERROR_MSG(1, id->lineno, "undefined %s", id->val.s);
        return error_attr;
    }
    else {
        attr_t ret = {var->type, var->symbol, 0};
        return ret;
    }
}


attr_t analyze_vardec(const node_t *vardec, const CmmType *inh_type) {
    assert(vardec->type == YY_VarDec);

    if (vardec->child->type == YY_ID) {
        const node_t *id = vardec->child;
        attr_t return_attr = {inh_type, id->val.s, 1};
        return return_attr;
    } else if (vardec->child->type == YY_VarDec) {
        // Array
        // Now we have seen the most distanced dimension of an array
        // It should be a base type of the prior vardec, so we generate
        // a new array type first, and pass it as an inherit attribute
        // to the sub vardec.
        const node_t *sub_vardec = vardec->child;
        int size = sub_vardec->sibling->sibling->val.i;
        CmmArray *temp_array = new_type_array(size, inh_type);
        return analyze_vardec(sub_vardec, GENERIC(temp_array));
    } else {
        assert(0);
        return error_attr;
    }
}


// next comes from the declist behind
attr_t analyze_field_dec(const node_t *dec, const CmmType *type, const CmmField *next) {
    assert(dec->type == YY_Dec);

    const node_t *vardec = dec->child;
    // field does not allow assignment
    if (vardec->sibling != NULL) {
        SEMA_ERROR_MSG(15, dec->lineno, "Initialization in the structure definition is not allowed");
    }

    attr_t vardec_attr = analyze_vardec(vardec, type);
    return vardec_attr;
}


// next comes from the deflist behind
attr_t analyze_field_declist(const node_t *declist, const CmmType *type, const CmmField *next) {
    assert(declist->type == YY_DecList);

    // The first child is always dec
    const node_t *dec = declist->child;

    // Handle declist if it exists, otherwise the analysis of dec will use the default next_attr which contains next.
    attr_t next_attr = { GENERIC(next), NULL, 0 };
    if (dec->sibling != NULL) {
        // declist -> dec comma declist
        const node_t *sub_list = dec->sibling->sibling;
        next_attr = analyze_field_declist(sub_list, type, next);
    }

    // Analyze the dec later because we want the list head of the declist to link up.
    // If we don't have a declist here, the next_attr.type remains next in parameters.
    attr_t this_attr = analyze_field_dec(dec, type, Field(next_attr.type));

    LOG("Generate a new field: %s", this_attr.name);
    CmmField *field = new_type_field(this_attr.type, this_attr.name, Field(next_attr.type));

    attr_t return_attr = { GENERIC(field), NULL, 0 };
    return return_attr;
}


// next comes from the deflist behind
attr_t analyze_field_def(const node_t *def, const CmmField *next) {
    // Get body symbols
    const node_t *spec = def->child;
    const node_t *declist = spec->sibling;
    const node_t *semi = declist->sibling;
    assert(spec->type == YY_Specifier);
    assert(declist->type == YY_DecList);
    assert(semi->type == YY_SEMI);

    // Handle Specifier
    const CmmType *field_type = analyze_specifier(def->child).type;

    // Handle DecList and get a field list
    attr_t declist_attr = analyze_field_declist(declist, field_type, next);

    return declist_attr;
}


// no next!
attr_t analyze_field_deflist(const node_t *deflist) {
    if (deflist == NULL) {
        return error_attr;
    }
    const node_t *def = deflist->child;
    const node_t *sub_list = def->sibling;
    attr_t sub_field = analyze_field_deflist(sub_list);
    attr_t this_field = analyze_field_def(def, Field(sub_field.type));
    return this_field;
}


attr_t analyze_struct_spec(const node_t *struct_spec) {
    assert(struct_spec->type == YY_StructSpecifier);
    node_t *body = struct_spec->child->sibling;
    LOG("The second child of struct spec is %s", get_token_name(body->type));
    if (body->type == YY_Tag) {
        // No new struct type will be registered;
        // Get name
        assert(body->child->type == YY_ID);
        const sym_ent_t *ent = query(body->child->val.s, -1);
        attr_t ret = error_attr;
        if (ent == NULL) {
            SEMA_ERROR_MSG(17, body->lineno, "Undefined struct name '%s'", body->child->val.s);
            CmmStruct *this = new_type_struct("");
            ret.type = GENERIC(this);
            ret.name = "";
            ret.lval_flag = 0;
        } else {
            ret.type = ent->type;
            ret.name = ent->symbol;
        }
        return ret;
    }
    // The follow cases will generate new structure.
    const char *name = NULL;
    if (body->type == YY_LC){
        // Construct a new struct type with empty name
        name = "";
        body = body->sibling;
    } else {
        // Get name
        assert(body->type == YY_OptTag);
        name = body->child->val.s;
        body = body->sibling->sibling;
    }
    assert(body->type == YY_DefList);

    // Construct struct type
    CmmStruct *this = new_type_struct(name);

    // Get field list
    attr_t field = analyze_field_deflist(body);
    assert(*(field.type) == CMM_TYPE_FIELD);
    this->field_list = Field(field.type);

    // Use the struct name to register in the symbol table
    int insert_rst = !strcmp(this->tag, "") ? 1 :
                     insert(this->tag, GENERIC(this), struct_spec->lineno, -1);
    if (insert_rst < 1) {
        SEMA_ERROR_MSG(16, struct_spec->lineno, "The struct name '%s' has collision with previous one(s)", name);
    }

    attr_t attr_ret = {GENERIC(this), this->tag, 0};
    return attr_ret;
}


attr_t analyze_specifier(const node_t *specifier) {
    attr_t ret = {NULL, NULL, 0};
    if (specifier->child->type == YY_TYPE) {
        const char *type_name = specifier->child->val.s;
        if (!strcmp(type_name, typename(global_int))) {
            ret.type = global_int;
        } else if (!strcmp(type_name, typename(global_float))) {
            ret.type = global_float;
        }
        return ret;
    } else {
        return analyze_struct_spec(specifier->child);
    }
}
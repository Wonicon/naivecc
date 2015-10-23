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


typedef struct _attr_t {
    const CmmType *type;
    const char *name;
} var_t;

#define STRUCT_SCOPE (-10086)

static var_t default_attr = {NULL, NULL};

#define SEMA_ERROR_MSG(type, lineno, fmt, ...) \
fprintf(stderr, "Error type %d at Line %d: " fmt "\n", type, lineno, ## __VA_ARGS__)

//
// Some predeclaration
//
const CmmType *analyze_specifier(const node_t *);                     // required by analyze_[def|paramdec|extdef]
const CmmType *analyze_exp(const node_t *exp, int scope);             // required by check_param_lsit, analyze_vardec
const CmmType *analyze_compst(const node_t *, const CmmType *, int);  // required by analyze_stmt


var_t analyze_vardec(const node_t *vardec, const CmmType *inh_type) {
    assert(vardec->type == YY_VarDec);

    if (vardec->child->type == YY_ID) {
        const node_t *id = vardec->child;
        var_t return_attr = { inh_type, id->val.s };
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
        return default_attr;
    }
}


// next comes from the declist behind
var_t analyze_dec(const node_t *dec, const CmmType *type, int scope) {
    assert(dec->type == YY_Dec);

    const node_t *vardec = dec->child;
    var_t var = analyze_vardec(vardec, type);

    // TODO is field need insert symbol?
    if (scope != STRUCT_SCOPE && (insert(var.name, var.type, dec->child->child->lineno, scope) < 0)) {
        SEMA_ERROR_MSG(4, dec->child->child->lineno, "Duplicated variable '%s'", var.name);
        // TODO handle memory leak
    }

    // Assignment / Initialization
    if (vardec->sibling != NULL) {
        if (scope == STRUCT_SCOPE) {  // Field does not allow assignment
            SEMA_ERROR_MSG(15, dec->lineno, "Initialization in the structure definition is not allowed");
        } else {  // Assignment consistency check
            const node_t *exp = vardec->sibling->sibling;
            const CmmType *exp_type = analyze_exp(exp, scope);
            if (!typecmp(exp_type, type)) {
                SEMA_ERROR_MSG(5, vardec->sibling->lineno, "Type mismatch");
            }
        }
    }

    return var;
}


// next comes from the deflist behind
const CmmField *analyze_declist(const node_t *declist, const CmmType *type, const int scope,  const CmmField *next) {
    assert(declist->type == YY_DecList);
    const node_t *dec = declist->child;

    // Handle declist if it exists.
    // We do assignment here ignoring whether the declist is in the struct
    // because this behavior has no side effects. We will discard the next_field
    // in the end of this function if we are in CompSt declist!
    const CmmField *next_field;
    if (dec->sibling != NULL) {
        // declist -> dec comma declist
        const node_t *sub_list = dec->sibling->sibling;
        next_field = analyze_declist(sub_list, type, scope, next);
    } else {
        next_field = next;
    }

    // Analyze the dec later because we want the list head of the declist to link up.
    var_t var = analyze_dec(dec, type, scope);

    // If the declist is not in struct, then we just return NULL.
    if (scope == STRUCT_SCOPE) {
        // Get id line number
        const node_t *find_id = dec->child;
        while (find_id->type != YY_ID) {
            find_id = find_id->child;
        }
        CmmField *field = new_type_field(var.type, var.name, find_id->lineno, next_field);
        return field;
    } else {
        return NULL;
    }
}


// next comes from the deflist behind
const CmmField *analyze_def(const node_t *def, const int scope, const CmmField *next) {
    assert(def->type == YY_Def);

    // Get body symbols
    const node_t *spec = def->child;
    const node_t *declist = spec->sibling;

    // Handle Specifier
    const CmmType *type = analyze_specifier(def->child);

    // Handle DecList and get a field list if we are in struct scope
    return analyze_declist(declist, type, scope, next);
}


// no next!
const CmmField *analyze_deflist(const node_t *deflist, int scope) {
    assert(deflist->type == YY_DefList);

    const node_t *def = deflist->child;
    const node_t *sub_list = def->sibling;
    const CmmField *sub_field = sub_list == NULL ? NULL : analyze_deflist(sub_list, scope);
    const CmmField *field = analyze_def(def, scope, sub_field);
    return field;
}


var_t analyze_struct_spec(const node_t *struct_spec) {
    assert(struct_spec->type == YY_StructSpecifier);

    const node_t *body = struct_spec->child->sibling;  // Jump tag 'struct'
    LOG("The second child of struct spec is %s", get_token_name(body->type));
    if (body->type == YY_Tag) {
        // No new struct type will be registered.
        // Get name from symbol table.
        const node_t *id = body->child;
        assert(id->type == YY_ID);
        const sym_ent_t *ent = query(id->val.s, -1);
        var_t ret = default_attr;
        if (ent == NULL) {  // The symbol is not found
            SEMA_ERROR_MSG(17, body->lineno, "Undefined struct name '%s'", body->child->val.s);
            // Create an empty anonymous struct for this variable to allow it stored in the symbol table.
            CmmStruct *this = new_type_struct("");
            ret.type = GENERIC(this);
            ret.name = "";
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
        LOG("Get name from tag %s", body->child->val.s);
        assert(body->type == YY_OptTag);
        name = body->child->val.s;
        body = body->sibling->sibling;
    }
    assert(body->type == YY_DefList);

    // Construct struct type
    CmmStruct *this = new_type_struct(name);

    // Get field list
    const CmmField *field = analyze_deflist(body, STRUCT_SCOPE);
    assert(field->type == CMM_TYPE_FIELD);
    this->field_list = field;

    // Check fields
    const CmmField *outer = this->field_list;
    const CmmField *inner;
    while (outer != NULL) {
        inner = outer->next;
        while (inner != NULL) {
            if (!strcmp(outer->name, inner->name)) {
                SEMA_ERROR_MSG(15, inner->def_line_no, "Duplicated decleration of field %s, the previous one is at %d",
                               outer->name, outer->def_line_no);
                break;  // Avoid duplicated error msg when more than two duplications
            }
            inner = inner->next;
        }
        outer = outer->next;
    }

    // Use the struct name to register in the symbol table.
    // Ignore the struct with empty tag.
    int insert_rst = !strcmp(this->tag, "") ? 1 :
                     insert(this->tag, GENERIC(this), struct_spec->lineno, -1);
    if (insert_rst < 1) {
        SEMA_ERROR_MSG(16, struct_spec->lineno, "The struct name '%s' has collision with previous one(s)", name);
    }

    var_t attr_ret = { GENERIC(this), this->tag };
    return attr_ret;
}


const CmmType *analyze_specifier(const node_t *specifier) {
    if (specifier->child->type == YY_TYPE) {
        const char *type_name = specifier->child->val.s;
        if (!strcmp(type_name, typename(global_int))) {
            return global_int;
        } else if (!strcmp(type_name, typename(global_float))) {
            return global_float;
        } else {
            assert(0);
        }
    } else {
        return analyze_struct_spec(specifier->child).type;
    }
}


//
// Analyze FunDec production:
//   FunDec -> ID LP VarList RP
//   FunDec -> ID LP RP
// The most important part is the analysis of VarList.
// It is a param list, which is akin to the field list.
// But we cannot reuse the analyze_field_declist because the production is so different.
//

// First we should analyze the VarList part. Because a parameter must follow a Specifier,
// so we can avoid the hell of declist that inherits the same Specifier.
// We can just register the parameter here.
var_t analyze_paramdec(const node_t *paramdec) {
    assert(paramdec->type == YY_ParamDec);
    const node_t *specifier = paramdec->child;
    const node_t *vardec = specifier->sibling;
    const CmmType *spec = analyze_specifier(specifier);
    var_t var = analyze_vardec(vardec, spec);
    int insert_ret = insert(var.name, var.type, paramdec->lineno, -1);
    if (insert_ret < 1) {
        SEMA_ERROR_MSG(3, vardec->lineno, "Duplicated variable definition of '%s'", var.name);
    }
    return var;
}

// Then we should link the paramdec's type up to form a param type list.
// varlist should return a type of CmmParam, and the generation of CmmParam occurs here.
const CmmType *analyze_varlist(const node_t *varlist) {
    const node_t *paramdec = varlist->child;
    const node_t *sub_varlist = paramdec->sibling == NULL ? NULL : paramdec->sibling->sibling;
    assert(paramdec->type == YY_ParamDec);
    assert(sub_varlist == NULL || sub_varlist->type == YY_VarList);

    var_t param_attr = analyze_paramdec(paramdec);
    const CmmType *sub_list = sub_varlist == NULL ? NULL : analyze_varlist(sub_varlist);
    CmmParam *param = new_type_param(param_attr.type, Param(sub_list));

    return GENERIC(param);
}

// Analyze the function and register it
//   FunDec -> ID LP VarList RP
//   FunDec -> ID LP RP
void analyze_fundec(const node_t *fundec, const CmmType *inh_type) {
    assert(fundec->type == YY_FunDec);
    const node_t *id = fundec->child;
    const node_t *varlist = id->sibling->sibling;

    // Get identifier
    const char *name = id->val.s;

    // Get param list if exists
    const CmmParam *param_list = NULL;
    if (varlist->type == YY_VarList) {
        param_list = Param(analyze_varlist(varlist));
    } else {
        assert(varlist->type == YY_RP);
    }

    // Generate function symbol
    CmmFunc *func = new_type_func(name, inh_type);
    func->param_list = param_list;

    if (insert(func->name, GENERIC(func), fundec->lineno, -1) < 0) {
        SEMA_ERROR_MSG(4, fundec->lineno, "Duplicated function defination of '%s'", func->name);
        // TODO handle memory leak!
    }
}


void analyze_extdeclist(const node_t *extdeclist, const CmmType *inh_type) {
    assert(extdeclist->type == YY_ExtDecList);

    const node_t *vardec = extdeclist->child;
    assert(vardec->type == YY_VarDec);
    var_t var_attr = analyze_vardec(vardec, inh_type);
    if (insert(var_attr.name, var_attr.type, vardec->lineno, -1) < 0) {
        SEMA_ERROR_MSG(3, vardec->child->lineno, "Duplicated identifier '%s'", var_attr.name);
        // TODO handle memory leak
    }

    if (vardec->sibling != NULL) {
        analyze_extdeclist(vardec->sibling, inh_type);
    }
}

// TODO if-elseif-else return routes analysis

//
// exp only return its type.
// We use stmt analyzer to check the return type.
//
static inline int is_lval(const node_t *exp) {
    assert(exp != NULL && exp->type == YY_Exp);
    if (exp->child->type == YY_ID) {
        return 1;
    } else if (exp->child->sibling != NULL && exp->child->sibling->type == YY_LB) {
        return 1;
    } else if (exp->child->sibling != NULL && exp->child->sibling->type == YY_DOT) {
        return 1;
    } else {
        return 0;
    }
}


static int check_param_list(const CmmParam *param, const node_t *args, int scope) {
    if (param == NULL && args->type == YY_RP) {
        return 1;
    } else if ((param == NULL && args->type != YY_RP) ||
            (param != NULL && args->type == YY_RP)) {
        SEMA_ERROR_MSG(9, args->lineno, "parameter mismatch");
        return 0;
    } else {
        assert(args->type == YY_Args);
        const node_t *arg = NULL;
        while (param != NULL) {
            arg = args->child;
            const CmmType *param_type = analyze_exp(arg, scope);
            if (!typecmp(param_type, param->base)) {
                SEMA_ERROR_MSG(9, arg->lineno, "parameter type mismatches");
            }
            param = param->next;
            if (arg->sibling == NULL) {
                args = arg;
                break;
            } else {
                args = arg->sibling->sibling;
            }
        }

        if (!(param == NULL && args == NULL)) {
            SEMA_ERROR_MSG(9, arg->lineno, "parameter number mismatches");
            return 0;
        } else {
            return 1;
        }
    }
}

const CmmType *analyze_exp(const node_t *exp, int scope) {
    assert(exp->type == YY_Exp);

    const node_t *id, *lexp, *rexp, *op;
    const CmmType *lexp_type = NULL, *rexp_type = NULL;
    const sym_ent_t *query_result = NULL;
    switch (exp->child->type) {
        case YY_INT:
            return global_int;
        case YY_FLOAT:
            return global_float;
        case YY_MINUS:
            rexp = exp->child->sibling;
            return analyze_exp(rexp, scope);
        case YY_NOT:
            rexp = exp->child->sibling;
            return analyze_exp(rexp, scope);
        case YY_LP:
            lexp = exp->child->sibling;
            return analyze_exp(lexp, scope);
        case YY_ID:
            // TODO: func
            id = exp->child;
            // Regardless of whether it is an id or func call, let's query it first~
            query_result = query(id->val.s, scope);

            if (id->sibling != NULL && id->sibling->type == YY_LP) {
                if (query_result == NULL) {
                    SEMA_ERROR_MSG(2, id->lineno, "Function '%s' is not defined.", id->val.s);
                    return NULL;
                } else if (*(query_result->type) != CMM_TYPE_FUNC) {
                    SEMA_ERROR_MSG(11, id->lineno, "The identifier '%s' is not a function", id->val.s);
                    return query_result->type;
                } else if (!check_param_list(Fun(query_result->type)->param_list, id->sibling->sibling, scope)) {
                    // Error report in the check function
                    return Fun(query_result->type)->ret;
                } else {
                    // Return the return value
                    return Fun(query_result->type)->ret;
                }
            } else {
                if (query_result == NULL) {
                    SEMA_ERROR_MSG(1, id->lineno, "Variable '%s' is not defined.", id->val.s);
                    return NULL;
                } else {
                    return query_result->type;
                }
            }
        case YY_Exp:
            lexp = exp->child;
            op = lexp->sibling;
            rexp = op->sibling;
            lexp_type = analyze_exp(lexp, scope);
            rexp_type = analyze_exp(rexp, scope);
            switch (op->type) {
                case YY_DOT:
                    id = rexp;
                    if (*lexp_type != CMM_TYPE_STRUCT) {
                        SEMA_ERROR_MSG(13, op->lineno, "The left identifier of '.' is not a struct");
                    } else if ((rexp_type = query_field(lexp_type, id->val.s)) == NULL) {
                        SEMA_ERROR_MSG(14, id->lineno,
                                       "Undefined field '%s' in struct '%s'.",
                                       id->val.s, Struct(lexp_type)->tag);
                        return NULL;
                    } else {
                        // Commonly we return the type of the field
                        return rexp_type;
                    }
                case YY_LB:
                    if (*rexp_type != CMM_TYPE_INT) {
                        SEMA_ERROR_MSG(12, rexp->lineno, "The index's type is not 'int'");
                    }
                    if (*lexp_type != CMM_TYPE_ARRAY) {
                        SEMA_ERROR_MSG(10, lexp->lineno, "The expression before '[' is not an array type");
                        return NULL;
                    } else {
                        return Array(lexp_type)->base;
                    }
                default:
                    if (!typecmp(lexp_type, rexp_type)) {
                        SEMA_ERROR_MSG(7, op->lineno, "Operands' types mismatch");
                        // TODO: Return int as default, is this right?
                        return global_int;
                    } else if (op->type != YY_ASSIGNOP &&
                                (!typecmp(lexp_type, global_int)
                                 || !typecmp(lexp_type, global_float))) {
                        SEMA_ERROR_MSG(7, op->lineno, "The type is not allowed in operation '%s'", op->val.s);
                        return global_int;
                    } else if (op->type == YY_ASSIGNOP && !is_lval(lexp)) {
                        SEMA_ERROR_MSG(6, op->lineno, "The left expression of the '%s' is not a lvalue.",
                                        op->val.s);
                        return NULL;
                    } else {
                        return lexp_type;
                    }
            }
        default:
            LOG("HELL");
            assert(0);
    }
}

const CmmType *analyze_stmt(const node_t *stmt, const CmmType *inh_func_type, int scope) {
    assert(stmt->type == YY_Stmt);
    const node_t *first = stmt->child;
    const CmmType *stmt_type = NULL;
    const node_t *exp = NULL, *sub_stmt = NULL;
    switch (first->type) {
        case YY_Exp:
            analyze_exp(first, scope);
            break;
        case YY_CompSt:
            analyze_compst(first, inh_func_type, scope);
            break;
        case YY_IF:
            exp = first->sibling->sibling;
            stmt_type = analyze_exp(exp, scope);
            if (!typecmp(stmt_type, global_int)) {
                SEMA_ERROR_MSG(7, exp->lineno, "The condition expression must return int");
            }
            sub_stmt = exp->sibling->sibling;
            analyze_compst(sub_stmt, inh_func_type, scope);
            if (sub_stmt->sibling != NULL) {
                // ELSE
                analyze_stmt(sub_stmt->sibling->sibling, inh_func_type, scope);
            }
            break;
        case YY_WHILE:
            exp = first->sibling->sibling;
            stmt_type = analyze_exp(exp, scope);
            if (!typecmp(stmt_type, global_int)) {
                SEMA_ERROR_MSG(7, exp->lineno, "The condition expression must return int");
            }
            sub_stmt = exp->sibling->sibling;
            analyze_stmt(sub_stmt, inh_func_type, scope);
            break;
        case YY_RETURN:
            exp = first->sibling;
            stmt_type = analyze_exp(exp, scope);
            if (!typecmp(stmt_type, inh_func_type)) {
                SEMA_ERROR_MSG(8, exp->lineno, "Return type mismatch");
            }
            break;
        default:
            LOG("Awful statement");
            assert(0);
    }

    return stmt_type;
}

const CmmType *analyze_stmtlist(const node_t *stmtlist, const CmmType *inh_func_type, int scope) {
    const node_t *stmt = stmtlist->child;
    const CmmType *return_type = analyze_stmt(stmt, inh_func_type, scope);
    if (stmt->sibling != NULL) {
        return analyze_stmtlist(stmt->sibling, inh_func_type, scope);
    } else {
        return return_type;
    }
}

//
// Second level analyzer router : CompSt
// Currently the scope has no thing to do with analysis
//
const CmmType *analyze_compst(const node_t *compst, const CmmType *inh_func_type, int scope) {
    assert(compst->type == YY_CompSt);

    const node_t *list = compst->child->sibling;

    const CmmType *return_type = NULL;

    while (list != NULL) {
        if (list->type == YY_DefList) {
            analyze_deflist(list, scope);
        } else if (list->type == YY_StmtList) {
            return_type = analyze_stmtlist(list, inh_func_type, scope);
        }
        list = list->sibling;
    }

    return return_type;
}

//
// Top level analyzer of extdef acting as a router
// Production:
//   ExtDef -> Specifier ExtDecList SEMI
//   ExtDef -> Specifier FunDec CompSt
//   ExtDef -> Specifier SEMI
//
void analyze_extdef(const node_t *extdef) {
    assert(extdef->type == YY_ExtDef);

    const node_t *spec = extdef->child;
    const CmmType *type = analyze_specifier(spec);
    switch (spec->sibling->type) {
        case YY_ExtDecList:
            analyze_extdeclist(spec->sibling, type);
            break;
        case YY_FunDec:
            analyze_fundec(spec->sibling, type);
            analyze_compst(spec->sibling->sibling, type, 0);
            break;
        case YY_SEMI:
            LOG("Well, I think this is used for struct");
            break;
        default:
            LOG("WTF");
    }
}

void analyze_program(const node_t *prog) {
    const node_t *extdef = prog->child->child;
    while (extdef != NULL) {
        analyze_extdef(extdef);
        if (extdef->sibling != NULL) {
            extdef = extdef->sibling->child;
        } else {
            break;
        }
    }
}

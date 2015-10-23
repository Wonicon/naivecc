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


typedef struct {
    CmmType *type;
    const char *name;
} var_t;

#define STRUCT_SCOPE (-10086)

static var_t default_attr = {NULL, NULL};

#define SEMA_ERROR_MSG(type, lineno, fmt, ...) \
fprintf(stderr, "Error type %d at Line %d: " fmt "\n", type, lineno, ## __VA_ARGS__)

//
// Some pre-declaration
//
CmmType *analyze_specifier(node_t *);                     // required by analyze_[def|paramdec|extdef]
CmmType *analyze_exp(node_t *exp, int scope);             // required by check_param_lsit, analyze_vardec
CmmType *analyze_compst(node_t *, CmmType *, int);  // required by analyze_stmt


var_t analyze_vardec(node_t *vardec, CmmType *inh_type) {
    assert(vardec->type == YY_VarDec);

    if (vardec->child->type == YY_ID) {
        node_t *id = vardec->child;
        var_t return_attr = { inh_type, id->val.s };
        return return_attr;
    } else if (vardec->child->type == YY_VarDec) {
        // Array
        // Now we have seen the most distanced dimension of an array
        // It should be a base type of the prior vardec, so we generate
        // a new array type first, and pass it as an inherit attribute
        // to the sub vardec.
        node_t *sub_vardec = vardec->child;
        int size = sub_vardec->sibling->sibling->val.i;
        CmmArray *temp_array = new_type_array(size, inh_type);
        return analyze_vardec(sub_vardec, GENERIC(temp_array));
    } else {
        assert(0);
        return default_attr;
    }
}


// next comes from the declist behind
var_t analyze_dec(node_t *dec, CmmType *type, int scope) {
    assert(dec->type == YY_Dec);

    node_t *vardec = dec->child;
    var_t var = analyze_vardec(vardec, type);

    // TODO is field need insert symbol?
    if (scope != STRUCT_SCOPE && (insert(var.name, var.type, dec->child->child->lineno, scope) < 0)) {
        SEMA_ERROR_MSG(3, dec->child->child->lineno, "Redefined variable \"%s\".", var.name);
        // TODO handle memory leak
    }

    // Assignment / Initialization
    if (vardec->sibling != NULL) {
        if (scope == STRUCT_SCOPE) {  // Field does not allow assignment
            SEMA_ERROR_MSG(15, dec->lineno, "Initialization in the structure definition is not allowed");
        } else {  // Assignment consistency check
            node_t *exp = vardec->sibling->sibling;
            CmmType *exp_type = analyze_exp(exp, scope);
            if (!typecmp(exp_type, type)) {
                SEMA_ERROR_MSG(5, vardec->sibling->lineno, "Type mismatch");
            }
        }
    }

    return var;
}


// next comes from the deflist behind
CmmField *analyze_declist(node_t *declist, CmmType *type, int scope) {
    assert(declist->type == YY_DecList);
    node_t *dec = declist->child;

    // Analyze the dec first in order to have a right symbol registering sequence.
    var_t var = analyze_dec(dec, type, scope);

    // Handle declist if it exists.
    // We do assignment here ignoring whether the declist is in the struct
    // because this behavior has no side effects. We will discard the next_field
    // in the end of this function if we are in CompSt declist!
    CmmField *next_field;
    if (dec->sibling != NULL) {
        // declist -> dec comma declist
        node_t *sub_list = dec->sibling->sibling;
        next_field = analyze_declist(sub_list, type, scope);
    } else {
        next_field = NULL;
    }

    // If the declist is not in struct, then we just return NULL.
    if (scope == STRUCT_SCOPE) {
        // Get id line number
        node_t *find_id = dec->child;
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
CmmField *analyze_def(node_t *def, int scope) {
    assert(def->type == YY_Def);

    // Get body symbols
    node_t *spec = def->child;
    node_t *declist = spec->sibling;

    // Handle Specifier
    CmmType *type = analyze_specifier(spec);

    // Handle DecList and get a field list if we are in struct scope
    return analyze_declist(declist, type, scope);
}


CmmField *analyze_deflist(node_t *deflist, int scope) {
    assert(deflist->type == YY_DefList);

    // Resolve the first definition
    node_t *def = deflist->child;
    CmmField *field = analyze_def(def, scope);
    // Resolve the rest definitions if exists
    node_t *sub_list = def->sibling;
    CmmField *sub_field = sub_list == NULL ? NULL : analyze_deflist(sub_list, scope);

    // If we are in a struct scope, these CmmField will be discarded.

    if (scope == STRUCT_SCOPE) {  // Link up the field
        CmmField *pretail = field;
        while (pretail->next != NULL) {
            pretail = pretail->next;
        }
        pretail->next = sub_field;
    } else {
        assert(field == NULL && sub_field == NULL);
    }

    return field;
}


const CmmStruct *analyze_struct_spec(node_t *struct_spec) {
    assert(struct_spec->type == YY_StructSpecifier);

    node_t *body = struct_spec->child->sibling;  // Jump tag 'struct'

    //
    // Check whether this is a struct definition or declaration
    //
    if (body->type == YY_Tag) {
        //
        // Declaration
        //
        node_t *id = body->child;
        sym_ent_t *ent = query(id->val.s, STRUCT_SCOPE);
        if (ent == NULL) {  // The symbol is not found
            SEMA_ERROR_MSG(17, body->lineno, "Undefined struct name '%s'", body->child->val.s);
            return NULL;
        } else {
            return Struct(ent->type);
        }
    }

    //
    // Definition
    //
    const char *name = NULL;
    if (body->type == YY_LC){
        // Construct a new struct type with empty name
        name = "";
        body = body->sibling;
    } else {
        // Get name
        name = body->child->val.s;
        body = body->sibling->sibling;
    }

    assert(body->type == YY_DefList);

    // Construct struct type
    CmmStruct *this = new_type_struct(name);

    //
    // Get field list
    //
    CmmField *field = analyze_deflist(body, STRUCT_SCOPE);
    this->field_list = field;

    // Check field name collision

    for (CmmField *outer = this->field_list; outer != NULL; outer = outer->next) {
        if (outer->name[0] == '\0') {
            // This is an anonymous field, which means that it has been detected as a duplicated field.
            continue;
        }

        CmmField *inner = outer->next;
        while (inner != NULL) {
            if (!strcmp(outer->name, inner->name)) {
                SEMA_ERROR_MSG(15, inner->def_line_no,
                               "Duplicated decleration of field %s, the previous one is at %d",
                               outer->name, outer->def_line_no);
                inner->name = "";
            }
            inner = inner->next;
        }
    }

    // Use the struct name to register in the symbol table.
    // Ignore the struct with empty tag.
    int insert_rst = !strcmp(this->tag, "") ? 1 :
                     insert(this->tag, GENERIC(this), struct_spec->lineno, -1);
    if (insert_rst < 1) {
        SEMA_ERROR_MSG(16, struct_spec->lineno, "Duplicated name \"%s\".", name);
    }

    return this;
}


CmmType *analyze_specifier(node_t *specifier) {
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
        return GENERIC(analyze_struct_spec(specifier->child));
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
var_t analyze_paramdec(node_t *paramdec) {
    assert(paramdec->type == YY_ParamDec);
    node_t *specifier = paramdec->child;
    node_t *vardec = specifier->sibling;
    CmmType *spec = analyze_specifier(specifier);
    var_t var = analyze_vardec(vardec, spec);
    int insert_ret = insert(var.name, var.type, paramdec->lineno, -1);
    if (insert_ret < 1) {
        SEMA_ERROR_MSG(3, vardec->lineno, "Duplicated variable definition of '%s'", var.name);
    }
    return var;
}

// Then we should link the paramdec's type up to form a param type list.
// varlist should return a type of CmmParam, and the generation of CmmParam occurs here.
CmmType *analyze_varlist(node_t *varlist) {
    node_t *paramdec = varlist->child;
    node_t *sub_varlist = paramdec->sibling == NULL ? NULL : paramdec->sibling->sibling;
    assert(paramdec->type == YY_ParamDec);
    assert(sub_varlist == NULL || sub_varlist->type == YY_VarList);

    var_t param_attr = analyze_paramdec(paramdec);
    CmmType *sub_list = sub_varlist == NULL ? NULL : analyze_varlist(sub_varlist);
    CmmParam *param = new_type_param(param_attr.type, Param(sub_list));

    return GENERIC(param);
}

// Analyze the function and register it
//   FunDec -> ID LP VarList RP
//   FunDec -> ID LP RP
void analyze_fundec(node_t *fundec, CmmType *inh_type) {
    assert(fundec->type == YY_FunDec);
    node_t *id = fundec->child;
    node_t *varlist = id->sibling->sibling;

    // Get identifier
    const char *name = id->val.s;

    // Get param list if exists
    CmmParam *param_list = NULL;
    if (varlist->type == YY_VarList) {
        param_list = Param(analyze_varlist(varlist));
    } else {
        assert(varlist->type == YY_RP);
    }

    // Generate function symbol
    CmmFunc *func = new_type_func(name, inh_type);
    func->param_list = param_list;

    if (insert(func->name, GENERIC(func), fundec->lineno, -1) < 0) {
        SEMA_ERROR_MSG(4, fundec->lineno, "Redefined function \"%s\"", func->name);
        // TODO handle memory leak!
    }
}


void analyze_extdeclist(node_t *extdeclist, CmmType *inh_type) {
    assert(extdeclist->type == YY_ExtDecList);

    node_t *vardec = extdeclist->child;
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
static inline int is_lval(node_t *exp) {
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


static int check_param_list(CmmParam *param, node_t *args, int scope) {
    if (param == NULL && args->type == YY_RP) {
        return 1;
    } else if ((param == NULL && args->type != YY_RP) ||
            (param != NULL && args->type == YY_RP)) {
        SEMA_ERROR_MSG(9, args->lineno, "parameter mismatch");
        return 0;
    } else {
        assert(args->type == YY_Args);
        node_t *arg = NULL;
        while (param != NULL) {
            arg = args->child;
            CmmType *param_type = analyze_exp(arg, scope);
            if (!typecmp(param_type, param->base)) {
                SEMA_ERROR_MSG(9, arg->lineno, "parameter type mismatches");
            }
            param = param->next;
            if (arg->sibling == NULL) {
                args = arg->sibling;
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

const char *expr_to_s(node_t *exp);
CmmType *analyze_exp(node_t *exp, int scope) {
    assert(exp->type == YY_Exp);

    node_t *id, *lexp, *rexp, *op;
    CmmType *lexp_type = NULL, *rexp_type = NULL;
    sym_ent_t *query_result = NULL;
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
                    SEMA_ERROR_MSG(2, id->lineno, "Undefined function \"%s\".", id->val.s);
                    return NULL;
                } else if (*(query_result->type) != CMM_TYPE_FUNC) {
                    SEMA_ERROR_MSG(11, id->lineno, "\"%s\" is not a function.", id->val.s);
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
                    SEMA_ERROR_MSG(1, id->lineno, "Undefined variable \"%s\"", id->val.s);
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
            switch (op->type) {
                case YY_DOT:
                    id = rexp;
                    if (*lexp_type != CMM_TYPE_STRUCT) {
                        SEMA_ERROR_MSG(13, op->lineno, "The left identifier of '.' is not a struct");
                        return NULL;
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
                    rexp_type = analyze_exp(rexp, scope);
                    if (*rexp_type != CMM_TYPE_INT) {
                        SEMA_ERROR_MSG(12, rexp->lineno, "%s is not a integer", expr_to_s(rexp));
                    }

                    // If lexp_type is null, it means that an semantic error has occurred, then we can ignore the
                    // consecutive errors.
                    if (lexp_type == NULL) {
                        return NULL;
                    } else if (*lexp_type != CMM_TYPE_ARRAY) {
                        SEMA_ERROR_MSG(10, lexp->lineno, " \"%s\" is not an array.", expr_to_s(lexp));
                        return NULL;
                    } else {
                        return Array(lexp_type)->base;
                    }
                default:
                    rexp_type = analyze_exp(rexp, scope);
                    if (!typecmp(lexp_type, rexp_type)) {
                        if (op->type == YY_ASSIGNOP) {
                            SEMA_ERROR_MSG(5, op->lineno, "Type mismatched for assignment.");
                            // TODO: Return int as default, is this right?
                            return global_int;
                        } else {
                            SEMA_ERROR_MSG(7, op->lineno, "Type mismatched for operands");
                            return NULL;
                        }
                    } else if (op->type != YY_ASSIGNOP &&
                                (!typecmp(lexp_type, global_int)
                                 && !typecmp(lexp_type, global_float))) {
                        SEMA_ERROR_MSG(7, op->lineno, "The type is not allowed in operation '%s'", op->val.s);
                        return global_int;
                    } else if (op->type == YY_ASSIGNOP && !is_lval(lexp)) {
                        SEMA_ERROR_MSG(6, op->lineno, "The left-hand side of an assignment must be a variable.");
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

CmmType *analyze_stmt(node_t *stmt, CmmType *inh_func_type, int scope) {
    assert(stmt->type == YY_Stmt);
    node_t *first = stmt->child;
    CmmType *stmt_type = NULL;
    node_t *exp = NULL, *sub_stmt = NULL;
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
            analyze_stmt(sub_stmt, inh_func_type, scope);
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
                SEMA_ERROR_MSG(8, exp->lineno, "Type mismatched for return.");
            }
            break;
        default:
            LOG("Awful statement");
            assert(0);
    }

    return stmt_type;
}

CmmType *analyze_stmtlist(node_t *stmtlist, CmmType *inh_func_type, int scope) {
    node_t *stmt = stmtlist->child;
    CmmType *return_type = analyze_stmt(stmt, inh_func_type, scope);
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
CmmType *analyze_compst(node_t *compst, CmmType *inh_func_type, int scope) {
    assert(compst->type == YY_CompSt);

    node_t *list = compst->child->sibling;

    CmmType *return_type = NULL;

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
void analyze_extdef(node_t *extdef) {
    assert(extdef->type == YY_ExtDef);

    node_t *spec = extdef->child;
    CmmType *type = analyze_specifier(spec);
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

void analyze_program(node_t *prog) {
    node_t *extdef = prog->child->child;
    while (extdef != NULL) {
        analyze_extdef(extdef);
        if (extdef->sibling != NULL) {
            extdef = extdef->sibling->child;
        } else {
            break;
        }
    }
}

void print_expr(const node_t *nd, FILE *fp) {
    if (nd == NULL) {
        return;
    }
    if (nd->type == YY_Exp) {
        print_expr(nd->child, fp);
        if (nd->sibling == NULL) {
            return;
        }
        const node_t *op = nd->sibling;
        switch (op->type) {
            case YY_ASSIGNOP: case YY_RELOP:
            case YY_AND: case YY_OR:
            case YY_PLUS: case YY_MINUS:
            case YY_STAR: case YY_DIV:
                fprintf(fp, " %s ", op->val.s);
                print_expr(op->sibling->child, fp);
                return;
            case YY_COMMA:
                fputs(",", fp); 
                return;
            case YY_LB:
                fputs("[", fp);
                print_expr(op->sibling->child, fp);
                fputs("]", fp);
                return;
            case YY_DOT:
                fputs(".", fp);
                fputs(op->sibling->val.s, fp);
                return;
            default:
                assert(0);
        }

    } else {
        switch (nd->type) {
            case YY_LP:
                fputs("(", fp);
                print_expr(nd->sibling->child, fp);
                fputs(")", fp);
                return;
            case YY_MINUS: case YY_NOT:
                fprintf(fp, "%s", nd->val.s);
                print_expr(nd->sibling->child, fp);
                return;
            case YY_INT:
                fprintf(fp, "%d", nd->val.i);
                return;
            case YY_FLOAT:
                fprintf(fp, "%f", nd->val.f);
                return;
            case YY_ID:
                fputs(nd->val.s, fp);
                if (nd->sibling != NULL) {
                    fputs("(", fp);
                    nd = nd->sibling->sibling;
                    while (nd->type == YY_Args) {
                        print_expr(nd->child, fp);
                        if (nd->child->sibling != NULL) {
                            nd = nd->child->sibling->sibling;
                        } else {
                            break;
                        }
                    }
                    fputs(")", fp);
                }
                return;
            default:
                assert(0);
        }
    }
}

const char *expr_to_s(node_t *exp) {
    static char *s = NULL;
    assert(exp->type == YY_Exp);
    FILE *tmp = tmpfile();
    print_expr(exp->child, tmp);
    int len = (int)ftell(tmp);
    if (s != NULL) {
        free(s);
    }
    s = (char *)malloc((size_t)(len + 1));
    rewind(tmp);
    fgets(s, len + 1, tmp);
    fclose(tmp);
    return s;
}

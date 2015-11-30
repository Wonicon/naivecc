#include "node.h"
#include "yytname.h"
#include "cmm_symtab.h"
#include <stdlib.h>
#include <assert.h>
#include <memory.h>


extern int is_check_return;


//
// node constructor, wrapping some initialization
//
Node new_node(enum YYTNAME_INDEX type) {
    Node p = (Node)malloc(sizeof(struct Node_));
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
void free_node(Node nd) {
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
void midorder(Node nd, int level) {
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
void puts_tree(Node nd) {
    midorder(nd, 0);
}


typedef struct {
    Type *type;
    int lineno;
    const char *name;
} var_t;

#define STRUCT_SCOPE (-10086)

static var_t default_attr = {NULL, 0, NULL};

#define SEMA_ERROR_MSG(type, lineno, fmt, ...) \
fprintf(stderr, "Error type %d at Line %d: " fmt "\n", type, lineno, ## __VA_ARGS__)

//
// Some pre-declaration
//
Type * analyze_specifier(const Node );     // required by analyze_[def|paramdec|extdef]
Type *analyze_exp(Node exp, int scope);    // required by check_param_lsit, analyze_vardec
Type *analyze_compst(Node , Type *, int);  // required by analyze_stmt


var_t analyze_vardec(Node vardec, Type *inh_type) {
    assert(vardec->type == YY_VarDec);

    if (vardec->child->type == YY_ID) {
        // Identifier
        Node id = vardec->child;
        var_t return_attr = { inh_type, id->lineno, id->val.s };
        return return_attr;
    } else if (vardec->child->type == YY_VarDec) {
        // Array
        // Now we have seen the most distanced dimension of an array
        // It should be a base type of the prior vardec, so we generate
        // a new array type first, and pass it as an inherit attribute
        // to the sub vardec.
        Node sub_vardec = vardec->child;
        Type *temp_array = new_type(CMM_ARRAY, NULL, inh_type, NULL);
        temp_array->size = sub_vardec->sibling->sibling->val.i;
        temp_array->type_size = temp_array->size * inh_type->type_size;
        return analyze_vardec(sub_vardec, temp_array);
    } else {
        assert(0);
        return default_attr;
    }
}


// next comes from the declist behind
var_t analyze_dec(Node dec, Type *type, int scope) {
    assert(dec->type == YY_Dec);

    Node vardec = dec->child;
    var_t var = analyze_vardec(vardec, type);

    // TODO is field need insert symbol?
    if (scope != STRUCT_SCOPE) {
        if (insert(var.name, var.type, dec->child->child->lineno, scope) < 0) {
            if (scope != STRUCT_SCOPE) {
                SEMA_ERROR_MSG(3, dec->child->child->lineno, "Redefined variable \"%s\".", var.name);
            }
        // TODO handle memory leak
        }
    }

    // Assignment / Initialization
    if (vardec->sibling != NULL) {
        if (scope == STRUCT_SCOPE) {  // Field does not allow assignment
            SEMA_ERROR_MSG(15, dec->lineno, "Initialization in the structure definition is not allowed");
        } else {  // Assignment consistency check
            Node exp = vardec->sibling->sibling;
            const Type *exp_type = analyze_exp(exp, scope);
            if (!typecmp(exp_type, type)) {
                SEMA_ERROR_MSG(5, vardec->sibling->lineno, "Type mismatch");
            }
        }
    }

    return var;
}


// next comes from the deflist behind
Type *analyze_declist(Node declist, Type *type, int scope) {
    assert(declist->type == YY_DecList);
    Node dec = declist->child;

    // Analyze the dec first in order to have a right symbol registering sequence.
    var_t var = analyze_dec(dec, type, scope);

    // Handle declist if it exists.
    // We do assignment here ignoring whether the declist is in the struct
    // because this behavior has no side effects. We will discard the next_field
    // in the end of this function if we are in CompSt declist!
    Type *next_field;
    if (dec->sibling != NULL) {
        // declist -> dec comma declist
        Node sub_list = dec->sibling->sibling;
        next_field = analyze_declist(sub_list, type, scope);
    } else {
        next_field = NULL;
    }

    // If the declist is not in struct, then we just return NULL.
    if (scope == STRUCT_SCOPE) {
        Type *field = new_type(CMM_FIELD, var.name, var.type, next_field);
        field->lineno = var.lineno;
        return field;
    } else {
        return NULL;
    }
}


// next comes from the deflist behind
Type *analyze_def(Node def, int scope) {
    assert(def->type == YY_Def);

    // Get body symbols
    Node spec = def->child;
    Node declist = spec->sibling;

    // Handle Specifier
    Type *type = analyze_specifier(spec);
    assert(type->type_size != 0);

    // Handle DecList and get a field list if we are in struct scope
    return analyze_declist(declist, type, scope);
}


Type *analyze_deflist(const Node deflist, int scope) {
    assert(deflist->type == YY_DefList);

    // Resolve the first definition
    Node def = deflist->child;
    Type *field = analyze_def(def, scope);
    // Resolve the rest definitions if exists
    Node sub_list = def->sibling;
    Type *sub_field = sub_list == NULL ? NULL : analyze_deflist(sub_list, scope);

    // If we are in a struct scope, these CmmField will be discarded.

    if (scope == STRUCT_SCOPE) {  // Link up the field
        Type *pretail = field;
        while (pretail->link != NULL) {
            pretail = pretail->link;
        }
        pretail->link = sub_field;
    } else {
        assert(field == NULL && sub_field == NULL);
    }

    return field;
}


Type *analyze_struct_spec(const Node struct_spec) {
    assert(struct_spec->type == YY_StructSpecifier);

    Node body = struct_spec->child->sibling;  // Jump tag 'struct'

    //
    // Check whether this is a struct definition or declaration
    //
    if (body->type == YY_Tag) {
        //
        // Declaration
        //
        Node id = body->child;
        sym_ent_t *ent = query(id->val.s, STRUCT_SCOPE);
        if (ent == NULL || ent->type->class != CMM_TYPE || ent->type->meta->class != CMM_STRUCT) {
            // The symbol is not found
            SEMA_ERROR_MSG(17, body->lineno, "Undefined struct name '%s'", body->child->val.s);
            // Return a fake structure type, if the symbol can be store in the symbol table or lists,
            // everything will be OK. But if the symbol has name collision and is finally discarded,
            // then a memroy leak will occur.
            return new_type(CMM_STRUCT, id->val.s, NULL, NULL);
        } else {
            // ent is meta type
            // the real type is inside.
            return ent->type->meta;
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

    // Construct struct
    Type *this = new_type(CMM_STRUCT, name, NULL, NULL);
    this->lineno = body->child->lineno;

    // Get field list
    this->field = analyze_deflist(body, STRUCT_SCOPE);

    int struct_size = 0;
    // Check field name collision and add up the size and offset
    for (Type *outer = this->field; outer != NULL; outer = outer->link) {
        if (outer->name[0] == '\0') {
            // This is an anonymous field, which means that it has been detected as a duplicated field.
            continue;
        }

        if (insert(outer->name, outer->base, outer->lineno, STRUCT_SCOPE) < 0) {
            SEMA_ERROR_MSG(3, outer->lineno, "Redefined field \"%s\".", outer->name);
        }

        // 记录偏移量并累加大小
        outer->offset = struct_size;
        struct_size += outer->meta->type_size;

        Type *inner = outer->link;
        while (inner != NULL) {
            if (!strcmp(outer->name, inner->name)) {
                SEMA_ERROR_MSG(15, inner->lineno,
                               "Duplicated decleration of field %s, the previous one is at %d",
                               outer->name, outer->lineno);
                inner->name = "";
            }
            inner = inner->link;
        }
    }
    this->type_size = struct_size;

    // Use the struct name to register in the symbol table.
    // Ignore the struct with empty tag.

    int insert_rst = 1;
    if (strcmp(this->name, "") != 0) {
        Type *meta = new_type(CMM_TYPE, this->name, this, NULL);
        meta->lineno = struct_spec->lineno;
        insert_rst = insert(meta->name, meta, struct_spec->lineno, 0);
    }

    if (insert_rst < 1) {
        SEMA_ERROR_MSG(16, struct_spec->lineno, "Duplicated name \"%s\".", name);
    }

    return this;
}


Type * analyze_specifier(const Node specifier) {
    if (specifier->child->type == YY_TYPE) {
        const char *type_name = specifier->child->val.s;
        if (!strcmp(type_name, BASIC_INT->name)) {
            return BASIC_INT;
        } else if (!strcmp(type_name, BASIC_FLOAT->name)) {
            return BASIC_FLOAT;
        } else {
            assert(0);
        }
    } else {
        return analyze_struct_spec(specifier->child);
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
var_t analyze_paramdec(Node paramdec) {
    assert(paramdec->type == YY_ParamDec);
    Node specifier = paramdec->child;
    Node vardec = specifier->sibling;
    Type *spec = analyze_specifier(specifier);
    var_t var = analyze_vardec(vardec, spec);
    int insert_ret = insert(var.name, var.type, paramdec->lineno, -1);
    if (insert_ret < 1) {
        SEMA_ERROR_MSG(3, vardec->lineno, "Duplicated variable definition of '%s'", var.name);
    }
    return var;
}

// Then we should link the paramdec's type up to form a param type list.
// varlist should return a type of CmmParam, and the generation of CmmParam occurs here.
Type *analyze_varlist(Node varlist) {
    Node paramdec = varlist->child;
    Node sub_varlist = paramdec->sibling == NULL ? NULL : paramdec->sibling->sibling;
    assert(paramdec->type == YY_ParamDec);
    assert(sub_varlist == NULL || sub_varlist->type == YY_VarList);

    var_t param_attr = analyze_paramdec(paramdec);
    Type *sub_list = sub_varlist == NULL ? NULL : analyze_varlist(sub_varlist);
    Type *param = new_type(CMM_PARAM, param_attr.name, param_attr.type, sub_list);

    return param;
}

// Analyze the function and register it
//   FunDec -> ID LP VarList RP
//   FunDec -> ID LP RP
const char *
analyze_fundec(Node fundec, Type *inh_type) {
    assert(fundec->type == YY_FunDec);
    Node id = fundec->child;
    Node varlist = id->sibling->sibling;

    // Get identifier
    const char *name = id->val.s;

    // Get param list if exists
    Type *param_list = NULL;
    if (varlist->type == YY_VarList) {
        param_list = analyze_varlist(varlist);
    } else {
        assert(varlist->type == YY_RP);
    }

    // Generate function symbol
    Type *func = new_type(CMM_FUNC, name, inh_type, param_list);

    if (insert(func->name, func, fundec->lineno, -1) < 0) {
        SEMA_ERROR_MSG(4, fundec->lineno, "Redefined function \"%s\"", func->name);
        // TODO handle memory leak!
    }

    return name;
}


void analyze_extdeclist(Node extdeclist, Type *inh_type) {
    assert(extdeclist->type == YY_ExtDecList);

    Node vardec = extdeclist->child;
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


//
// exp only return its type.
// We use stmt analyzer to check the return type.
//
static inline int is_lval(const Node exp)
{
    assert(exp != NULL && exp->type == YY_Exp);

    if (exp->child->type == YY_ID)
    {
        // Avoid function name and type name.
        // An array directly found in the symbol table is a constant variable
        // which cannot be assigned.
        sym_ent_t *ent = query(exp->child->val.s, 0);
        // TODO Ugly conditions
        return ent != NULL && ent->type->class != CMM_FUNC && ent->type->class != CMM_TYPE && ent->type->class != CMM_ARRAY;
    }
    else 
    {
        Node follow = exp->child->sibling;
        return (follow != NULL && (follow->type == YY_LB || follow->type == YY_DOT));
    }
}


static int check_param_list(const Type *param, Node args, int scope) {
    if (param == NULL && args->type == YY_RP) {
        return 1;
    } else if ((param == NULL && args->type != YY_RP) ||
            (param != NULL && args->type == YY_RP)) {
        SEMA_ERROR_MSG(9, args->lineno, "parameter mismatch");
        return 0;
    } else {
        assert(args->type == YY_Args);
        Node arg = NULL;
        while (param != NULL) {
            arg = args->child;
            const Type *param_type = analyze_exp(arg, scope);
            if (!typecmp(param_type, param->base)) {
                SEMA_ERROR_MSG(9, arg->lineno, "parameter type mismatches");
            }
            param = param->link;
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

const char *expr_to_s(Node exp);
// TODO Split this bulk function!
Type *analyze_exp(Node exp, int scope) {
    assert(exp->type == YY_Exp);

    Node id, lexp, rexp, op;
    Type *lexp_type = NULL, *rexp_type = NULL;
    sym_ent_t *query_result = NULL;
    switch (exp->child->type) {
        case YY_INT:
            return BASIC_INT;
        case YY_FLOAT:
            return BASIC_FLOAT;
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
            id = exp->child;
            // Regardless of whether it is an id or func call, let's query it first~
            query_result = query(id->val.s, scope);

            // Handle function call
            if (id->sibling != NULL && id->sibling->type == YY_LP) {
                if (query_result == NULL) {
                    SEMA_ERROR_MSG(2, id->lineno, "Undefined function \"%s\".", id->val.s);
                    return NULL;
                } else if (query_result->type->class != CMM_FUNC) {
                    SEMA_ERROR_MSG(11, id->lineno, "\"%s\" is not a function.", id->val.s);
                    return query_result->type;
                } else {
                    // Error report in the check.
                    check_param_list(query_result->type->param, id->sibling->sibling, scope);
                    // Return the return type anyway, because using a wrong function and giving wrong
                    // parameters are problems of two aspects.
                    return query_result->type->ret;
                }
            } else {
                // Handle id
                if (query_result == NULL) {
                    SEMA_ERROR_MSG(1, id->lineno, "Undefined variable \"%s\"", id->val.s);
                    return NULL;
                } else if (query_result->type->class == CMM_TYPE) {
                    SEMA_ERROR_MSG(1, id->lineno, "Cannot resovle variable \"%s\"", id->val.s);
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
            // right exp can be id directly, so be cautious.
            switch (op->type) {
                case YY_DOT:
                    id = rexp;
                    if (lexp_type->class != CMM_STRUCT) {
                        SEMA_ERROR_MSG(13, op->lineno, "The left identifier of '.' is not a struct");
                        return NULL;
                    } else if ((rexp_type = query_field(lexp_type, id->val.s)) == NULL) {
                        SEMA_ERROR_MSG(14, id->lineno,
                                       "Undefined field \"%s\" in struct \"%s\".",
                                       id->val.s, lexp_type->name);
                        return NULL;
                    } else {
                        // Commonly we return the type of the field
                        return rexp_type;
                    }
                case YY_LB:
                    rexp_type = analyze_exp(rexp, scope);
                    if (rexp_type != NULL && rexp_type->class != CMM_INT) {
                        SEMA_ERROR_MSG(12, rexp->lineno, "%s is not a integer", expr_to_s(rexp));
                    }

                    // If lexp_type is null, it means that an semantic error has occurred, then we can ignore the
                    // consecutive errors.
                    if (lexp_type == NULL) {
                        return NULL;
                    } else if (lexp_type->class != CMM_ARRAY) {
                        SEMA_ERROR_MSG(10, lexp->lineno, " \"%s\" is not an array.", expr_to_s(lexp));
                        return NULL;
                    } else {
                        return lexp_type->base;
                    }
                default:
                    // Normal two operands operation
                    rexp_type = analyze_exp(rexp, scope);
                    if (!typecmp(lexp_type, rexp_type)) {
                        // Type mismatched
                        if (op->type == YY_ASSIGNOP) {
                            SEMA_ERROR_MSG(5, op->lineno, "Type mismatched for assignment.");
                            // TODO: Return int as default, is this right?
                            return BASIC_INT;
                        } else {
                            SEMA_ERROR_MSG(7, op->lineno, "Type mismatched for operands");
                            return NULL;
                        }
                    } else if (op->type != YY_ASSIGNOP &&
                                (!typecmp(lexp_type, BASIC_INT)
                                 && !typecmp(lexp_type, BASIC_FLOAT))) {
                        // Type matched, but cannot be operated
                        SEMA_ERROR_MSG(7, op->lineno, "The type is not allowed in operation '%s'", op->val.s);
                        return NULL;
                    } else if (op->type == YY_ASSIGNOP && (!is_lval(lexp))) {
                        // Type matched, but the left operand cannot be assigned.
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


//
// Statement analyzer will check condition type and return type
// which cannot be judged in expression analyzer. In other cases
// it is just a router, and receives the type of the composite 
// statement, which may contain a return statement.
//
Type *analyze_stmt(Node stmt, Type *inh_func_type, int scope) {
    assert(stmt->type == YY_Stmt);

    Node first = stmt->child;
    Node exp;
    Node sub_stmt;
    Type *cond_type;  // Used to check condition type
    Type *return_type = NULL;
    Type *else_return_type;
    switch (first->type) {
        case YY_Exp:
            analyze_exp(first, scope);
            break;
        case YY_CompSt:
            return_type = analyze_compst(first, inh_func_type, scope);
            break;
        case YY_IF:
            exp = first->sibling->sibling;
            cond_type = analyze_exp(exp, scope);
            if (!typecmp(cond_type, BASIC_INT)) {
                SEMA_ERROR_MSG(7, exp->lineno, "The condition expression must return int");
            }
            sub_stmt = exp->sibling->sibling;
            return_type = analyze_stmt(sub_stmt, inh_func_type, scope);
            if (sub_stmt->sibling != NULL) {
                // ELSE
                else_return_type = analyze_stmt(sub_stmt->sibling->sibling, inh_func_type, scope);
                return_type = (return_type != NULL) ? else_return_type : return_type;
            } else {
                return_type = NULL;
            }
            break;
        case YY_WHILE:
            exp = first->sibling->sibling;
            cond_type = analyze_exp(exp, scope);
            if (!typecmp(cond_type, BASIC_INT)) {
                SEMA_ERROR_MSG(7, exp->lineno, "The condition expression must return int");
            }
            sub_stmt = exp->sibling->sibling;
            analyze_stmt(sub_stmt, inh_func_type, scope);
            // We suppose the loop can jump out, so whether the compst returned decided by the following statement.
            break;
        case YY_RETURN:
            exp = first->sibling;
            return_type = analyze_exp(exp, scope);
            if (!typecmp(return_type, inh_func_type)) {
                SEMA_ERROR_MSG(8, exp->lineno, "Type mismatched for return.");
            }
            break;
        default:
            LOG("Awful statement");
            assert(0);
    }

    return return_type;
}


//
// Traverse the stmtlist
//
Type *analyze_stmtlist(Node stmtlist, Type *inh_func_type, int scope) {
    Node stmt = stmtlist->child;
    Type *return_type = analyze_stmt(stmt, inh_func_type, scope);
    Type *sub_return_type;
    if (stmt->sibling != NULL) {
        sub_return_type = analyze_stmtlist(stmt->sibling, inh_func_type, scope);
        return_type = (return_type != NULL) ? return_type : sub_return_type;
    }
    return return_type;
}


//
// Second level analyzer router : CompSt
// Currently the scope has no thing to do with analysis
//
Type *analyze_compst(Node compst, Type *inh_func_type, int scope) {
    assert(compst->type == YY_CompSt);

    Node list = compst->child->sibling;

    Type *return_type = NULL;
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
void analyze_extdef(Node extdef) {
    assert(extdef->type == YY_ExtDef);

    Node spec = extdef->child;
    Type *type = analyze_specifier(spec);
    Type *return_type;
    const char *func_name;
    switch (spec->sibling->type) {
        case YY_ExtDecList:
            analyze_extdeclist(spec->sibling, type);
            break;
        case YY_FunDec:
            func_name = analyze_fundec(spec->sibling, type);
            return_type = analyze_compst(spec->sibling->sibling, type, 0);
            if (is_check_return && return_type == NULL) {
                fprintf(stderr, "Unreturned branch in function \"%s\"!\n", func_name);
            }
            break;
        case YY_SEMI:
            LOG("Well, I think this is used for struct");
            break;
        default:
            LOG("WTF");
    }
}


void analyze_program(Node prog) {
    Node extdef = prog->child->child;
    while (extdef != NULL) {
        analyze_extdef(extdef);
        if (extdef->sibling != NULL) {
            extdef = extdef->sibling->child;
        } else {
            break;
        }
    }
}


// TODO Split this ugly function!
void print_expr(Node nd, FILE *fp) {
    if (nd == NULL) {
        return;
    }
    if (nd->type == YY_Exp) {
        print_expr(nd->child, fp);
        if (nd->sibling == NULL) {
            return;
        }
        Node op = nd->sibling;
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


const char *expr_to_s(Node exp) {
    static char *s = NULL;

    assert(exp->type == YY_Exp);
    FILE *tmp = tmpfile();
    print_expr(exp->child, tmp);
    int len = (int)ftell(tmp);  // long long -> int
    if (s != NULL) {
        free(s);
    }
    s = (char *)malloc((size_t)(len + 1));
    memset(s, 0, (size_t)(len + 1));
    rewind(tmp);
    fgets(s, len + 1, tmp);
    fclose(tmp);
    return s;
}

#include "semantic.h"
#include "cmm-type.h"
#include "cmm-symtab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


typedef struct {
    Type *type;
    int lineno;
    const char *name;
} var_t;


typedef void (*ast_visitor)(Node);
static ast_visitor sema_visitors[];
#define sema_visit(x) sema_visitors[x->tag](x)


bool semantic_error = false;


#define STRUCT_SCOPE (-10086)


static var_t default_attr = {NULL, 0, NULL};


#define SEMA_ERROR_MSG(type, lineno, fmt, ...) do { semantic_error = true; \
fprintf(stderr, "Error type %d at Line %d: " fmt "\n", type, lineno, ## __VA_ARGS__); } while(0)


//
// Some pre-declaration
//
Type *analyze_specifier(const Node);       // required by analyze_[def|paramdec|extdef]
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
            Node exp = vardec->sibling;
            const Type *exp_type = analyze_exp(exp, scope);
            if (!typecmp(exp_type, type)) {
                SEMA_ERROR_MSG(5, vardec->sibling->lineno, "Type mismatch");
            }
        }
    }

    return var;
}


static Type *
analyze_field_dec(Node dec, Type *type, int scope) {
    if (dec == NULL) {
        return NULL;
    }

    assert(dec->type == YY_Dec);

    // Analyze the dec first in order to have a right symbol registering sequence.
    var_t var = analyze_dec(dec, type, scope);

    // Recursively analyze field to insert the node before head in order.
    Type *next_field_list = analyze_field_dec(dec, type, scope);
    Type *new_field = new_type(CMM_FIELD, var.name, var.type, next_field_list);

    new_field->lineno = var.lineno;

    return new_field;
}


Type *analyze_def(Node def, int scope) {
    assert(def->type == YY_Def);

    // Handle Specifier
    Node spec = def->child;
    Type *type = analyze_specifier(spec);
    assert(type->type_size != 0);

    // Handle DecList and get a field list if we are in struct scope
    if (scope == STRUCT_SCOPE) {
        return analyze_field_dec(spec->sibling, type, scope);
    }
    else {
        for (Node dec = spec->sibling; dec != NULL; dec = dec->sibling) {
            analyze_dec(dec, type, scope);
        }
        return NULL;
    }
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


Type *
analyze_exp_is_unary(Node exp, int scope)
{
    return analyze_exp(exp->child, scope);
}


Type *
analyze_exp_is_binary(Node exp, int scope)
{
    Node left = exp->child;
    Node right = left->sibling;

    Type *left_type = analyze_exp(left, scope);
    Type *right_type = analyze_exp(right, scope);

    if (!typecmp(left_type, right_type)) {
        // Type mismatched
        SEMA_ERROR_MSG(7, exp->lineno, "Type mismatched for operands");
        return NULL;
    }
    else if (!typecmp(left_type, BASIC_INT) && !typecmp(left_type, BASIC_FLOAT)) {
        // Type matched, but cannot be operated
        SEMA_ERROR_MSG(7, exp->lineno, "The type is not allowed in operation '%s'", exp->val.operator);
        return NULL;
    }
    else {
        return left_type;
    }
}


Type *
analyze_exp_is_assign(Node exp, int scope)
{
    Node left = exp->child;
    Node right = left->sibling;

    Type *type_l = analyze_exp(left, scope);
    Type *type_r = analyze_exp(right, scope);

    if (!typecmp(type_l, type_r)) {
        SEMA_ERROR_MSG(5, exp->lineno, "Type mismatched for assignment.");
    }
    else if (!is_lval(left)) {
        SEMA_ERROR_MSG(6, exp->lineno, "The left-hand side of an assignment must be a variable.");
    }
    
    return type_l;
}


Type *
analyze_exp_is_exp_idx(Node exp, int scope)
{
    Node lexp = exp->child;
    Node rexp = lexp->sibling;

    Type *ltype = analyze_exp(lexp, scope);
    Type *rtype = analyze_exp(rexp, scope);

    if (rtype != NULL && rtype->class != CMM_INT) {
        SEMA_ERROR_MSG(12, rexp->lineno, "expression is not a integer");
    }

    // If lexp_type is null, it means that an semantic error has occurred, then we can ignore the
    // consecutive errors.
    if (ltype == NULL) {
        return NULL;
    }
    else if (ltype->class != CMM_ARRAY) {
        SEMA_ERROR_MSG(10, lexp->lineno, "expression is not an array.");
        return NULL;
    }
    else {
        return ltype->base;
    }
}


Type *analyze_exp(Node exp, int scope) {
    assert(exp->type == YY_Exp);

    switch (exp->tag) {
        case EXP_is_EXP:
            return analyze_exp(exp->child, scope);
        case EXP_is_INT:
            return BASIC_INT;
        case EXP_is_FLOAT:
            return BASIC_FLOAT;
        case EXP_is_ID:
            {
                Node id = exp->child;
                assert(id->type == YY_ID);

                sym_ent_t *query_result = query(id->val.s, scope);

                if (query_result == NULL) {
                    SEMA_ERROR_MSG(1, id->lineno, "Undefined variable \"%s\"", id->val.s);
                    return NULL;
                }
                else if (query_result->type->class == CMM_TYPE) {
                    SEMA_ERROR_MSG(1, id->lineno, "Cannot resovle variable \"%s\"", id->val.s);
                    return NULL;
                }
                else {
                    return query_result->type;
                }
            }
        case EXP_is_UNARY:
            return analyze_exp_is_unary(exp, scope);
        case EXP_is_BINARY:
            return analyze_exp_is_binary(exp, scope);
        case EXP_is_ID_ARG:
            {
                Node id = exp->child;
                assert(id->type == YY_ID);
                assert(id->sibling->type == YY_Args);

                sym_ent_t *query_result = query(id->val.s, scope);

                if (query_result == NULL) {
                    SEMA_ERROR_MSG(2, id->lineno, "Undefined function \"%s\".", id->val.s);
                    return NULL;
                }
                else if (query_result->type->class != CMM_FUNC) {
                    SEMA_ERROR_MSG(11, id->lineno, "\"%s\" is not a function.", id->val.s);
                    return query_result->type;
                }
                else {
                    // Error report in the check.
                    check_param_list(query_result->type->param, id->sibling, scope);
                    // Return the return type anyway, because using a wrong function and giving wrong
                    // parameters are problems of two aspects.
                    return query_result->type->ret;
                }
            }
        case EXP_is_EXP_FIELD:
            {
                Type *struct_type = analyze_exp(exp->child, scope);

                if (struct_type->class != CMM_STRUCT) {
                    SEMA_ERROR_MSG(13, exp->lineno, "The left identifier of '.' is not a struct");
                    return NULL;
                }
                else {
                    Node field = exp->child->sibling;
                    Type *field_type = query_field(struct_type, field->val.s);
                    if (field_type == NULL) {
                        SEMA_ERROR_MSG(14, field->lineno, "Undefined field \"%s\" in struct \"%s\".",
                                field->val.s, struct_type->name);
                        return NULL;
                    }
                    else {
                        return field_type;
                    }
                }
            }
        case EXP_is_EXP_IDX:
            return analyze_exp_is_exp_idx(exp, scope);
        default:
            printf("%d\n", exp->tag);
            assert(0);
            return NULL;
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
            if (return_type == NULL) {
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


static void extdef_is_spec_extdec(Node extdef)
{
    assert(extdef->tag == EXTDEF_is_SPEC_EXTDEC);
    
    Node spec = extdef->child;
    Node extdec = spec->sibling;

    sema_visit(spec);
    sema_visit(extdec);
}


static void extdef_is_spec(Node extdef)
{
    assert(extdef->tag == EXTDEF_is_SPEC);

    Node spec = extdef->child;

    sema_visit(spec);
}


static void extdef_is_spec_func_compst(Node extdef)
{
    assert(extdef->tag == EXTDEF_is_SPEC_FUNC_COMPST);

    Node spec = extdef->child;
    Node func = spec->sibling;
    Node compst = func->sibling;

    sema_visit(spec);
    sema_visit(func);
    sema_visit(compst);
}


static void prog_is_extdef(Node prog)
{
    assert(prog->tag == PROG_is_EXTDEF);

    Node extdef = prog->child;
    while (extdef != NULL) {
        analyze_extdef(extdef);
        extdef = extdef->sibling;
    }
}


static ast_visitor sema_visitors[] = {
    [PROG_is_EXTDEF]             = prog_is_extdef,
    [EXTDEF_is_SPEC_EXTDEC]      = extdef_is_spec_extdec,
    [EXTDEF_is_SPEC]             = extdef_is_spec,
    [EXTDEF_is_SPEC_FUNC_COMPST] = extdef_is_spec_func_compst
};


void analyze_program(Node prog)
{
    sema_visit(prog);
}


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
            sema_visit(exp);
            const Type *exp_type = exp->sema.type;
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
    sema_visit(spec);
    Type *type = spec->sema.type;
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


static void spec_is_type(Node spec)
{
    const char *type_name = spec->child->val.s;
    if (!strcmp(type_name, BASIC_INT->name)) {
        spec->sema.type = BASIC_INT;
    }
    else if (!strcmp(type_name, BASIC_FLOAT->name)) {
        spec->sema.type = BASIC_FLOAT;
    }
    else {
        PANIC("Unexpected type");
    }
}


static void spec_is_struct(Node spec)
{
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

    sema_visit(specifier);

    Type *spec = specifier->sema.type;
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


// Analyze the function signiture, include function's name and parameter list.
static void func_is_id_var(Node fundec)
{
    assert(fundec->tag == FUNC_is_ID_VAR);

    Node id = fundec->child;
    Node var = id->sibling;

    // Get identifier
    const char *name = id->val.s;

    // Get param list if exists
    Type *param_list = (var != NULL) ? analyze_varlist(var) : NULL;

    // Generate function symbol
    Type *func = new_type(CMM_FUNC, name, fundec->sema.type, param_list);

    if (insert(func->name, func, fundec->lineno, -1) < 0) {
        SEMA_ERROR_MSG(4, fundec->lineno, "Redefined function \"%s\"", func->name);
        // TODO handle memory leak!
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
            sema_visit(arg);
            Type *param_type = arg->sema.type;
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


static void exp_is_unary(Node exp)
{
    sema_visit(exp->child);

    Type *type = exp->child->sema.type;

    if (typecmp(type, BASIC_INT)) {
        exp->sema.type = type;
    }
    else if (typecmp(type, BASIC_FLOAT) && exp->val.operator[0] == '!') {
        SEMA_ERROR_MSG(7, exp->lineno, "\"!\" cannot cast on float");
    }
    else {
        SEMA_ERROR_MSG(8, exp->lineno, "\"%s\" cannot case on unbasic type", exp->val.operator);
    }
}


static void exp_is_binary(Node exp)
{
    Node lexp = exp->child;
    Node rexp = lexp->sibling;

    sema_visit(lexp);
    sema_visit(rexp);

    Type *ltype = lexp->sema.type;
    Type *rtype = rexp->sema.type;

    if (!typecmp(ltype, rtype)) {
        // Type mismatched
        SEMA_ERROR_MSG(7, exp->lineno, "Type mismatched for operands");
    }
    else if (!typecmp(ltype, BASIC_INT) && !typecmp(ltype, BASIC_FLOAT)) {
        // Type matched, but cannot be operated
        SEMA_ERROR_MSG(7, exp->lineno, "The type is not allowed in operation '%s'", exp->val.operator);
    }
    else {
        exp->sema.type = ltype;
    }
}


static void exp_is_assign(Node exp)
{
    assert(exp->tag == EXP_is_ASSIGN);

    Node lexp = exp->child;
    Node rexp = lexp->sibling;

    sema_visit(lexp);
    sema_visit(rexp);

    if (!typecmp(lexp->sema.type, rexp->sema.type)) {
        SEMA_ERROR_MSG(5, exp->lineno, "Type mismatched for assignment.");
    }
    else if (!is_lval(lexp)) {
        SEMA_ERROR_MSG(6, exp->lineno, "The left-hand side of an assignment must be a variable.");
    }

    exp->sema.type = lexp->sema.type;
}


static void exp_is_exp_idx(Node exp)
{
    Node lexp = exp->child;
    Node rexp = lexp->sibling;

    sema_visit(lexp);
    sema_visit(rexp);

    if (rexp->sema.type != NULL && rexp->sema.type->class != CMM_INT) {
        SEMA_ERROR_MSG(12, rexp->lineno, "expression is not a integer");
    }

    // If lexp_type is null, it means that an semantic error has occurred, then we can ignore the
    // consecutive errors.
    if (exp->sema.type != NULL) {
        if (exp->sema.type->class != CMM_ARRAY) {
            SEMA_ERROR_MSG(10, lexp->lineno, "expression is not an array.");
        }
        else {
            exp->sema.type = lexp->sema.type->base;
        }
    }
}


static void exp_is_id_arg(Node exp)
{
    assert(exp->tag == EXP_is_ID_ARG);

    Node id = exp->child;

    sym_ent_t *query_result = query(id->val.s, 1);

    if (query_result == NULL) {
        SEMA_ERROR_MSG(2, id->lineno, "Undefined function \"%s\".", id->val.s);
    }
    else if (query_result->type->class != CMM_FUNC) {
        SEMA_ERROR_MSG(11, id->lineno, "\"%s\" is not a function.", id->val.s);
    }
    else {
        // Error report in the check
        check_param_list(query_result->type->param, id->sibling, 1);
        // Return the return type while ignoring errors in arguments
        exp->sema.type = query_result->type->ret;
    }
}


static void exp_is_exp_field(Node exp)
{
    assert(exp->tag == EXP_is_EXP_FIELD);

    Node struc = exp->child;

    sema_visit(struc);

    if (struc->sema.type->class != CMM_STRUCT) {
        SEMA_ERROR_MSG(13, exp->lineno, "The left identifier of '.' is not a struct");
    }
    else {
        Node field = struc->sibling;
        Type *field_type = query_field(struc->sema.type, field->val.s);
        if (field_type == NULL) {
            SEMA_ERROR_MSG(14, field->lineno, "Undefined field \"%s\" in struct \"%s\".",
                    field->val.s, struc->sema.type->name);
        }
        else {
            exp->sema.type = field_type;
        }
    }
}


static void exp_is_id(Node exp)
{
    assert(exp->tag == EXP_is_ID);

    Node id = exp->child;
    sym_ent_t *query_result = query(id->val.s, 1);

    if (query_result == NULL) {
        SEMA_ERROR_MSG(1, id->lineno, "Undefined variable \"%s\"", id->val.s);
    }
    else if (query_result->type->class == CMM_TYPE) {
        SEMA_ERROR_MSG(1, id->lineno, "Cannot resovle variable \"%s\"", id->val.s);
    }
    else {
        exp->sema.type = query_result->type;
    }
}


static void exp_is_int(Node exp)
{
    assert(exp->tag == EXP_is_INT);

    exp->sema.type = BASIC_INT;
}


static void exp_is_float(Node exp)
{
    assert(exp->tag == EXP_is_FLOAT);

    exp->sema.type = BASIC_FLOAT;
}


static void stmt_is_return(Node stmt)
{
    assert(stmt->tag == STMT_is_RETURN);

    Node exp = stmt->child;

    sema_visit(exp);

    if (!typecmp(exp->sema.type, stmt->sema.type)) {
        SEMA_ERROR_MSG(8, exp->lineno, "Type mismatched for return.");
    }
}


static void stmt_is_while(Node stmt)
{
    assert(stmt->tag == STMT_is_WHILE);

    Node cond = stmt->child;
    Node loop = cond->sibling;
    
    sema_visit(cond);
    if (!typecmp(cond->sema.type, BASIC_INT)) {
        SEMA_ERROR_MSG(7, cond->lineno, "The condition expression must return int");
    }

    loop->type = stmt->type;  // Check return type in true-branch
    sema_visit(loop);
}


static void stmt_is_if(Node stmt)
{
    assert(stmt->tag == STMT_is_IF);
    Node cond = stmt->child;
    Node behav = cond->sibling;
    
    sema_visit(cond);
    if (!typecmp(cond->sema.type, BASIC_INT)) {
        SEMA_ERROR_MSG(7, stmt->lineno, "The condition expression must return int");
    }

    behav->type = stmt->type;  // Check return type in true-branch
    sema_visit(behav);
}


static void stmt_is_if_else(Node stmt)
{
    assert(stmt->tag == STMT_is_IF_ELSE);
    Node cond = stmt->child;
    Node true_branch = cond->sibling;
    Node false_branch = cond->sibling;
    
    sema_visit(cond);
    if (!typecmp(cond->sema.type, BASIC_INT)) {
        SEMA_ERROR_MSG(7, stmt->lineno, "The condition expression must return int");
    }

    true_branch->sema.type = stmt->sema.type;  // Check return type
    sema_visit(true_branch);

    false_branch->sema.type = stmt->sema.type; // Cehck return type
    sema_visit(false_branch);
}


static void stmt_is_exp(Node stmt)
{
    assert(stmt->tag == STMT_is_EXP);
    sema_visit(stmt->child);
}


static void stmt_is_compst(Node stmt)
{
    assert(stmt->tag == STMT_is_COMPST);

    Node compst = stmt->child;
    compst->sema.type = stmt->sema.type;
    sema_visit(compst);
}


static void compst_is_def_stmt(Node compst)
{
    assert(compst->tag == COMPST_is_DEF_STMT);

    Node stmt = compst->child;
    while (stmt != NULL) {
        sema_visit(stmt);
        stmt = stmt->sibling;
    }
}


static void extdec_is_vardec(Node extdec)
{
    assert(extdec->tag == EXTDEC_is_VARDEC);

    Node vardec = extdec->child;
    while (vardec != NULL) {
        vardec->sema.type = extdec->sema.type;
        sema_visit(vardec);

        if (insert(vardec->sema.name, vardec->sema.type, vardec->sema.lineno, -1) < 0) {
            SEMA_ERROR_MSG(3, vardec->sema.lineno, "Duplicated identifier '%s'", vardec->sema.name);
            // TODO handle memory leak
        }

        vardec = vardec->sibling;
    }
}


static void extdef_is_spec_extdec(Node extdef)
{
    assert(extdef->tag == EXTDEF_is_SPEC_EXTDEC);
    
    Node spec = extdef->child;
    Node extdec = spec->sibling;

    sema_visit(spec);
    extdec->sema.type = spec->sema.type;
    sema_visit(extdec);
}


static void extdef_is_spec(Node extdef)
{
    assert(extdef->tag == EXTDEF_is_SPEC);
    sema_visit(extdef->child);
}


static void extdef_is_spec_func_compst(Node extdef)
{
    assert(extdef->tag == EXTDEF_is_SPEC_FUNC_COMPST);

    Node spec = extdef->child;
    Node func = spec->sibling;
    Node compst = func->sibling;

    sema_visit(spec);
    func->sema.type = spec->sema.type;  // Inherit the type info to register the function symbol
    sema_visit(func);
    compst->sema.type = spec->sema.type; // Inherit the type info to check return type consistentcy
    sema_visit(compst);
}


static void prog_is_extdef(Node prog)
{
    assert(prog->tag == PROG_is_EXTDEF);

    Node extdef = prog->child;
    while (extdef != NULL) {
        sema_visit(extdef);
        extdef = extdef->sibling;
    }
}


static ast_visitor sema_visitors[] = {
    [PROG_is_EXTDEF]             = prog_is_extdef,
    [EXTDEF_is_SPEC_EXTDEC]      = extdef_is_spec_extdec,
    [EXTDEF_is_SPEC]             = extdef_is_spec,
    [EXTDEF_is_SPEC_FUNC_COMPST] = extdef_is_spec_func_compst,
    [EXTDEC_is_VARDEC]           = extdec_is_vardec,
    [SPEC_is_TYPE]               = spec_is_type,
    [SPEC_is_STRUCT]             = spec_is_struct,
    [FUNC_is_ID_VAR]             = func_is_id_var,
    [COMPST_is_DEF_STMT]         = compst_is_def_stmt,
    [STMT_is_COMPST]             = stmt_is_compst,
    [STMT_is_EXP]                = stmt_is_exp,
    [STMT_is_IF]                 = stmt_is_if,
    [STMT_is_IF_ELSE]            = stmt_is_if_else,
    [STMT_is_WHILE]              = stmt_is_while,
    [STMT_is_RETURN]             = stmt_is_return,
    [EXP_is_INT]                 = exp_is_int,
    [EXP_is_FLOAT]               = exp_is_float,
    [EXP_is_ID]                  = exp_is_id,
    [EXP_is_ASSIGN]              = exp_is_assign,
    [EXP_is_EXP_IDX]             = exp_is_exp_idx,
    [EXP_is_ID_ARG]              = exp_is_id_arg,
    [EXP_is_EXP_FIELD]           = exp_is_exp_field,
    [EXP_is_UNARY]               = exp_is_unary,
    [EXP_is_BINARY]              = exp_is_binary,
};


void analyze_program(Node prog)
{
    sema_visit(prog);
}


#include "semantic.h"
#include "cmm-type.h"
#include "cmm-symtab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


typedef void (*ast_visitor)(Node);
static ast_visitor sema_visitors[];  // Predeclaration
#define sema_visit(x) sema_visitors[x->tag](x)


bool semantic_error = false;


#define STRUCT_SCOPE (-10086)


#define SEMA_ERROR_MSG(lineno, fmt, ...) \
    do {\
        semantic_error = true; \
        fprintf(stderr, "Semantic error at line %d: " fmt "\n", lineno, ## __VA_ARGS__);\
    } while(0)


static bool is_in_struct = false;


static void vardec_is_id(Node vardec)
{
    vardec->sema.name = vardec->child->val.s;
    vardec->sema.lineno = vardec->child->lineno;
}


static void vardec_is_vardec_size(Node vardec)
{
    Node sub_vardec = vardec->child;
    Node size = sub_vardec->sibling;

    Type *temp_array = new_type(CMM_ARRAY, NULL, vardec->sema.type, NULL);
    temp_array->size = size->val.i;
    temp_array->type_size = temp_array->size * vardec->sema.type->type_size;
    
    sub_vardec->sema.type = temp_array;
    sema_visit(sub_vardec);

    vardec->sema = sub_vardec->sema;  // Together with name, lineno
}


static void dec_is_vardec(Node dec)
{

    Node vardec = dec->child;
    vardec->sema.type = dec->sema.type;
    sema_visit(vardec);

    if (insert(vardec->sema.name, vardec->sema.type, vardec->sema.lineno, 1) < 0) {
        SEMA_ERROR_MSG(vardec->sema.lineno, "Redefined variable \"%s\".", vardec->sema.name);
        // TODO handle memory leak
    }

    dec->sema = vardec->sema;
}


static void dec_is_vardec_initialization(Node dec)
{
    dec_is_vardec(dec);

    // Initialization
    if (is_in_struct) {
        // Field does not allow assignment
        SEMA_ERROR_MSG(dec->lineno, "Initialization in the structure definition is not allowed");
    }
    else {
        // Assignment consistency check
        Node init = dec->child->sibling;
        sema_visit(init);
        if (!typecmp(init->sema.type, dec->sema.type)) {
            SEMA_ERROR_MSG(init->lineno, "Type mismatch");
        }
    }
}


static void analyze_field_dec(Node dec)
{
    if (dec == NULL) {
        dec->sema.type = NULL;
        return;
    }

    // Analyze the dec first in order to have a right symbol registering sequence.
    sema_visit(dec);

    // Recursively analyze field to insert the node before head in order.
    analyze_field_dec(dec->sibling);

    Type *new_field = new_type(CMM_FIELD, dec->sema.name, dec->sema.type, dec->sibling->sema.type);
    new_field->lineno = dec->sema.lineno;

    dec->sema.type = new_field;
}


static void def_is_spec_dec(Node def)
{
    // Handle Specifier
    Node spec = def->child;
    sema_visit(spec);
    Type *type = spec->sema.type;
    assert(type->type_size != 0);

    // Handle DecList and get a field list if in struct scope
    if (is_in_struct) {
        Node dec = spec->sibling;
        dec->sema.type = type;

        analyze_field_dec(spec->sibling);

        def->sema.type = spec->sibling->sema.type;  // Transfer field link list
    }
    else {
        for (Node dec = spec->sibling; dec != NULL; dec = dec->sibling) {
            dec->sema.type = type;
            sema_visit(dec);
        }
    }
}


static void struct_is_id(Node struc)
{
    Node id = struc->child;
    sym_ent_t *ent = query(id->val.s, STRUCT_SCOPE);
    if (ent == NULL || ent->type->class != CMM_TYPE || ent->type->meta->class != CMM_STRUCT) {
        SEMA_ERROR_MSG(id->lineno, "Undefined struct name '%s'", id->val.s);
    }
    else {
        // ent->type is a meta type
        struc->sema.type = ent->type->meta;
    }
}


static void struct_is_id_def(Node struc)
{
    is_in_struct = true;

    const char *name = (struc->tag == STRUCT_is_DEF) ? "" : struc->child->val.s;
    Node def = (struc->tag == STRUCT_is_DEF) ? struc->child : struc->child->sibling;

    // Construct struct
    Type *this = new_type(CMM_STRUCT, name, NULL, NULL);
    this->lineno = struc->lineno;

    // Get field list
    sema_visit(def);
    this->field = def->sema.type;
    def = def->sibling;
    for (Type *tail = this->field; def != NULL; def = def->sibling) {
        sema_visit(def);
        tail->link = def->sema.type;
        while (tail->link) tail = tail->link;
    }

    int struct_size = 0;

    // Check field name collision and add up the size and offset
    for (Type *outer = this->field; outer != NULL; outer = outer->link) {
        if (outer->name[0] == '\0') {
            // This is an anonymous field, which means that it has been detected as a duplicated field.
            continue;
        }

        if (insert(outer->name, outer->base, outer->lineno, STRUCT_SCOPE) < 0) {
            SEMA_ERROR_MSG(outer->lineno, "Redefined field \"%s\".", outer->name);
        }

        // 记录偏移量并累加大小
        outer->offset = struct_size;
        struct_size += outer->meta->type_size;

        Type *inner = outer->link;
        while (inner != NULL) {
            if (!strcmp(outer->name, inner->name)) {
                SEMA_ERROR_MSG(inner->lineno,
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

    if (struc->tag == STRUCT_is_ID_DEF) {
        Type *meta = new_type(CMM_TYPE, this->name, this, NULL);
        meta->lineno = struc->lineno;
        if (insert(meta->name, meta, struc->lineno, 0) < 1) {
            SEMA_ERROR_MSG(struc->lineno, "Duplicated name \"%s\".", name);
        }
    }

    struc->sema.type = this;

    is_in_struct = false;
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
    sema_visit(spec->child);
    spec->sema = spec->child->sema;
}


static void var_is_spec_vardec(Node paramdec)
{
    Node spec = paramdec->child;
    Node vardec = spec->sibling;

    sema_visit(spec);

    Type *type = spec->sema.type;
    vardec->sema.type = type;

    sema_visit(vardec);

    int insert_ret = insert(vardec->sema.name, vardec->sema.type, paramdec->lineno, -1);
    if (insert_ret < 1) {
        SEMA_ERROR_MSG(vardec->lineno, "Duplicated variable definition of '%s'", vardec->sema.name);
    }

    paramdec->sema = vardec->sema;
}

// Then we should link the paramdec's type up to form a param type list.
// varlist should return a type of CmmParam, and the generation of CmmParam occurs here.
static Type *get_params(Node var)
{
    if (var == NULL) {
        return NULL;
    }

    sema_visit(var);

    Type *sub_list = get_params(var->sibling);

    Type *param = new_type(CMM_PARAM, var->sema.name, var->sema.type, sub_list);

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
    Type *param_list = get_params(var);

    // Generate function symbol
    Type *func = new_type(CMM_FUNC, name, fundec->sema.type, param_list);

    if (insert(func->name, func, fundec->lineno, -1) < 0) {
        SEMA_ERROR_MSG(fundec->lineno, "Redefined function \"%s\"", func->name);
        // TODO handle memory leak!
    }
}


//
// exp only return its type.
// We use stmt analyzer to check the return type.
//
static inline int is_lval(const Node exp)
{
    if (exp->tag == EXP_is_ID) {
        // Avoid function name and type name.
        // An array directly found in the symbol table is a constant variable
        // which cannot be assigned.
        sym_ent_t *ent = query(exp->child->val.s, 0);
        // TODO Ugly conditions
        return ent != NULL && ent->type->class != CMM_FUNC && ent->type->class != CMM_TYPE && ent->type->class != CMM_ARRAY;
    }
    else {
        return exp->tag == EXP_is_EXP_IDX || exp->tag == EXP_is_EXP_FIELD;
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
        SEMA_ERROR_MSG(exp->lineno, "\"!\" cannot cast on float");
    }
    else {
        SEMA_ERROR_MSG(exp->lineno, "\"%s\" cannot case on unbasic type", exp->val.operator);
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
        SEMA_ERROR_MSG(exp->lineno, "Type mismatched for operands");
    }
    else if (!typecmp(ltype, BASIC_INT) && !typecmp(ltype, BASIC_FLOAT)) {
        // Type matched, but cannot be operated
        SEMA_ERROR_MSG(exp->lineno, "The type is not allowed in operation '%s'", exp->val.operator);
    }
    else {
        exp->sema.type = ltype;
    }
}


static void exp_is_assign(Node exp)
{
    Node lexp = exp->child;
    Node rexp = lexp->sibling;

    sema_visit(lexp);
    sema_visit(rexp);

    if (!typecmp(lexp->sema.type, rexp->sema.type)) {
        SEMA_ERROR_MSG(exp->lineno, "Type mismatched for assignment.");
    }
    else if (!is_lval(lexp)) {
        SEMA_ERROR_MSG(exp->lineno, "The left-hand side of an assignment must be a variable.");
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
        SEMA_ERROR_MSG(rexp->lineno, "expression is not a integer");
    }

    // If lexp_type is null, it means that an semantic error has occurred, then we can ignore the
    // consecutive errors.
    if (exp->sema.type != NULL) {
        if (exp->sema.type->class != CMM_ARRAY) {
            SEMA_ERROR_MSG(lexp->lineno, "expression is not an array.");
        }
        else {
            exp->sema.type = lexp->sema.type->base;
        }
    }
}


static void check_param_list(Type *param, Node arg, int lineno)
{
    while (param != NULL && arg != NULL) {
        sema_visit(arg);
        Type *param_type = arg->sema.type;

        if (!typecmp(param_type, param->base)) {
            SEMA_ERROR_MSG(arg->lineno, "parameter type mismatches");
        }

        param = param->link;
        arg = arg->sibling;
    }

    if (!(param == NULL && arg == NULL)) {
        SEMA_ERROR_MSG(lineno, "parameter number mismatches");
    }
}


static void exp_is_id_arg(Node exp)
{
    Node id = exp->child;

    sym_ent_t *query_result = query(id->val.s, 1);

    if (query_result == NULL) {
        SEMA_ERROR_MSG(id->lineno, "Undefined function \"%s\".", id->val.s);
    }
    else if (query_result->type->class != CMM_FUNC) {
        SEMA_ERROR_MSG(id->lineno, "\"%s\" is not a function.", id->val.s);
    }
    else {
        // Error report in the check
        check_param_list(query_result->type->param, id->sibling, exp->lineno);
        // Return the return type while ignoring errors in arguments
        exp->sema.type = query_result->type->ret;
    }
}


static void exp_is_exp_field(Node exp)
{
    Node struc = exp->child;

    sema_visit(struc);

    if (struc->sema.type->class != CMM_STRUCT) {
        SEMA_ERROR_MSG(exp->lineno, "The left identifier of '.' is not a struct");
    }
    else {
        Node field = struc->sibling;
        Type *field_type = query_field(struc->sema.type, field->val.s);
        if (field_type == NULL) {
            SEMA_ERROR_MSG(field->lineno, "Undefined field \"%s\" in struct \"%s\".",
                    field->val.s, struc->sema.type->name);
        }
        else {
            exp->sema.type = field_type;
        }
    }
}


static void exp_is_id(Node exp)
{
    Node id = exp->child;
    sym_ent_t *query_result = query(id->val.s, 1);

    if (query_result == NULL) {
        SEMA_ERROR_MSG(id->lineno, "Undefined variable \"%s\"", id->val.s);
    }
    else if (query_result->type->class == CMM_TYPE) {
        SEMA_ERROR_MSG(id->lineno, "Cannot resovle variable \"%s\"", id->val.s);
    }
    else {
        exp->sema.type = query_result->type;
    }
}


static void exp_is_int(Node exp)
{
    exp->sema.type = BASIC_INT;
}


static void exp_is_float(Node exp)
{
    exp->sema.type = BASIC_FLOAT;
}


static void stmt_is_return(Node stmt)
{
    Node exp = stmt->child;

    sema_visit(exp);

    if (!typecmp(exp->sema.type, stmt->sema.type)) {
        SEMA_ERROR_MSG(exp->lineno, "Type mismatched for return.");
    }
}


static void stmt_is_while(Node stmt)
{
    Node cond = stmt->child;
    Node loop = cond->sibling;
    
    sema_visit(cond);
    if (!typecmp(cond->sema.type, BASIC_INT)) {
        SEMA_ERROR_MSG(cond->lineno, "The condition expression must return int");
    }

    loop->sema.type = stmt->sema.type;  // Check return type in true-branch
    sema_visit(loop);
}


static void stmt_is_if(Node stmt)
{
    Node cond = stmt->child;
    Node behav = cond->sibling;
    
    sema_visit(cond);
    if (!typecmp(cond->sema.type, BASIC_INT)) {
        SEMA_ERROR_MSG(stmt->lineno, "The condition expression must return int");
    }

    behav->sema.type = stmt->sema.type;  // Check return type in true-branch
    sema_visit(behav);
}


static void stmt_is_if_else(Node stmt)
{
    Node cond = stmt->child;
    Node true_branch = cond->sibling;
    Node false_branch = cond->sibling;
    
    sema_visit(cond);
    if (!typecmp(cond->sema.type, BASIC_INT)) {
        SEMA_ERROR_MSG(stmt->lineno, "The condition expression must return int");
    }

    true_branch->sema.type = stmt->sema.type;  // Check return type
    sema_visit(true_branch);

    false_branch->sema.type = stmt->sema.type; // Cehck return type
    sema_visit(false_branch);
}


static void stmt_is_exp(Node stmt)
{
    sema_visit(stmt->child);
}


static void stmt_is_compst(Node stmt)
{
    Node compst = stmt->child;
    compst->sema.type = stmt->sema.type;
    sema_visit(compst);
}


static void compst_is_def_stmt(Node compst)
{
    Node stmt = compst->child;
    while (stmt != NULL) {
        sema_visit(stmt);
        stmt = stmt->sibling;
    }
}


static void extdec_is_vardec(Node extdec)
{
    Node vardec = extdec->child;
    while (vardec != NULL) {
        vardec->sema.type = extdec->sema.type;
        sema_visit(vardec);

        if (insert(vardec->sema.name, vardec->sema.type, vardec->sema.lineno, -1) < 0) {
            SEMA_ERROR_MSG(vardec->sema.lineno, "Duplicated identifier '%s'", vardec->sema.name);
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
    [STRUCT_is_ID]               = struct_is_id,
    [STRUCT_is_DEF]              = struct_is_id_def,  // Share most part
    [STRUCT_is_ID_DEF]           = struct_is_id_def,
    [FUNC_is_ID_VAR]             = func_is_id_var,
    [VARDEC_is_ID]               = vardec_is_id,
    [VARDEC_is_VARDEC_SIZE]      = vardec_is_vardec_size,
    [VAR_is_SPEC_VARDEC]         = var_is_spec_vardec,
    [COMPST_is_DEF_STMT]         = compst_is_def_stmt,
    [DEF_is_SPEC_DEC]            = def_is_spec_dec,
    [DEC_is_VARDEC]              = dec_is_vardec,
    [DEC_is_VARDEC_INITIALIZATION] = dec_is_vardec_initialization,
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
    [EXP_is_RELOP]               = exp_is_binary,
};


void analyze_program(Node prog)
{
    sema_visit(prog);
}


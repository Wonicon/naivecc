#ifndef CMM_TYPE_H
#define CMM_TYPE_H

#include "lib.h"
#include <stdio.h>

enum CMM_TYPE {
    CMM_TYPE_int,
    CMM_TYPE_float,
    CMM_TYPE_array,
    CMM_TYPE_field,
    CMM_TYPE_struct,
    CMM_TYPE_param,
    CMM_TYPE_func
};

/*
 * The common part of the type structs,
 * used as a generic type to store multiple types.
 */
typedef struct __TypeHead__ {
    struct __TypeHead__ *type;
    char *name;
    enum CMM_TYPE type_code;
} TypeHead;

/*
 * This anonymous struct declaration withous
 * semi comma is only use to allow direct access
 * to the field of the common parts of all types
 */
#define TypeHeader struct {       \
    TypeHead *type;               \
    char *name;                   \
    enum CMM_TYPE type_code;      \
}

/* The fixed size array type */
typedef struct __CMM_ARRAY__ {
    TypeHeader;
    int size; /* The number of elements in the array */
} CMM_array;

/* The commonly repeated variable type */
typedef struct __CMM_VAR_LIST_NODE__ {
    TypeHeader;
    struct __CMM_VAR_LIST_NODE__ *next;
} CMM_var_list_node;

typedef CMM_var_list_node CMM_field;
typedef CMM_var_list_node CMM_param;

/* The struct field */
typedef struct __CMM_STRUCT__ {
    TypeHeader;
    CMM_field *field_list;
} CMM_struct;

/* The function type */
typedef struct __CMM_FUNC__ {
    TypeHeader; /* For return value */
    CMM_param *param_list;
} CMM_func;

#define ctor_helper(type) concat(CMM_, type) *concat(new_, type)(TypeHead *base, char *name);
#include "cmm_type.template"
#undef ctor_helper

#endif /* CMM_TYPE_H */

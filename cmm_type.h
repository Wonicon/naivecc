#ifndef CMM_TYPE_H
#define CMM_TYPE_H

#include "lib.h"
#include <stdio.h>

typedef enum __CMM_TYPE__ {
    CMM_TYPE_INT,
    CMM_TYPE_FLOAT,
    CMM_TYPE_ARRAY,
    CMM_TYPE_STRUCT,
    CMM_TYPE_FUNC
} CmmType;

/*
 * A CmmType * field is the common part of the Type structures,
 * which is used as a generic type pointer to store multiple types.
 */

typedef CmmType CmmInt;

typedef CmmType CmmFloat;

/* The fixed size array type */
typedef struct __CMM_ARRAY__ {
    CmmType type;  /* The type indicator to recognize itself */
    const CmmType *base; /* The array type is an array of another base */
    int size;      /* The number of elements in the array */
} CmmArray;

/*
 * The field and param cannot be considered as an individual type, which
 * is a part of the structure or function. So the meaning of fields in this
 * struct type may be somewhat different.
 * The type field is a pointer but not a value because the node has no needs
 * to recognized itself, it links to the real type this field owns.
 */
typedef struct __TYPE_FIELD__ {
    const CmmType *type;
    const char *name;
    const struct __TYPE_FIELD__ *next;
} CmmField;

// The param list don't have name attribute.
// But name is one of the fundamental attribute in structure.
typedef struct __TYPE_PARAM__ {
    const CmmType *type;
    const struct __TYPE_PARAM__ *next;
} CmmParam;

/* The struct field */
typedef struct __CMM_STRUCT__ {
    CmmType type;
    const char *tag;            /* Optional, can be NULL */
    const CmmField *field_list;
} CmmStruct;

/* The function type */
typedef struct __CMM_FUNC__ {
    CmmType type;
    const CmmType *ret;          /* The type of return value */
    const char *name;            /* The function name, must have */
    const CmmParam *param_list;
} CmmFunc;


extern const CmmType *global_int;
extern const CmmType *global_float;


CmmArray *new_type_array(int size, const CmmType *base);
CmmStruct *new_type_struct(const char *name);
CmmField *new_type_field(const CmmType *type, const char *name, const CmmField *next);
CmmParam *new_type_param(const CmmType *type, const CmmParam *next);
CmmFunc *new_type_func(const char *name, const CmmType *ret);
const CmmType *query_field(const CmmType *target_struct, const char *field_name);
const char *typename(const CmmType *x);
/* Compare two cmm type, return 1 if they are the same, 0 otherwise */
int typecmp(const CmmType *x, const CmmType *y);
void print_type(const CmmType *x);


#define GENERIC(x) ((CmmType *)x)
#define ARRAY(x) ((CmmArray *)(x))
#define STRUCT(x) ((CmmStruct *)(x))
#define FUNC(x) ((CmmFunc *)(x))


#endif /* CMM_TYPE_H */

#ifndef CMM_TYPE_H
#define CMM_TYPE_H

#include "lib.h"
#include <stdio.h>

typedef enum __CMM_TYPE__ {
    CMM_TYPE_INT,
    CMM_TYPE_FLOAT,
    CMM_TYPE_ARRAY,
    CMM_TYPE_STRUCT,
    CMM_TYPE_FUNC,
    CMM_TYPE_FIELD,
    CMM_TYPE_PARAM
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
    CmmType *base; /* The array type is an array of another base */
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
    CmmType type;
    CmmType *base;
    const char *name;
    int def_line_no;
    struct __TYPE_FIELD__ *next;
} CmmField;

// The param list don't have name attribute.
// But name is one of the fundamental attribute in structure.
typedef struct __TYPE_PARAM__ {
    CmmType type;
    CmmType *base;
    struct __TYPE_PARAM__ *next;
} CmmParam;

/* The struct field */
typedef struct __CMM_STRUCT__ {
    CmmType type;
    const char *tag;            /* Optional, can be NULL */
    CmmField *field_list;
} CmmStruct;

/* The function type */
typedef struct __CMM_FUNC__ {
    CmmType type;
    CmmType *ret;          /* The type of return value */
    const char *name;            /* The function name, must have */
    CmmParam *param_list;
} CmmFunc;


extern CmmType *global_int;
extern CmmType *global_float;


CmmArray *new_type_array(int size, CmmType *base);
CmmStruct *new_type_struct(const char *name);
CmmField *new_type_field(CmmType *type, const char *name, int lineno, CmmField *next);
CmmParam *new_type_param(CmmType *type, CmmParam *next);
CmmFunc *new_type_func(const char *name, CmmType *ret);
CmmType *query_field(CmmType *target_struct, const char *field_name);
const char *typename(CmmType *x);
/* Compare two cmm type, return 1 if they are the same, 0 otherwise */
int typecmp(CmmType *x, CmmType *y);
void print_type(CmmType *x);

#define GENERIC(x) ((CmmType *)x)
#define SAFE
#ifndef SAFE
#define ARRAY(x) ((CmmArray *)(x))
#define STRUCT(x) ((CmmStruct *)(x))
#define FUNC(x) ((CmmFunc *)(x))
#define FIELD(x) ((CmmField *)(x))
#define PARAM(x) ((CmmParam *)(x))
#else
#include <assert.h>
static inline CmmArray *Array(CmmType *type) { assert(type == NULL || *type == CMM_TYPE_ARRAY); return (CmmArray *)type; }
static inline CmmStruct *Struct(CmmType *type) { assert(type == NULL || *type == CMM_TYPE_STRUCT); return (CmmStruct *)type; }
static inline CmmFunc *Fun(CmmType *type) { assert(type == NULL || *type == CMM_TYPE_FUNC); return (CmmFunc *)type; }
static inline CmmField *Field(CmmType *type) { assert(type == NULL || *type == CMM_TYPE_FIELD); return (CmmField *)type; }
static inline CmmParam *Param(CmmType *type) { assert(type == NULL || *type == CMM_TYPE_PARAM); return (CmmParam *)type; }
#endif

#endif /* CMM_TYPE_H */

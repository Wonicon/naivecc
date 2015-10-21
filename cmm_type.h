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
typedef struct __TYPE_NODE__ {
    CmmType *type;
    struct __TYPE_NODE__ *next;
} TypeNode;

/* The struct field */
typedef struct __CMM_STRUCT__ {
    CmmType type;
    char *tag;            /* Optional, can be NULL */
    TypeNode *field_list;
} CmmStruct;

/* The function type */
typedef struct __CMM_FUNC__ {
    CmmType type;
    CmmType *ret;          /* The type of return value */
    char *name;            /* The function name, must have */
    TypeNode *param_list;
} CmmFunc;

/* Compare two cmm type, return 1 if they are the same, 0 otherwise */
int typecmp(CmmType *x, CmmType *y);
void print_type(CmmType *x);

extern CmmType *global_int;
extern CmmType *global_float;

CmmArray *new_type_array(int size, CmmType *base);
CmmStruct *new_type_struct(char *name);
TypeNode *new_type_node(CmmType *type, TypeNode *next);
CmmFunc *new_type_func(char *name, CmmType *ret);

#define GENERIC(x) ((CmmType *)x)
#define ARRAY(x) ((CmmArray *)(x))
#define STRUCT(x) ((CmmStruct *)(x))
#define FUNC(x) ((CmmFunc *)(x))

#endif /* CMM_TYPE_H */

#ifndef CMM_TYPE_H
#define CMM_TYPE_H

#include "lib.h"
#include <stdio.h>

struct _Symbol;

typedef struct _Type {
    CmmType class;
    const char *name;
    union {
        struct _Type *base;    // For array, field, param,
        struct _Type *meta;    // For metatype to record struct
        struct _Type *ret;     // For function
    };
    union {
        struct _Type *link;    // For field and param
        struct _Type *field;   // For struct
        struct _Type *param;   // For function
        struct _Symbol **field_table;
    };
    union {
        int ref;               // How many instance are using this type, used by struct
        int size;              // Every variable have its own array type
    };
    union {
        int type_size;
        int offset;            // Used by field, field use meta->type_size as its size
    };
    int lineno;                // The line number of definition
} Type;


extern Type *BASIC_INT;
extern Type *BASIC_FLOAT;

Type *new_type(CmmType class, const char *name, Type *type, Type *link);
bool typecmp(const Type *x, const Type *y);
void print_type(const Type *type);

#endif // CMM_TYPE_H

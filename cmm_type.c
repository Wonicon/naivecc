#include "cmm_type.h"
#include <stdlib.h> /* malloc */
#include <string.h> /* strlen */
#include <assert.h> /* assert */

const char *type_s[] =
{
    "int",
    "float",
    "array",
    "struct",
    "function"
};
/* Constructors */

CmmArray *new_type_array(int size, CmmType *base)
{
    /* Todo: avoid the cross reference! */
    CmmArray *p = (CmmArray *)malloc(sizeof(CmmArray));
    p->type = CMM_TYPE_ARRAY;
    p->size = size;
    p->base = base;
    return p;
}

CmmStruct *new_type_struct(char *name)
{
    CmmStruct *p = (CmmStruct *) malloc(sizeof(CmmStruct));
    p->type = CMM_TYPE_STRUCT;
    p->tag = name;
    p->field_list = NULL;
    return p;
}

CmmFunc *new_type_func(char *name, CmmType *ret)
{
    CmmFunc *p = (CmmFunc *)malloc(sizeof(CmmFunc));
    p->type = CMM_TYPE_FUNC;
    p->name = name;
    p->ret = ret;
    p->param_list = NULL;
    return p;
}

TypeNode *new_type_node(CmmType *type, TypeNode *next)
{
    TypeNode *p = (TypeNode *)malloc(sizeof(TypeNode));
    p->type = type;
    p->next = next;
    return p;
}

const char *typename(CmmType *x)
{
    return type_s[*x];
}

/* constructors end */

#define ARRAY(x) ((CmmArray *)(x))
#define STRUCT(x) ((CmmStruct *)(x))
#define FUNC(x) ((CmmFunc *)(x))
#define GENERIC(x) ((CmmType *)(x))

int typecmp(CmmType *x, CmmType *y)
{
    LOG("Now compare %s and %s", typename(x), typename(y));
    /* Totally different! */
    if (*x != *y) {
        LOG("Totally different!");
        return 0;
    }

    /* The function only use name to differ */
    if (*x == CMM_TYPE_FUNC) {
        LOG("Then we fall into func comparision");
        CmmFunc *func_x, *func_y;
        func_x = FUNC(x);
        func_y = FUNC(y);
        if (strcmp(func_x->name, func_y->name) != 0) {
            return 0;
        } else {
            TypeNode *param_x = func_x->param_list;
            TypeNode *param_y = func_y->param_list;
            while (param_x != NULL && param_y != NULL && typecmp(param_x->type, param_y->type)) {
                param_x = param_x->next;
                param_y = param_y->next;
            }

            /* Compare to the end means all the same */
            if (param_x != NULL || param_y != NULL) {
                return 0;
            }

            /* Compare return type */
            return typecmp(FUNC(x)->ret, FUNC(y)->ret);
        }
    }

    if (*x == CMM_TYPE_ARRAY) {
        LOG("Then we fall into array comparision");
        /* The array differs from its dimension and basic type */
        int loopno = 1;
        /* End loop when reach the basic type */
        while (*x == CMM_TYPE_ARRAY && *y == CMM_TYPE_ARRAY) {
            LOG("loop %d", loopno);
            if (*x != *y) {
                return 0;
            }
            x = ARRAY(x)->base;
            y = ARRAY(y)->base;
            loopno++;
        }

        if (*x == CMM_TYPE_ARRAY || *y == CMM_TYPE_ARRAY) {
            /* The dimension differs */
            return 0;
        }
        else {
            /* Compare the base type */
            return typecmp(x, y);
        }
    }

    if (*x == CMM_TYPE_STRUCT) {
        LOG("Then we fall into struct comparision");
#ifdef STRUCTURE
        TypeNode *field_x = STRUCT(x)->field_list;
        TypeNode *field_y = STRUCT(y)->field_list;
        /* Compare the field one by one */
        while (field_x != NULL && field_y != NULL && typecmp(field_x->type, field_y->type)) {
            field_x = field_x->next;
            field_y = field_y->next;
        }

        if (field_x == NULL && field_y == NULL) {
            return 1;  /* The fields' numbers are the same, and the type is the same respectively */
        } else {
            return 0;
        }
#else /* just the name */
        return strcmp(STRUCT(x)->tag, STRUCT(y)->name) == 0;
#endif /* ifdef STRUCTURE */
    }

    assert(*x == CMM_TYPE_INT || *x == CMM_TYPE_FLOAT);

    return 1;
} /* typecmp */

void _print_type(CmmType *generic, char *end)
{
    CmmType *base;
    TypeNode *nd = NULL;
    switch (*generic) {
        case CMM_TYPE_INT:
            printf("int");
            break;
        case CMM_TYPE_FLOAT:
            printf("float");
            break;
        case CMM_TYPE_ARRAY:
            /* Find the base */
            base = ARRAY(generic)->base;
            while (*base == CMM_TYPE_ARRAY) {
                base = ARRAY(base)->base;
            }
            /* Print the base recursively */
            _print_type(base, " ");
            /* Print dimension from left to right */
            while (*generic == CMM_TYPE_ARRAY) {
                printf("[%d]", ARRAY(generic)->size);
                generic = ARRAY(generic)->base;
            }
            break;
        case CMM_TYPE_STRUCT:
            printf("struct %s { ", STRUCT(generic)->tag);
            /* Print field list */
            nd = STRUCT(generic)->field_list;
            while (nd != NULL) {
                _print_type(nd->type, "; ");
                nd = nd->next;
            }
            printf("}");
            break;
        case CMM_TYPE_FUNC:
            /* Print return type */
            _print_type(FUNC(generic)->ret, " ");
            /* Print function name */
            printf("%s (", FUNC(generic)->name);
            /* Print param list */
            nd = FUNC(generic)->param_list;
            while (nd != NULL && nd->next != NULL) {
                _print_type(nd->type, ", ");
                nd = nd->next;
            }
            /* Tail param handling */
            if (nd != NULL) {
                _print_type(nd->type, "");
            }
            printf(")");
            break;
        default:
            assert(0);
    }

    printf("%s", end);
}

void print_type(CmmType *x)
{
    _print_type(x, "\n");
}

/* Test */
#ifdef TEST_TYPE
int main()
#else  /* ifndef TEST_TYPE */
int test_cmm_type()
#endif /* endif  TEST_TYPE */
{
    CmmType top = CMM_TYPE_INT;
    CmmType *current = &top;
    CmmType *types[100];
    for (int i = 0; i < 100; i++) {
        int rd = rand();
        int idx = (unsigned)rd % 3;

        switch (idx) {
            case 0:
                current = (CmmType *)new_type_array(rd, current);
                break;
            case 1:
                current = (CmmType *)new_type_struct("hello");
                STRUCT(current)->field_list = new_type_node(&top, NULL);
                STRUCT(current)->field_list->next = new_type_node(&top, NULL);
                break;
            case 2:
                current = (CmmType *)new_type_func("foo", current);
                FUNC(current)->param_list = new_type_node(&top, NULL);
                FUNC(current)->param_list->next = new_type_node(&top, NULL);
                break;
            default:
                assert(0);
        }

        types[i] = current;
    }

    for (int k = 0; k < 100; k++) {
        assert(typecmp(types[k], types[k]));
    }

    for (int k = 0; k < 100; k++) {
        print_type(types[k]);
    }

    return 0;
}

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

CmmType basic_type[2] = { CMM_TYPE_INT, CMM_TYPE_FLOAT };

const CmmType *global_int = &basic_type[0];
const CmmType *global_float = &basic_type[1];

CmmArray *new_type_array(int size, const CmmType *base)
{
    CmmArray *p = (CmmArray *)malloc(sizeof(CmmArray));
    p->type = CMM_TYPE_ARRAY;
    p->size = size;
    p->base = base;
    return p;
}

CmmStruct *new_type_struct(const char *name)
{
    CmmStruct *p = (CmmStruct *) malloc(sizeof(CmmStruct));
    p->type = CMM_TYPE_STRUCT;
    p->tag = name;
    p->field_list = NULL;
    return p;
}

CmmFunc *new_type_func(const char *name, const CmmType *ret)
{
    CmmFunc *p = (CmmFunc *)malloc(sizeof(CmmFunc));
    p->type = CMM_TYPE_FUNC;
    p->name = name;
    p->ret = ret;
    p->param_list = NULL;
    return p;
}

CmmField *new_type_field(const CmmType *type, const char *name, int lineno, const CmmField *next) {
    CmmField *p = NEW(CmmField);
    p->type = CMM_TYPE_FIELD;
    p->base = type;
    p->next = next;
    p->name = name;
    p->def_line_no = lineno;
    return p;
}

CmmParam *new_type_param(const CmmType *type, const CmmParam *next)
{
    CmmParam *p = NEW(CmmParam);
    p->type = CMM_TYPE_PARAM;
    p->base = type;
    p->next = next;
    return p;
}

const char *typename(const CmmType *x) {
    return type_s[*x];
}

/* constructors end */

int typecmp(const CmmType *x, const CmmType *y) {
    if (x == y) {  // Allow void
        return 1;
    } else if (x == NULL || y == NULL) {  // void vs others
        return 0;
    }
    LOG("Now compare %s and %s", typename(x), typename(y));
    /* Totally different! */
    if (*x != *y) {
        LOG("Totally different!");
        return 0;
    }

    /* The function only use name to differ */
    if (*x == CMM_TYPE_FUNC) {
        LOG("Then we fall into func comparision");
        const CmmFunc *func_x, *func_y;
        func_x = Fun(x);
        func_y = Fun(y);
        if (strcmp(func_x->name, func_y->name) != 0) {
            return 0;
        } else {
            const CmmParam *param_x = func_x->param_list;
            const CmmParam *param_y = func_y->param_list;
            while (param_x != NULL && param_y != NULL && typecmp(param_x->base, param_y->base)) {
                param_x = param_x->next;
                param_y = param_y->next;
            }

            /* Compare to the end means all the same */
            if (param_x != NULL || param_y != NULL) {
                return 0;
            }

            /* Compare return type */
            return typecmp(Fun(x)->ret, Fun(y)->ret);
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
            x = Array(x)->base;
            y = Array(y)->base;
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
        // Comparison based on structure similarity
        // But don't compare their name
        const CmmField *field_x = Struct(x)->field_list;
        const CmmField *field_y = Struct(y)->field_list;
        /* Compare the field one by one */
        while (field_x != NULL && field_y != NULL && typecmp(field_x->base, field_y->base)) {
            field_x = field_x->next;
            field_y = field_y->next;
        }

        if (field_x == NULL && field_y == NULL) {
            return 1;  /* The fields' numbers are the same, and the type is the same respectively */
        } else {
            return 0;
        }
#else /* just the name */
        return strcmp(STRUCT(x)->tag, STRUCT(y)->tag) == 0;
#endif /* ifdef STRUCTURE */
    }

    assert(*x == CMM_TYPE_INT || *x == CMM_TYPE_FLOAT);

    return 1;
} /* typecmp */

// Find out whether a field belongs to a given structure
// return the type of that field, NULL if not found
const CmmType *query_field(const CmmType *target_struct, const char *field_name) {
    assert(*target_struct == CMM_TYPE_STRUCT);
    assert(field_name != NULL);

    // Traverse the field list
    for (const CmmField *target_field = Struct(target_struct)->field_list;
         target_field != NULL; target_field = target_field->next) {
        if (!strcmp(target_field->name, field_name)) {
            return target_field->base;
        }
    }

    // Not found
    return NULL;
}

void _print_type(const CmmType *generic, const char *end)
{
    const CmmType *base;
    const CmmParam *param = NULL;
    const CmmField *field = NULL;
    switch (*generic) {
        case CMM_TYPE_INT:
            printf("int");
            break;
        case CMM_TYPE_FLOAT:
            printf("float");
            break;
        case CMM_TYPE_ARRAY:
            /* Find the base */
            base = Array(generic)->base;
            while (*base == CMM_TYPE_ARRAY) {
                base = Array(base)->base;
            }
            /* Print the base recursively */
            _print_type(base, " ");
            /* Print dimension from left to right */
            while (*generic == CMM_TYPE_ARRAY) {
                printf("[%d]", Array(generic)->size);
                generic = Array(generic)->base;
            }
            break;
        case CMM_TYPE_STRUCT:
            printf("struct %s { ", Struct(generic)->tag);
            /* Print field list */
            field = Struct(generic)->field_list;
            while (field != NULL) {
                _print_type(field->base, " ");
                printf("%s; ", field->name);
                field = field->next;
            }
            printf("}");
            break;
        case CMM_TYPE_FUNC:
            /* Print return type */
            _print_type(Fun(generic)->ret, " ");
            /* Print function name */
            printf("%s (", Fun(generic)->name);
            /* Print param list */
            param = Fun(generic)->param_list;
            while (param != NULL && param->next != NULL) {
                _print_type(param->base, ", ");
                param = param->next;
            }
            /* Tail param handling */
            if (param != NULL) {
                _print_type(param->base, "");
            }
            printf(")");
            break;
        default:
            assert(0);
    }

    printf("%s", end);
}

void print_type(const CmmType *x) {
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
    CmmStruct *struc;
    CmmFunc *foo;
    for (int i = 0; i < 100; i++) {
        int rd = rand();
        int idx = (unsigned)rd % 3;

        switch (idx) {
            case 0:
                current = (CmmType *)new_type_array(rd, current);
                break;
            case 1:
                struc = new_type_struct("hello");
                struc->field_list = new_type_field(&top, "a", 0, NULL);
                struc->field_list = new_type_field(&top, "b", 1, Struct(current)->field_list);
                current = GENERIC(struc);
                break;
            case 2:
                foo = new_type_func("foo", current);
                foo->param_list = new_type_param(&top, NULL);
                foo->param_list = new_type_param(&top, Fun(current)->param_list);
                current = GENERIC(foo);
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

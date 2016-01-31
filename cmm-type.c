#include "cmm-type.h"
#include <stdlib.h> /* malloc */
#include <string.h> /* strlen */
#include <assert.h> /* assert */

const char *class_s[] = {
    "int",
    "float",
    "array",
    "struct",
    "function",
    "field",
    "param",
    "type"
};

Type _BASIC_INT = {
    CMM_INT,
    "int",
    { NULL },
    { NULL },
    { 0 },
    { 4 },
    0
};

Type _BASIC_FLOAT = {
    CMM_FLOAT,
    "float",
    { NULL },
    { NULL },
    { 0 },
    { 4 },
    0
};

Type *BASIC_INT = &_BASIC_INT;
Type *BASIC_FLOAT = &_BASIC_FLOAT;

// Constructor
Type *new_type(CmmType class, const char *name, Type *type, Type *link)
{
    Type *this = NEW(Type);
    this->class = class;
    this->name = name;
    this->base = type;  // Equivalent to this->meta, this->ret
    this->link = link;
    this->ref = 0;
    this->lineno = -1;
    return this;
}

bool typecmp(const Type *x, const Type *y)
{
    // TODO allow void ?
    if (x == y) {  // Allow void
        return true;
    }
    else if (x == NULL || y == NULL) {  // void vs others
        return true;  // This is error, but NULL is used for undefined variables, return true allow not reporting error recursively.
    }

    LOG("Now compare %s and %s", x->name, y->name);
    /* Totally different! */
    if (x->class != y->class) {
        LOG("%s and %s are totally different!", class_s[x->class], class_s[y->class]);
        return false;
    }

    // The function only use name to differ
    if (x->class == CMM_FUNC) {
        LOG("Then we fall into func comparision");
        if (strcmp(x->name, y->name) != 0) {
            return false;
        }
        else {
            const Type *param_x = x->param;
            const Type *param_y = y->param;
            while (param_x != NULL && param_y != NULL && typecmp(param_x->base, param_y->base)) {
                param_x = param_x->link;
                param_y = param_y->link;
            }

            // Compare to the end means all the same
            if (param_x != NULL || param_y != NULL) {
                return false;
            }
            else {
                // Compare return type
                // TODO: we don't allow void type now
                assert(x->ret != NULL && y->ret != NULL);
                return typecmp(x->ret, y->ret);
            }
        }
    }

    if (x->class == CMM_ARRAY) {
        LOG("Then we fall into array comparision");
        // The array differs from its dimension and basic type
        int loopno = 1;
        // End loop when reach the basic type
        while (x->class == CMM_ARRAY && y->class == CMM_ARRAY) {
            LOG("loop %d", loopno);
            x = x->base;
            y = y->base;
            loopno++;
        }

        if (x->class == CMM_ARRAY || y->class == CMM_ARRAY) {
            // The dimension differs
            return false;
        }
        else {
            // Compare the base type
            return typecmp(x, y);
        }
    }

    if (x->class == CMM_STRUCT) {
        LOG("Then we fall into struct comparision");
#ifdef STRUCTURE
        // Comparison based on structure similarity
        // But don't compare their name
        const Type *field_x = x->field;
        const Type *field_y = y->field;
        // Compare the field one by one
        while (field_x != NULL && field_y != NULL && typecmp(field_x->base, field_y->base)) {
            field_x = field_x->link;
            field_y = field_y->link;
        }

        if (field_x == NULL && field_y == NULL) {
            return true;  /* The fields' numbers are the same, and the type is the same respectively */
        }
        else {
            return false;
        }
#else  // just the name
        return strcmp(STRUCT(x)->tag, STRUCT(y)->tag) == 0;
#endif // ifdef STRUCTURE
    }

    assert(x->class == CMM_INT || x->class == CMM_FLOAT);

    return 1;
}

void _print_type(const Type *type, char *end)
{
    const Type *base, *link;
    if (type == NULL) {
        printf("void%s", end);
        return;
    }
    switch (type->class) {
        case CMM_INT:
        case CMM_FLOAT:
            printf("%s", class_s[type->class]);
            break;
        case CMM_ARRAY:
            // Find the base
            base = type->base;
            while (base->class == CMM_ARRAY) {
                base = base->base;
            }
            // Print the base recursively
            _print_type(base, "");
            // Print dimension from left to right
            while (type->class == CMM_ARRAY) {
                printf("[%d]", type->size);
                type = type->base;
            }
            break;
        case CMM_STRUCT:
            printf("%s %s{", class_s[type->class], type->name);
            // Print field list
            link = type->field;
            while (link != NULL) {
                _print_type(link->base, " ");
                printf("%s;", link->name);
                link = link->link;
            }
            printf("}");
            break;
        case CMM_FUNC:
            // Print return type
            _print_type(type->ret, " ");
            // Print function name
            printf("%s(", type->name);
            // Print param list
            link = type->param;
            while (link != NULL && link->link != NULL) {
                _print_type(link->base, ", ");
                link = link->link;
            }
            // Tail param handling
            if (link != NULL) {
                _print_type(link->base, "");
            }
            printf(")");
            break;
        case CMM_TYPE:
            _print_type(type->base, end);
            return;
        default:
            assert(0);
    }

    printf("%s", end);
}

void print_type(const Type *type)
{
    _print_type(type, "\n");
}

// Test
#ifdef TEST_TYPE
int main()
#else  // ifndef TEST_TYPE
int test_cmm_type()
#endif // endif  TEST_TYPE
{
    Type *top = BASIC_INT;
    Type *current = top;
    Type *types[100];
    Type *struc;
    Type *foo;
    for (int i = 0; i < 100; i++) {
        int rd = rand();
        int idx = (unsigned)rd % 3;

        switch (idx) {
            case 0:
                current = new_type(CMM_ARRAY, NULL, current, NULL);
                break;
            case 1:
                struc = new_type(CMM_STRUCT, "hello", NULL, NULL);
                struc->field = new_type(CMM_FIELD, "a", top, NULL);
                struc->field = new_type(CMM_FIELD, "b", top, struc->field);
                current = struc;
                break;
            case 2:
                foo = new_type(CMM_FUNC, "foo", current, NULL);
                foo->param = new_type(CMM_PARAM, "boo", BASIC_INT, NULL);
                foo->param = new_type(CMM_PARAM, "coo", BASIC_FLOAT, foo->param);
                current = foo;
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


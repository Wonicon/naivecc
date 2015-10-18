#include "cmm_type.h"
#include <stdlib.h> /* malloc */
#include <string.h> /* strlen */
#include <assert.h> /* assert */

/* The basic type */
TypeHead type_int = { NULL, "int", CMM_TYPE_int };
TypeHead type_float = { NULL, "float", CMM_TYPE_float };

char *strdup(const char *str)
{
    char *s = (char *)malloc(sizeof(char) * strlen(str) + 1);
    strcpy(s, str);
    return s;
}
/* Constructors {{{ */

/* Common initialization routine */
static void
new_type_init
(TypeHead *this, TypeHead *base, char *name, enum CMM_TYPE code)
{
    assert(this != NULL);
    assert(name != NULL);
    this->type = base;
    /* Deep copy the name */
    this->name = strdup(name);
    this->type_code = code;
}

#define NEW(var, type) type *var = (type *)malloc(sizeof(type))
#define INIT(var, base, name, code) new_type_init((TypeHead *)var, base, name, code)

#define concat(x, y) x ## y

#define constructor_helper(type)                      \
    concat(CMM_, type) *                              \
    concat(new_, type)(TypeHead *base, char *name) {  \
        NEW(t, concat(CMM_, type));                   \
        memset(t, 0, sizeof(*t));                     \
        INIT(t, base, name, concat(CMM_TYPE_, type)); \
        return (concat(CMM_, type) *)t;               \
    }

#include "cmm_type.template"

#undef NEW
#undef INIT
#undef concat
/* }}} constructors end */

#ifdef DEBUG
#define LOG(s, ...) fprintf(stderr, "[%s] " s "\n", __func__, ## __VA_ARGS__)
#else
#define LOG(s, ...)
#endif

#define STRUCTURE
int typecmp(TypeHead *x, TypeHead *y)
{
    LOG("Now compare %s and %s", x->name, y->name);
    /* Totally different! */
    if (x->type_code != y->type_code) {
        LOG("Totally different!");
        return 0;
    }

    assert(x->type_code == y->type_code);

    /* The function only use name to differ */
    if (x->type_code == CMM_TYPE_func) {
        LOG("Then we fall into func comparision");
        if (strcmp(x->name, y->name) != 0) {
            return 0;
        } else {
            CMM_func *func_x, *func_y;
            func_x = (CMM_func *)x;
            func_y = (CMM_func *)y;
            CMM_param *param_x, *param_y;
            param_x = func_x->param_list;
            param_y = func_y->param_list;
            while (param_x != NULL && param_y != NULL &&
                    typecmp((TypeHead *)param_x, (TypeHead *)param_y)) {
                param_x = param_x->next;
                param_y = param_y->next;
            }

            /* Compare to the end means all the same */
            if (param_x != NULL || param_y != NULL) {
                return 0;
            }

            /* Compare return type */
            return typecmp(x->type, y->type);
        }
    }

    if (x->type_code == CMM_TYPE_array) {
        LOG("Then we fall into array comparision");
        /* The array differs from its dimension! */
        TypeHead *arr_x, *arr_y;
        arr_x = x;
        arr_y = y;
        /* Use ->type to reserve the base type */
        int loopno = 1;
        while (arr_x->type != NULL && arr_y->type != NULL) {
            LOG("loop %d %s <-> %s", loopno, arr_x->name, arr_y->name);
            if (x->type_code != y->type_code) {
                return 0;
            }
#ifdef STRICT_ARRAY
            if (x->type_code != CMM_TYPE_array) {
                return 0;
            }
#endif /* ifdef STRICT_ARRAY */
            arr_x = arr_x->type;
            arr_y = arr_y->type;
            loopno++;
        }

        if (arr_x->type != NULL || arr_y->type != NULL) {
            /* The dimension differs */
            return 0;
        }
        else {
            /* Compare the base type */
            return typecmp(arr_x, arr_y);
        }
    }

    if (x->type_code == CMM_TYPE_struct) {
        LOG("Then we fall into struct comparision");
#ifdef STRUCTURE
        CMM_field *field_x = (CMM_field *)x, *field_y = (CMM_field *)y;
        /* Compare the field one by one */
        while (field_x != NULL && field_y != NULL && 
                typecmp((TypeHead *)field_x, (TypeHead *)field_y)) {
            field_x = field_x->next;
            field_y = field_y->next;
        }

        if (field_x != NULL && field_y != NULL) {  /* Mismatch */
            return 0;
        } else if (field_x != field_y) {  /* Number mismatch, one should be 0 */
            assert(field_x == NULL || field_y == NULL);
            return 0;
        } else {  /* x's and y's param list reach the end at the same time */
            return 1;
        }
#else /* just the name */
        return strcmp(x->name, y->name) == 0;
#endif /* ifdef STRUCTURE */
    }

    assert(x->type_code == CMM_TYPE_int || CMM_TYPE_float);

    return 1;
} /* typecmp */

/* Test */
#ifdef TEST_TYPE
int main()
#else  /* ifndef TEST_TYPE */
int test_cmm_type()
#endif /* endif  TEST_TYPE */
{
    TypeHead top;
    top.name = "bottom";
    top.type_code = CMM_TYPE_int;
    top.type = &type_int;
    TypeHead *current = &top;
    unsigned int rd;
    for (int i = 0; i < 100; i++) {
        rd = rand();
        int idx = rd % 3;

        switch (idx) {
            case 0:
                current = (TypeHead *)new_array(current, "array");
                ((CMM_array *)current)->size = rd;
                break;
            case 1:
                current = (TypeHead *)new_struct(current, "struct");
                ((CMM_struct *)current)->field_list = new_field(&type_float, "field1");
                ((CMM_struct *)current)->field_list->next = new_field(&type_float, "field2");
                break;
            case 2:
                current = (TypeHead *)new_func(current, "func");
                ((CMM_func *)current)->param_list = new_field(&type_int, "param1");
                ((CMM_func *)current)->param_list->next = new_field(&type_int, "param2");
                break;
            default:
                assert(0);
        }
    }

    assert(typecmp(current, current));

    int i = 1;
    int listno = 0;
    CMM_field *field = NULL;
    CMM_param *param = NULL;
    while (current != NULL) {
        switch (current->type_code) {
            case CMM_TYPE_array:
                printf("%d: array [name]%s [size]%d\n", i, current->name, ((CMM_array *)current)->size);
                break;
            case CMM_TYPE_struct:
                printf("%d: struct [name]%s\n", i, current->name);
                field = ((CMM_struct *)current)->field_list;
                listno = 1;
                while (field != NULL) {
                    printf("    field %d: [name]%s [type]%s\n", listno, field->name, field->type->name);
                    field = field->next;
                }
                break;
            case CMM_TYPE_func:
                printf("%d: func [name]%s\n", i, current->name);
                param = ((CMM_func *)current)->param_list;
                listno = 1;
                while (param != NULL) {
                    printf("    param %d: [name]%s [type]%s\n", listno, param->name, param->type->name);
                    param = param->next;
                }
                break;
            case CMM_TYPE_int:
                printf("HIT GOOD TRAP\n");
                break;
            default:
                printf("Unexpected: %d\n", current->type_code);
                assert(0);
        }
        current = current->type;
    }

    return 0;
}

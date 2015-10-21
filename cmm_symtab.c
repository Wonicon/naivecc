#include "cmm_symtab.h"
#include "cmm_type.h"
#include "lib.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct _sym_ent_t
{
    char *symbol;   /* The symbol string */
    CmmType *type;  /* The generic type pointer, we can know the real type by dereferncing it */
    int line;       /* The line number this symbol first DECLARED */
    int size;       /* The size this symbol will occupy the memory, maybe we can get it from the type? */
    int address;    /* The memory address, or register code */
    int scope;      /* The scope index, each symbol will get exactly one */
    struct _sym_ent_t *link;  /* Used for open hashing */
} sym_ent_t;

#define SIZE 0x3fff
static sym_ent_t symtab[SIZE] = { 0 };

unsigned int hash(char *name)
{
    unsigned int val = 0, i;
    for (; *name; ++name) {
        val = (val << 2) + *name;
        i = val * SIZE;
        if (i) {
            val = (val ^ (i >> 12)) & SIZE;
        }
    }
    return val;
}

int insert(char *sym, CmmType *type, int line, int scope)
{
    assert(sym != NULL);
    int ret = 0;
    sym_ent_t *dest = NULL;
    unsigned int index = hash(sym);
    LOG("Hash index %u", index);
    if (symtab[index].symbol == NULL) {
        LOG("Symbol %s attained the first blood at slot %d. Congratulations!", sym, index);
        dest = &symtab[index];
    } else {
        if (!strcmp(sym, symtab[index].symbol)) {
            LOG("To query %s: collision detected at slot %d", sym, index);
            return -1;
        } else {
            LOG("Unfortunately, slot %d is occupied by '%s'", index, symtab[index].symbol);
        }

        dest = symtab[index].link;
        sym_ent_t *pre = NULL;
        ret = 1;
        while (dest != NULL) {
            ret++;
            if (!strcmp(sym, symtab[index].symbol)) {
                LOG("To query %s: collision detected at slot %d, list %d", sym, index, ret);
                return -1;
            } else {
                LOG("Unfortunately, slot %d's list has '%s'", index, dest->symbol);
            }
            pre = dest;
            dest = dest->link;
        }

        if (pre == NULL) {
            dest = symtab[index].link = (sym_ent_t *)malloc(sizeof(sym_ent_t));
        } else {
            dest = pre->link = (sym_ent_t *)malloc(sizeof(sym_ent_t));
        }
    }

    dest->symbol = sym;
    dest->type = type;
    dest->line  = line;
    dest->scope = scope;

    return ret;
}

int query(char *sym, int scope)
{
    assert(sym);
    unsigned int index = hash(sym);
    sym_ent_t *scanner = &symtab[index];

    if (scanner->symbol == NULL) {
        LOG("To query %s: totaly emtpy slot", sym);
        return 0;
    }

    int listno = 0;
    while (scanner != NULL) {
        if (!strcmp(sym, scanner->symbol)) {
            LOG("To query %s: collision detected at slot %d, list %d", sym, index, listno);
            /* TODO: check scope */
            return 1;
        }
        listno++;
        scanner = scanner->link;
    }

    return 0;
}

void print_symtab()
{
    for (int i = 0; i < SIZE; i++) {
        if (symtab[i].symbol == NULL) {
            continue;
        }
        sym_ent_t *ent = &symtab[i];
        for (int k = 0; ent != NULL; i++, ent = ent->link) {
            printf("Slot %d, list %d: symbol '%s'\n", i, k, ent->symbol);
            printf("  Type: ");
            print_type(ent->type);
            printf("  Link: %d\n", ent->line);
        }
    }
}

#ifdef TEST_SYM
int main()
#else
int test_sym()
#endif /* TEST_SYM */
{
    CmmFunc *s = new_type_func("helloworld", global_int);
    CmmArray *a1 = new_type_array(10, global_float);
    CmmArray *a2 = new_type_array(10, GENERIC(a1));
    CmmArray *a3 = new_type_array(10, GENERIC(a2));

    insert(s->name, GENERIC(s), 10, -1);
    insert(s->name, GENERIC(s), 10, -1);
    print_symtab();
    return 0;
}
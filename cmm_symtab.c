#include "cmm_symtab.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define SIZE 0x3fff
static sym_ent_t *symtab[SIZE] = { 0 };

unsigned int hash(const char *name)
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

int insert(const char *sym, CmmType *type, int line, int scope) {
    assert(sym != NULL);

    unsigned int index = hash(sym);
    LOG("Hash index %u", index);

    // Find collision or reach the end
    sym_ent_t *dest = symtab[index];
    sym_ent_t *pre = NULL;
    int listno = 0;
    while (dest != NULL) {
        if (!strcmp(sym, symtab[index]->symbol)) {
            LOG("To insert %s: collision detected at slot %d, list %d", sym, index, listno);
            return -1;
        } else {
            LOG("Unfortunately, slot %d's list has '%s'", index, dest->symbol);
            pre = dest;
            dest = dest->link;
        }
        listno++;
    }

    if (pre == NULL) {
        LOG("The slot %d is empty, insert %s here", index, sym);
        assert(dest == symtab[index]);
        dest = symtab[index] = NEW(sym_ent_t);
    } else {
        dest = pre->link = NEW(sym_ent_t);
    }  // NOTE we should modify the pointer stored in the hash table, but not only `dest'

    dest->symbol = sym;
    dest->type = type;
    dest->line  = line;
    dest->scope = scope;

    return 1;
}

sym_ent_t *query(const char *sym, int scope)
{
    assert(sym);
    unsigned int index = hash(sym);
    sym_ent_t *scanner = symtab[index];

    if (scanner == NULL) {
        LOG("To query %s: totaly emtpy slot", sym);
        return 0;
    }

    int listno = 0;
    while (scanner != NULL) {
        if (!strcmp(sym, scanner->symbol)) {
            LOG("To query %s: collision detected at slot %d, list %d", sym, index, listno);
            // TODO: check scope
            return scanner;
        } else {
            listno++;
            scanner = scanner->link;
        }
    }

    return NULL;
}

void print_symtab()
{
    for (int i = 0; i < SIZE; i++) {
        if (symtab[i] == NULL) {
            continue;
        }
        sym_ent_t *ent = symtab[i];
        for (int k = 0; ent != NULL; i++, ent = ent->link) {
            printf("Slot %d, list %d: symbol '%s'\n", i, k, ent->symbol);
            printf("  Type: ");
            print_type(ent->type);
            printf("  Line: %d\n", ent->line);
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
    insert("a_new_array", GENERIC(a3), 10, 0);
    print_symtab();

    sym_ent_t *ret = query("a_new_array", 0);
    if (ret != NULL) {
        printf("Found!\n");
    }

    ret = query("sdf", 0);
    if (ret == NULL) {
        printf("Miss\n");
    }
    return 0;
}

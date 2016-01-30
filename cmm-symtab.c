#include "cmm-symtab.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define SIZE 0x3f

unsigned int hash(const char *name)
{
    unsigned int val = 0, i;
    for (; *name; name++) {
        val = (val << 2) + *name;
        i = val * SIZE;
        if (i) {
            val = (val ^ (i >> 12)) & SIZE;
        }
    }
    return val;
}

int insert(const char *sym, Type *type, int line, Symbol **table)
{
    assert(sym != NULL);
    assert(type != NULL);

    unsigned int index = hash(sym);
    LOG("Hash index %u", index);

    // Find collision or reach the end
    Symbol **ptr = &table[index];
    int listno = 0;
    while (*ptr != NULL) {
        if (!strcmp(sym, table[index]->symbol)) {
            LOG("To insert %s: collision detected at slot %d, list %d", sym, index, listno);
            return -1;
        }
        else {
            LOG("Unfortunately, slot %d's list has '%s'", index, dest->symbol);
            ptr = &((*ptr)->link);
            listno++;
        }
    }

    *ptr = malloc(sizeof(Symbol));
    memset(*ptr, 0, sizeof(Symbol));
    (*ptr)->symbol = sym;
    (*ptr)->type   = type;
    (*ptr)->line   = line;

    return 1;
}

const Symbol *query(const char *sym, Symbol **table)
{
    assert(sym != NULL);
    assert(table != NULL);

    unsigned int index = hash(sym);
    Symbol *scanner = table[index];

    if (scanner == NULL) {
        LOG("Cannot find %s", sym);
        return NULL;
    }

    int listno = 0;
    while (scanner != NULL) {
        if (!strcmp(sym, scanner->symbol)) {
            LOG("To query %s: collision detected at slot %d, list %d", sym, index, listno);
            return scanner;
        }
        else {
            scanner = scanner->link;
            listno++;
        }
    }

    LOG("Cannot find %s", sym);
    return NULL;
}

void init_symtab()
{
}

void new_symtab()
{
}

Symbol **pop_symtab()
{
    return NULL;
}

Symbol **get_symtab_top()
{
    return NULL;
}


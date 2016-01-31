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


static Symbol ***scopes = NULL;

static size_t scope_capacity = 1;

static size_t scope_cnt = 0;

void new_symtab()
{
    if (scope_cnt == scope_capacity) {
        scope_capacity *= 2;
        scopes = realloc(scopes, scope_capacity);
    }

    scopes[scope_cnt++] = calloc(SIZE, sizeof(Symbol *));
}

void init_symtab()
{
    scope_capacity = 1;
    scope_cnt = 0;
    scopes = calloc(scope_capacity, sizeof(Symbol **));
}

void push_symtab(Symbol **symtab)
{
    if (scope_cnt == scope_capacity) {
        scope_capacity *= 2;
        scopes = realloc(scopes, scope_capacity);
    }

    scopes[scope_cnt++] = symtab;
}

Symbol **pop_symtab()
{
    if (scope_cnt == 0) {
        return NULL;
    }
    else {
        return scopes[--scope_cnt];
    }
}

Symbol **get_symtab_top()
{
    return scopes[scope_cnt - 1];
}


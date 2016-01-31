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

Symbol *insert(const char *sym, Type *type, int line, Symbol **table)
{
    assert(sym != NULL);
    assert(type != NULL);

    unsigned int index = hash(sym);

    // Find collision or reach the end
    Symbol **ptr = &table[index];
    int listno = 0;
    while (*ptr != NULL) {
        if (!strcmp(sym, table[index]->symbol)) {
            LOG("When inserting %s: collision detected at slot %d, list %d", sym, index, listno);
            return NULL;
        }
        else {
            ptr = &((*ptr)->link);
            listno++;
        }
    }

    LOG("Insert %s at slot %d, list %d", sym, index, listno);
    *ptr = malloc(sizeof(Symbol));
    memset(*ptr, 0, sizeof(Symbol));
    (*ptr)->symbol = sym;
    (*ptr)->type   = type;
    (*ptr)->line   = line;

    return *ptr;
}

// query_without_fallback:
//   The basic query procedure.
//   Most of the query for symbols should use `query' with fallback support.
//   This function can be used by structure type as an exception.
const Symbol *query_without_fallback(const char *sym, Symbol **table)
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
            LOG("Find %s at slot %d, list %d", sym, index, listno);
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

// query:
//   query the given symbol 'sym' in the current scope,
//   if the query is failed, it will fallback to the upper scope.
//
//   if the query is successful, the symbol pointer is returned,
//   the caller is expected not to modify the symbol.
//   if the symbol does not exists, the function will return NULL.
const Symbol *query(const char *sym)
{
    // idx is unsigned, when idx is zero, and idx-- will make it the largest number.
    for (size_t idx = scope_cnt - 1; idx < scope_cnt; idx--) {
        const Symbol *result = query_without_fallback(sym, scopes[idx]);
        if (result != NULL) {
            return result;
        }
    }

    return NULL;
}

void new_symtab()
{
    if (scope_cnt == scope_capacity) {
        scope_capacity *= 2;
        scopes = realloc(scopes, sizeof(Symbol ***) * scope_capacity);
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
        scopes = realloc(scopes, sizeof(Symbol ***) * scope_capacity);
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
    if (scope_cnt == 0) {
        return NULL;
    }
    else {
        return scopes[scope_cnt - 1];
    }
}


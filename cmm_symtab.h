#ifndef CMM_SYMTAB_H
#define CMM_SYMTAB_H

//
// C-- symbol table:
// This module handle the storing, inserting and querying of symbols.
// We use a hash table with open hashing method to store the symbol.
// The query will return a clone of the found symbol. Modifying the returned symbol
// will make no sense.
//
// One thing deserves to mention is the scope.
// Scope is the state determined by parser. So users should provide this attribute.
// As symbols with the same name will store in the same slot, we can conveniently access every
// overriding symbol. If the scope is mismatched, the query will select the nearest scope that
// is smaller than the provided one.
//
// We store structure types together with variables, functions in one single symbol table.
// This may be ambiguous when we consulting a symbol. One way to solve this ambiguity is
// check the symbol string with the string pointer stored in the structure type.
//

#include "cmm_type.h"

typedef struct _sym_ent_t
{
    const char *symbol;   /* The symbol string */
    CmmType *type;  /* The generic type pointer, we can know the real type by dereferncing it */
    int line;       /* The line number this symbol first DECLARED */
    int size;       /* The size this symbol will occupy the memory, maybe we can get it from the type? */
    int address;    /* The memory address, or register code */
    int scope;      /* The scope index, each symbol will get exactly one */
    struct _sym_ent_t *link;  /* Used for open hashing */
} sym_ent_t;

int insert(const char *sym, CmmType *type, int line, int scope);
// NOTE the return value is a shallow copy of the found symbol
// Release it directly through free, DO NOT release any of its pointer fields.
sym_ent_t *query(const char *sym, int scope);
void print_symtab();

#endif /* CMM_SYMTAB_H */

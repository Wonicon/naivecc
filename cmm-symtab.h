#ifndef CMM_SYMTAB_H
#define CMM_SYMTAB_H

//
// C-- symbol table:
// This module handles the storing, inserting and querying of symbols.
// We use a hash table with open hashing method to store symbols.
// The query will return a clone of the found symbol. Modifying the returned symbol
// will make no sense.
//
// One thing deserves to mention is the scope.
// Scope is the state determined by parser. So this attribute should be provided by the user.
// But the symbol table module will help maintain the hierarchy of nested scopes, and perform
// the fall back schema.
//
// We store structure types together with variables, functions in one single symbol table.
// structure type name can conflict with another variable. To solve the problem, we decide
// to store the structure type name with modified string like 'struct typename'
//

#include "cmm-type.h"
#include "operand.h"

typedef struct _Symbol {
    struct _Symbol *link;  // Used for open hashing
    const char *symbol;    // The symbol string
    int line;              // The line where the symbol first DECLARED
    Type *type;            // Detailed type information
    Operand address;       // The ir destination
    int offset;            // The field offset in struct
    const struct _Symbol **field_table;  // Valid only for structure type
} Symbol;

Symbol *insert(const char *sym, Type *type, int line, Symbol **table);
const Symbol *query_without_fallback(const char *sym, Symbol **table);
const Symbol *query(const char *sym);

void init_symtab();
void new_symtab();
void push_symtab(Symbol **);
Symbol **pop_symtab();
Symbol **get_symtab_top();

#endif // CMM_SYMTAB_H


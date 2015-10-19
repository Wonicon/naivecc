#ifndef CMM_SYMTAB_H
#define CMM_SYMTAB_H

/*
 * We store structure types together with variables, functions in one single symbol table.
 * This may be ambiguous when we consulting a symbol. One way to solve this ambiguity is
 * check the symbol string with the string pointer stored in the structure type.
 */

#include "cmm_type.h"

int insert(char *sym, CmmType *type, int line, int scope);
int query(char *sym, int scope);

#endif /* CMM_SYMTAB_H */

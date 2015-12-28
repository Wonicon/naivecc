#include "cmm-strtab.h"
#include "cmm-symtab.h"
#include "node.h"
#include <string.h>

// from syntax.tab.c
int yyrestart(FILE *);
int yyparse();
void ast();
void semantic_analysis();
void free_ast();
void simplify();
void translate();

extern int is_lex_error;
extern int is_syn_error;
extern bool semantic_error;

FILE *asm_file = NULL;

int main(int argc, char *argv[]) {
    if (argc <= 1) {
        return 1;
    }

    // ./cc src.cmm out.s
    FILE *file;
    file     = fopen(argv[0], "r");
    asm_file = fopen(argv[1], "w");

    if (!file) {
        perror(argv[i]);
        return 1;
    } else if (!asm_file) {
        perror(argv[i + 1]);
        return 1;
    }

    init_strtab();

    yyrestart(file);

    yyparse();

    if (!is_syn_error) {
        // Add predefined functions
        Type *read = new_type(CMM_FUNC, "read", NULL, NULL);
        read->ret = BASIC_INT;
        insert("read", read, -1, 0);

        Type *write = new_type(CMM_FUNC, "write", NULL, NULL);
        write->param = new_type(CMM_PARAM, "o", BASIC_INT, NULL);
        insert("write", write, -1, 0);

        semantic_analysis();

#ifdef DEBUG
        print_symtab();
        printf("======================================================\n");
#endif

        if (!semantic_error) {
            simplify();
            translate();
        }
    }

    free_ast();
    return 0;
}


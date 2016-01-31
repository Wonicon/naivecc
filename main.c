#include "cmm-strtab.h"
#include "cmm-symtab.h"
#include "node.h"
#include "asm.h"
#include <string.h>


// from syntax.tab.c
int yyrestart(FILE *);
int yyparse();
void ast();
void semantic_analysis();
void free_ast();
void translate();


extern int is_syn_error;
extern bool semantic_error;


int main(int argc, char *argv[])
{
    if (argc <= 1) {
        return 1;
    }

    // ./cc src.cmm out.s
    FILE *file;
    file     = fopen(argv[1], "r");
    asm_file = fopen(argv[2], "w");

    if (!file) {
        perror(argv[1]);
        return 1;
    }
    else if (!asm_file) {
        perror(argv[2]);
        return 1;
    }

    init_strtab();

    yyrestart(file);

    yyparse();

    if (!is_syn_error) {
        semantic_analysis();

        if (!semantic_error) {
            translate();
        }
    }

    free_ast();
    return 0;
}


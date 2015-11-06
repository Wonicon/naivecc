#include "cmm_strtab.h"
#include "cmm_symtab.h"
#include <string.h>

/* from syntax.tab.c */
extern int yyrestart(FILE *);
extern int yyparse();
extern void ast();
extern void semantic_analysis();
extern void free_ast();

extern int yydebug;

extern int is_lex_error;
extern int is_syn_error;

int is_greedy = 0;
int is_check_return = 0;

int main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        return 1;
    }

    // Handle parameters
    int i = 1;
    if (argc >= 3)
    {
        for (; i < argc - 1; i++)
        {
            if (!strcmp(argv[i], "-g") || !strcmp(argv[i], "greedy"))
            {
                is_greedy = 1;
            }
            else if (!strcmp(argv[i], "--check-return"))
            {
                is_check_return = 1;
            }
        }
    }

    FILE* file = fopen(argv[i], "r");
    if (!file)
    {
        perror(argv[i]);
        return 1;
    }

    init_strtab();
    yyrestart(file);
    //yydebug = 1;
    yyparse();
    if (!is_syn_error)
    {
        //ast();
        semantic_analysis();
    }
    free_ast();
#ifdef DEBUG
    print_symtab();
#endif
    return 0;
}

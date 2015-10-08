#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* from syntax.tab.c */
extern int yyrestart(FILE *);
extern int yyparse();
extern void ast();
extern void free_ast();

#ifdef YYDEBUG
extern int yydebug;
#endif

extern int is_lex_error;
extern int is_syn_error;

int is_greedy = 0;

int main(int argc, char *argv[])
{
    if (argc <= 1) return 1;
    int i = 1;
    if (argc >= 3) {
        i++;
        if (!strcmp(argv[1], "-g") || !strcmp(argv[1], "greedy")) {
            is_greedy = 1;
        }
    }
    FILE* f = fopen(argv[i], "r");
    if (!f)
    {
        perror(argv[i]);
        return 1;
    }
    yyrestart(f);
#ifdef YYDEBUG
    yydebug = 1;
#endif
    yyparse();
    if (is_syn_error) {
        printf("Failed to print ast due to previous errors.\n");
    }
    else {
        ast();
    }
    free_ast();
    return 0;
}

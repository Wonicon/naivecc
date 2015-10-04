#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int yyrestart(FILE *);
extern int yyparse();
extern int yydebug;
extern void ast();

int is_lex_error = 0;
int is_syn_error = 0;

int main(int argc, char *argv[])
{
    if (argc <= 1) return 1;
    FILE* f = fopen(argv[1], "r");
    if (!f)
    {
        perror(argv[1]);
        return 1;
    }
    yyrestart(f);
    //yydebug = 1;
    yyparse();
    if (is_syn_error) {
        printf("Failed to print ast due to previous errors.\n");
    }
    else {
        ast();
    }
    return 0;
}

char *split(char *msg)
{
    char *s = msg;
    char *err = strtok(s, ",");
    char *unexp = strtok(NULL, ",");
    char *exp = strtok(NULL, "");

    unexp = strtok(unexp, " ");
    unexp = strtok(NULL, " ");

    exp = strtok(exp, " ");
    exp = strtok(NULL, " ");

    return exp;
}

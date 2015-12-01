#include "cmm_strtab.h"
#include "cmm_symtab.h"
#include "node.h"
#include <string.h>

/* from syntax.tab.c */
int yyrestart(FILE *);
int yyparse();
void ast();
void semantic_analysis();
void free_ast();
void simplify();
void translate();

extern int yydebug;

extern int is_lex_error;
extern int is_syn_error;
extern bool semantic_error;

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

        // 添加预设函数
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

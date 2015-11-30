#include "lib.h"
#include <stdlib.h>
#include <string.h>

char *cmm_strdup(const char *str)
{
    char *s = (char *)malloc(strlen(str));
    memset(s, 0, strlen(str) + 1);
    strcpy(s, str);
    return s;
}
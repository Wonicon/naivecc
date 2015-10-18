#include "lib.h"
#include <stdlib.h>
#include <string.h>

char *cmm_strdup(const char *str)
{
    int len = strlen(str);
    char *s = (char *)malloc(sizeof(*str) * (len + 1));
    strcpy(s, str);
    return s;
}

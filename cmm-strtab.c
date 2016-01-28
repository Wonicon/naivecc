#include "cmm-strtab.h"
#include "lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memory.h>


#define DEFAULT_SIZE 64


static const char **strtab = NULL;
static size_t strtab_size = 0;
static size_t strtab_capacity = 0;


void init_strtab()
{
    strtab = (const char **)calloc(DEFAULT_SIZE, sizeof(char *));
    strtab_capacity = DEFAULT_SIZE;
}


//
// Store the string in the strtab
// We assume that the table will never delete entries
//
const char *register_str(const char *str)
{
    for (size_t i = 0; i < strtab_size; i++) {
        if (!strcmp(strtab[i], str)) {
            // Find duplicated string
            return strtab[i];
        }
    }

    if (strtab_size >= strtab_capacity) {
        // Extend the capacity
        strtab_capacity = strtab_capacity + strtab_capacity / 2;
        strtab = (const char **)realloc(strtab, strtab_capacity * sizeof(char *));
    }

    const char *s = cmm_strdup(str);
    strtab[strtab_size++] = s;
    return s;
}


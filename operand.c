//
// Created by whz on 15-11-29.
//

#include "operand.h"
#include "lib.h"
#include <string.h>


// 操作数缓冲区, 用于当前基本块的分析
typedef struct {
    Operand buf[4096];
    int count;
} OpeTable;


OpeTable opetab = { { 0 }, 0 };


void print_operand(Operand ope, char *str);


void init_opetable()
{
    memset(&opetab, 0, sizeof(opetab));
}

void addope(Operand ope)
{
    for (int i = 0; i < opetab.count; i++) {
        if (opetab.buf[i] == ope) {
            return;
        }
    }
#ifdef DEBUG
    char strbuf[32];
    print_operand(ope, strbuf);
    LOG("加入操作数:%s", strbuf);
#endif
    opetab.buf[opetab.count++] = ope;
}

Operand getope(int idx)
{
    if (idx < opetab.count) {
        return opetab.buf[idx];
    } else {
        return NULL;
    }
}
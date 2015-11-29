//
// Created by whz on 15-11-29.
//

#ifndef __OPERAND_H__
#define __OPERAND_H__

#include "ir.h"

// 操作数表接口
void init_opetable();
void addope(Operand ope);
Operand getope(int idx);

// 判定接口
bool is_always_live(Operand ope);
bool is_const(Operand ope);
bool is_tmp(Operand ope);

// 常规接口
Operand new_operand(Ope_Type type);
void print_operand(Operand ope, char *str);
Operand calc_const(IR_Type op, Operand left, Operand right);

// 调试
#ifdef DEBUG
#define LOG_OPE(operand)                \
    do {                                \
        char strbuf[32];                \
        print_operand(operand, strbuf); \
        LOG("%s", strbuf);              \
    } while (0)
#else
#define LOG_OPE(operand)
#endif
#endif //__OPERAND_H__
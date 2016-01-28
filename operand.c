//
// Created by whz on 15-11-29.
//

#include "operand.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

// 出口变量判断
bool is_always_live(Operand ope)
{
    switch (ope->type) {
        case OPE_VAR:
        case OPE_REF:
        case OPE_BOOL:
            return true;
        default:
            return false;
    }
}

// 操作数构造函数
Operand new_operand(Ope_Type type)
{
    static int nr_ope = 0;  // Use uniform encoding for operands
    static int nr_label = 0;

    Operand p = (Operand)malloc(sizeof(struct Operand_));
    memset(p, 0, sizeof(*p));
    p->type = type;
    switch (type) {
        case OPE_VAR:
            p->liveness = ALIVE;
            p->size = 4;
            p->index = nr_ope++;
            break;
        case OPE_REF:
            p->liveness = ALIVE;
            p->index = nr_ope++;
            break;
        case OPE_BOOL:
            p->liveness = ALIVE;
            p->size = 4;
            p->index = nr_ope++;
            break;
        case OPE_TEMP:
            p->size = 4;
            p->index = nr_ope++;
            break;
        case OPE_ADDR:
            p->size = 4;
            p->index = nr_ope++;
            break;
        case OPE_LABEL:
            p->liveness = 1;
            p->label = nr_label++;
            break;
        case OPE_FUNC:
            p->liveness = 1;
            break;
        default:
            break;
    }
    return p;
}

// 打印操作数, 为了变量和标签工厂函数可以简单实现, 以及方便比较,
// 变量和标签都存储为整型, 在打印操作数的时候加上统一前缀变成合法的变量名
// NOP指令答应为空字符串, 希望将来可以自动过滤.
const char *print_operand(Operand ope)
{
    static char str[NAME_LEN];

    if (ope == NULL) {
        sprintf(str, "%s", "");
    }
    else {
        switch (ope->type) {
            case OPE_VAR:
                sprintf(str, "v%d", ope->index);
                break;
            case OPE_REF:
                //sprintf(str, "r%d", ope->index);
                sprintf(str, "r%d_%d", ope->index, ope->size);
                break;
            case OPE_BOOL:
                sprintf(str, "b%d", ope->index);
                break;
            case OPE_FUNC:
                sprintf(str, "%s", ope->name);
                break;
            case OPE_TEMP:
                sprintf(str, "t%d", ope->index);
                break;
            case OPE_ADDR:
                sprintf(str, "a%d", ope->index);
                break;
            case OPE_DEREF:
                sprintf(str, "*a%d", ope->index);
                break;
            case OPE_FLOAT:
                sprintf(str, "#%f", ope->real);
                break;
            case OPE_LABEL:
                sprintf(str, "L%d", ope->label);
                break;
            case OPE_INTEGER:
                sprintf(str, "#%d", ope->integer);
                break;
            case OPE_REFADDR:
                sprintf(str, "&r%d", ope->index);
                break;
            default:
                sprintf(str, "%s", "");
        }
    }

    return str;
}

// 常量判断
bool is_const(Operand ope)
{
    return ope->type == OPE_INTEGER || ope->type == OPE_FLOAT;
}

// 块内临时变量判断
bool is_tmp(Operand ope)
{
    if (ope == NULL) {
        return false;
    }
    else {
        return ope->type == OPE_ADDR || ope->type == OPE_TEMP;
    }
}

// 常量计算
Operand calc_const(IR_Type op, Operand left, Operand right)
{
    assert(left->type == right->type && is_const(left));
    assert(IR_ADD <= op && op <= IR_DIV);

    Operand rst = new_operand(left->type);
    switch (left->type) {
        case OPE_INTEGER: {
            switch (op) {
                case IR_ADD:
                    rst->integer = left->integer + right->integer;
                    break;
                case IR_SUB:
                    rst->integer = left->integer - right->integer;
                    break;
                case IR_MUL:
                    rst->integer = left->integer * right->integer;
                    break;
                case IR_DIV:
                    rst->integer = left->integer / right->integer;
                    break;
                default: assert(0);
            }
            break;
        }
        case OPE_FLOAT: {
            switch (op) {
                case IR_ADD:
                    rst->real = left->real + right->real;
                    break;
                case IR_SUB:
                    rst->real = left->real - right->real;
                    break;
                case IR_MUL:
                    rst->real = left->real * right->real;
                    break;
                case IR_DIV:
                    rst->real = left->real / right->real;
                    break;
                default: assert(0);
            }
            break;
        }
        default: assert(0);
    }
    return rst;
}

Operand get_neg(Operand ope)
{
    if (is_const(ope)) {
        Operand p = new_operand(ope->type);
        if (ope->type == OPE_INTEGER) {
            p->integer = -ope->integer;
        }
        else if (ope->type == OPE_FLOAT) {
            p->real = -ope->real;
        }
        else {
            LOG("Error");
            assert(0);
        }
        return p;
    }
    else {
        LOG("Not const");
        assert(0);
    }
}

bool cmp_operand(Operand first, Operand second)
{
    if (first == second) {
        return true;
    }
    else if (is_const(first) && is_const(second) && first->type == second->type) {
        switch (first->type) {
            case OPE_INTEGER: return first->integer == second->integer;
            case OPE_FLOAT: return first->real == second->real;
            default: PANIC("Unexpected");
        }
        return false;
    }
    else {
        return false;
    }
}

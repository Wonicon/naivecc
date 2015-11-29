//
// Created by whz on 15-11-29.
//

#include "operand.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>


// 操作数缓冲区, 用于当前基本块的分析
typedef struct {
    Operand buf[4096];
    int count;
} OpeTable;


// 操作数表, 每个块处理DAG时临时使用
OpeTable opetab = { { 0 }, 0 };


// 初始化操作数表, 防止残留在内部的非空指针造成干扰
void init_opetable()
{
    memset(&opetab, 0, sizeof(opetab));
}


// 加入操作数指针, 只有指令中有效的操作数才会被加入
// 像 *= 这样只是用来获取依赖的目标操作数就不会被加入
void addope(Operand ope)
{
    for (int i = 0; i < opetab.count; i++) {
        if (opetab.buf[i] == ope) {
            return;
        }
    }
    LOG_OPE(ope);
    opetab.buf[opetab.count++] = ope;
}

// 遍历用, 主要用来查看哪些出口变量还没有被赋值
Operand getope(int idx)
{
    if (idx < opetab.count) {
        return opetab.buf[idx];
    } else {
        return NULL;
    }
}

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
Operand new_operand(Ope_Type type) {
    static int nr_var = 0;
    static int nr_ref = 0;
    static int nr_tmp = 0;
    static int nr_bool = 0;
    static int nr_addr = 0;
    static int nr_label = 0;

    Operand p = (Operand)malloc(sizeof(struct Operand_));
    p->type = type;
    switch (type) {
        case OPE_VAR:
            p->liveness = ALIVE;
            p->index = nr_var++;
            break;
        case OPE_REF:
            p->liveness = ALIVE;
            p->index = nr_ref++;
            break;
        case OPE_BOOL:
            p->liveness = ALIVE;
            p->index = nr_bool++;
            break;
        case OPE_TEMP:
            p->index = nr_tmp++;
            break;
        case OPE_ADDR:
            p->index = nr_addr++;
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
void print_operand(Operand ope, char *str) {
    if (ope == NULL) {
        sprintf(str, "%s", "");
        return;
    }
    switch (ope->type) {
        case OPE_VAR:     sprintf(str, "v%d",  ope->index);    break;
        case OPE_REF:     sprintf(str, "r%d",  ope->index);    break;
        case OPE_BOOL:    sprintf(str, "b%d",  ope->index);    break;
        case OPE_FUNC:    sprintf(str, "%s",   ope->name);     break;
        case OPE_TEMP:    sprintf(str, "t%d",  ope->index);    break;
        case OPE_ADDR:    sprintf(str, "a%d",  ope->index);    break;
        case OPE_DEREF:   sprintf(str, "*a%d", ope->index);    break;
        case OPE_FLOAT:   sprintf(str, "#%f",  ope->real);     break;
        case OPE_LABEL:   sprintf(str, "L%d",  ope->label);    break;
        case OPE_INTEGER: sprintf(str, "#%d",  ope->integer);  break;
        case OPE_REFADDR: sprintf(str, "&r%d", ope->index);    break;
        default:          sprintf(str, "%s",   "");
    }
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
    } else {
        return ope->type == OPE_ADDR ||
               ope->type == OPE_TEMP ||
               ope->type == OPE_DEREF;
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
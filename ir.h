//
// Created by whz on 15-11-20.
//

//
// 中间代码相关模块
//

#ifndef __IR_H__
#define __IR_H__

typedef struct Operand_ *Operand;

enum Ope_Type {
    OPE_NOT_USED,
    OPE_VARIABLE,
    OPE_INTEGER,
    OPE_FLOAT,
    OPE_LABEL,
    OPE_FUNC
};
struct Operand_ {
    enum Ope_Type type;
    union {
        int index;
        int integer;
        float real;
        int label;
        const char *funcname;
    } var;
};

enum IR_Type {
    IR_LABEL,
    IR_FUNC,

    // 运算类指令
    IR_ASSIGN,
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_ADDR,     // 寻址, 相当于 &x
    IR_DEREF_R,  // 获取 rs 指向的地址的值
    IR_DEREF_L,  // 写入 rd 指向的地址

    // 跳转类指令, 含义参考 MIPS
    IR_BEQ,
    IR_JMP,
    IR_BLT,
    IR_BLE,
    IR_BGT,
    IR_BGE,
    IR_BNE,
    IR_RET,

    // 内存类
    IR_DEC,

    // 函数类指令
    IR_ARG,      // 参数压栈
    IR_CALL,
    IR_PRARM,    // 参数声明

    // I/O类指令
    IR_READ,
    IR_WRITE,

    // 无效指令
    IR_NOP
};
struct IR {
    enum IR_Type type;
    Operand rs;      // 第一源操作数
    Operand rt;      // 第二源操作数
    Operand rd;      // 目的操作数
};

#include "node.h"
#include <stdio.h>

//
// 中间代码模块对外接都口
//
Operand new_operand(enum Ope_Type type);
int new_instr(enum IR_Type type, Operand rs, Operand rt, Operand rd);
void print_instr(FILE *stream);
int new_variable();
int new_lable();

#endif // __IR_H__

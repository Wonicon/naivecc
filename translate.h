//
// Created by whz on 15-11-18.
//

#ifndef __TRANSLATE_H__
#define __TRANSLATE_H__

typedef struct Operand_ *Operand;

struct Operand_ {
    enum {
        OPE_VARIABLE,
        OPE_INTEGER,
        OPE_FLOAT,
        OPE_LABEL,     // 标签或者函数名
    } type;
    union {
        int index;
        int integer;
        float real;
        const char *label;
    } var;
};

struct IR {
    enum {
        // 运算类指令
        IR_ASSIGN,
        IR_ADD,
        IR_SUB,
        IR_MUL,
        IR_DIV,
        IR_ADDR,     // 寻址, 相当于 &x
        IR_DEREF_R,  // 获取 rs 指向的地址的值
        IR_DEREF_L,  // 写入 rd 指向的地址

        // 跳转类指令, 含义参考 MIPS 指令
        IR_JMP,
        IR_BLT,
        IR_BLE,
        IR_BGT,
        IR_BGE,
        IR_BEQ,
        IR_BNE,
        IR_RET,

        // 函数类指令
        IR_ARG,      // 参数压栈
        IR_CALL,
        IR_PRARM,    // 参数声明
        IR_LABEL,
        IR_FUNC,
        // I/O类指令
        IR_READ,
        IR_WRITE,

        // 无效指令
        IR_NOP
    } type;
    Operand rs;      // 第一源操作数
    Operand rt;      // 第二源操作数
    Operand rd;      // 目的操作数
};

#include "node.h"
#include <stdio.h>

void print_instr(FILE *stream);  // 向文件打印所有指令
void translate(node_t prog);     //
#endif // __TRANSLATE_H__

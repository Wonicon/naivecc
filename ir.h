//
// Created by whz on 15-11-20.
//

//
// 中间代码相关模块
//

#ifndef __IR_H__
#define __IR_H__

#include "cmm_type.h"

#define MAX_LINE 2048

#define FAIL_TO_GEN -1
#define NO_NEED_TO_GEN -2
#define MULTI_INSTR MAX_LINE

typedef enum {
    OPE_NOT_USED,
    OPE_VAR,      // 变量
    OPE_REF,      // 引用类型, 包括数组和结构类型, 引用类型的首元素支持不解引用直接访问
                  // 所以引用类型会保存一个REFVAR分身
    OPE_TEMP,     // 编译器自行分配的临时变量
    OPE_ADDR,     // 地址值, 类比REF, 保存一个DEREF分身用于内联
    OPE_DEREF,    // 内联解引用
    OPE_INITIAL,  // REF的首元素分身
    OPE_INTEGER,
    OPE_FLOAT,
    OPE_LABEL,
    OPE_FUNC
} Ope_Type;

typedef struct Operand_ *Operand;

struct Operand_ {
    Ope_Type type;     // 固有属性: 指示该操作数的类型, 用于打印和常量折叠
    int label;         // 固有属性: 指示该操作数作为标签时的编号(最后会被替换成行号)
    int index;         // 固有属性: 指示该操作数作为变量/引用/地址时的编号(3者独立)
    int integer;       // 固有属性: 该操作数作为整型常数的值
    float real;        // 固有属性: 该操作数作为浮点型常数的值
    const char *name;  // 固有属性: 该操作数作为函数时的函数名
    Type *base_type;   // 固有属性: 引用型操作数对应的类型, 关系到偏移量的计算
    Operand _inline;    // 固有属性: 引用型操作数首元素, 有些时候可以用它省略一行指令
                       // INITIAL自身可以用这个指针访问到对应的引用操作数

    Type *sub_type;    // 综合属性: 引用型操作数计算本层递归偏移量的依据
                       // 由于是单向递归, 所以可以安心地进行擦写, 在翻译 ID 时初始化

    Operand offset;    // 综合属性: 引用型操作数的偏移量
                       // 在 EXP [ EXP ] 和 EXP . ID 的递归表达式中, 偏移量自底向上进行计算
                       // 如果是常数, 我们计算新值并替换, 如果此层出现了变量, 则只能生成运算指令并替换为目标操作数
                       // 在翻译 ID 时初始化

    int label_ref_cnt; // Label 作为跳转目标的引用次数, 在删除 GOTO 或者翻转 BRANCH 后, 如果引用计数归零, 可以删除
};

typedef enum {
    // 无效指令
    IR_NOP,
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
    IR_JMP,
    IR_BEQ,
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
} IR_Type;
typedef struct {
    IR_Type type;
    Operand rs;      // 第一源操作数
    Operand rt;      // 第二源操作数
    Operand rd;      // 目的操作数
    int block;
} IR;

#include "node.h"
#include <stdio.h>

//
// 中间代码模块对外接都口
//
Operand new_operand(Ope_Type type);
int new_instr(IR_Type type, Operand rs, Operand rt, Operand rd);
void print_instr(FILE *stream);
IR_Type get_relop(const char *sym);
int replace_operand_global(Operand newbie, Operand old);
#endif // __IR_H__

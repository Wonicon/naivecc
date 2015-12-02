//
// Created by whz on 15-11-29.
//

#ifndef __OPERAND_H__
#define __OPERAND_H__

#include "lib.h"

typedef struct Operand_ *Operand;
typedef struct _Type Type;
typedef struct DagNode_ *pDagNode;

struct Operand_ {
    Ope_Type type;     // 固有属性: 指示该操作数的类型, 用于打印和常量折叠

    // OPE_LABEL
    int label;         // 固有属性: 指示该操作数作为标签时的编号(最后会被替换成行号)

    // OPE_VAR, OPE_REF, OPE_TEMP, OPE_ADDR
    int index;         // 固有属性: 指示该操作数作为变量/引用/地址时的编号(3者独立)

    // OPE_INT
    int integer;       // 固有属性: 该操作数作为整型常数的值

    // OPE_FLOAT
    float real;        // 固有属性: 该操作数作为浮点型常数的值

    // OPE_FUNC
    const char *name;  // 固有属性: 该操作数作为函数时的函数名

    // OPE_REF, OPE_REF_INFO
    Type *base_type;   // 综合属性: 引用型操作数对应的类型, 关系到偏移量的计算

    // OPE_REF_INFO
    Operand ref;       // 综合属性: 下标递归表达式使用, 结构体应该也能用
    Operand offset;    // 综合属性: 引用型操作数的偏移量

    // OPE_LABEL
    int label_ref_cnt; // Label 作为跳转目标的引用次数, 在删除 GOTO 或者翻转 BRANCH 后, 如果引用计数归零, 可以删除

    // 代码优化相关
    int liveness;      // 标记变量的活跃性, VAR和REF型变量一直活跃. 临时和地址初始不活跃
    int next_use;      // 标记下一条需要该操作数的指令编号
    pDagNode dep;       // 依赖结点
};

// 判定接口
bool is_always_live(Operand ope);
bool is_const(Operand ope);
bool is_tmp(Operand ope);

// 常规接口
Operand new_operand(Ope_Type type);
const char *print_operand(Operand ope);
Operand calc_const(IR_Type op, Operand left, Operand right);
Operand get_neg(Operand ope);
bool cmp_operand(Operand first, Operand second);

#endif //__OPERAND_H__
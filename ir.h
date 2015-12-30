//
// Created by whz on 15-11-20.
//

//
// 中间代码相关模块
//

#ifndef __IR_H__
#define __IR_H__

#include "lib.h"
#include <stdio.h>

typedef struct DagNode_ *pDagNode;
typedef struct Operand_ *Operand;
typedef struct Block *pBlock;

typedef struct {
    int liveness;
    int next_use;
} OptimizeInfo;

typedef struct {
    IR_Type type;
    union {
        struct {
            Operand rd;      // 目的操作数
            Operand rs;      // 第一源操作数
            Operand rt;      // 第二源操作数
        };
        Operand operand[NR_OPE];
    };
    int block;
    pDagNode depend;
    union {
        struct {
            OptimizeInfo rd_info;
            OptimizeInfo rs_info;
            OptimizeInfo rt_info;
        };
        OptimizeInfo info[NR_OPE];
    };
} IR;

//
// 中间代码模块对外接都口
//
int new_instr(IR_Type type, Operand rs, Operand rt, Operand rd);
void print_instr(FILE *stream);
IR_Type get_relop(const char *sym);
int replace_operand_global(Operand newbie, Operand old);
bool is_const(Operand ope);
Operand calc_const(IR_Type op, Operand left, Operand right);
int is_branch(IR *pIR);
bool can_jump(IR *pIR);

const char *ir_to_s(IR *);
#endif // __IR_H__

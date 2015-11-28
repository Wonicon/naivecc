//
// Created by whz on 15-11-24.
//

#ifndef __DAG_H__
#define __DAG_H__

#include "ir.h"

typedef struct DagNode_ *DagNode;

struct DagNode_ {
    // 非叶子结点
    IR_Type ir_type;    // 操作类型
    DagNode left;       // 左子节点
    DagNode right;      // 右子节点
    Operand embody;     // "代表"目的操作数
    int has_gen;        // 是否生成过指令

    // 叶子结点
    Ope_Type ope_type;  // 操作数类型
};

#endif // __DAG_H__

//
// Created by whz on 15-11-24.
//

#ifndef __DAG_H__
#define __DAG_H__

#include "ir.h"

typedef struct DagNode_ *pDagNode;

typedef enum {
    DAG_NA,
    DAG_OP,
    DAG_LEAF
} DagNodeType;

typedef struct DagNode_ {
    DagNodeType type;

    // 非叶子结点
    IR_Type op;          // 操作类型
    pDagNode left;       // 左子节点
    pDagNode right;      // 右子节点
    Operand embody;      // "代表"目的操作数
    int has_gen;         // 是否生成过指令

    // 叶子结点
    Operand initial_value;
    int ref_count;
} DagNode;

void init_dag();
pDagNode new_leaf(Operand ope);
pDagNode new_dagnode(IR_Type ir_type, pDagNode left, pDagNode right);
pDagNode query_dag_node(IR_Type ir_type, pDagNode left, pDagNode right);
void print_dag();
#endif // __DAG_H__

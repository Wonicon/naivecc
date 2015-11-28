//
// Created by whz on 15-11-24.
//

#ifndef __DAG_H__
#define __DAG_H__

#include "ir.h"

typedef struct DagNode_ *DagNode;

typedef enum {
    DAG_NA,
    DAG_OP,
    DAG_LEAF
} DagNodeType;

struct DagNode_ {
    DagNodeType type;

    struct {
        // 非叶子结点
        IR_Type ir_type;    // 操作类型
        DagNode left;       // 左子节点
        DagNode right;      // 右子节点
        Operand embody;     // "代表"目的操作数
        int has_gen;        // 是否生成过指令
    } op;

    // 叶子结点
    struct {
        Operand initial_value;
    } leaf;
};

void init_dag();
DagNode new_leaf(Operand ope);
DagNode new_dagnode(IR_Type ir_type, DagNode left, DagNode right);
DagNode query_dag_node(IR_Type ir_type, DagNode left, DagNode right);
void print_dag();
#endif // __DAG_H__

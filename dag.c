//
// Created by whz on 15-11-24.
//


//
// TODO 充满了内存泄漏
//

#include "dag.h"
#include <stdlib.h>
#include <assert.h>

#define MAX 2048
DagNode dag_buf[MAX];
int nr_dag_node = 0;

void init_dag()
{
    nr_dag_node = 0;
}

DagNode new_leaf(Operand ope)
{
    DagNode p = (DagNode)malloc(sizeof(*p));
    // TODO 解引用信息还原!
    if (ope->type == OPE_DEREF) {
    } else {
        p->leaf.initial_value = ope;
    }
    p->type = DAG_LEAF;
    dag_buf[nr_dag_node++] = p;
    return p;
}

DagNode new_dagnode(IR_Type ir_type, DagNode left, DagNode right)
{
    DagNode p = (DagNode)malloc(sizeof(*p));
    p->op.ir_type = ir_type;
    p->op.left = left;
    p->op.right = right;
    p->type = DAG_OP;
    dag_buf[nr_dag_node++] = p;
    return p;
}

int cmp_dag_node(DagNode first, DagNode second)
{
    assert(first);
    assert(second);
    if (first->type == second->type) {
        if (first->type == DAG_OP) {
            return first->op.ir_type == second->op.ir_type &&
                   cmp_dag_node(first->op.left, second->op.left) && cmp_dag_node(first->op.right, second->op.right);
        } else if (first->type== DAG_LEAF) {
            Operand left = first->leaf.initial_value;
            Operand right = second->leaf.initial_value;
            if (left->type == right->type) {
                switch (left->type) {
                    case OPE_INTEGER: return left->integer == right->integer;
                    case OPE_FLOAT: return left->real == right->real;
                    default: return left == right;
                }
            } else {
                return 0;
            }
        } else {
            return 0;
        }

    } else {
        return 0;
    }
}

//
// 查找 (op, left, right) 三元组, 没有的话新建该结点
//
DagNode query_dag_node(IR_Type ir_type, DagNode left, DagNode right)
{
    DagNode p = new_dagnode(ir_type, left, right);
    for (int i = nr_dag_node - 2; i >= 0; i--) {  // 回避新建的该结点
        if (cmp_dag_node(dag_buf[i], p)) {
            LOG("发现公共子表达式");
            free(p);
            nr_dag_node--;
            return dag_buf[i];
        }
    }
    return p;
}

//
// 测试打印
//
void print_operand(Operand ope, char *str);
static void print_dag_(DagNode dag, int level)
{
    if (dag == NULL) {
        return;
    } else {
        int indent = 2 * level;
        char str[16];
        if (dag->type == DAG_LEAF) {
            print_operand(dag->leaf.initial_value, str);
            printf("%*sleaf: %s\n", indent, "", str);
        } else if (dag->type == DAG_OP) {
            printf("%*snode: op %d\n", indent, "", dag->op.ir_type);
            print_dag_(dag->op.left, level + 1);
            print_dag_(dag->op.right, level + 1);
        } else {
            LOG("Unexpected dag");
            assert(0);
        }
    }
}
void print_dag()
{
    for (int i = nr_dag_node - 1; i >= 0; i--) {
        print_dag_(dag_buf[i], 0);
    }
}
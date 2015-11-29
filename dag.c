//
// Created by whz on 15-11-24.
//


//
// TODO 充满了内存泄漏
//

#include "dag.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MAX 2048
DagNode dag_buf[MAX];
int nr_dag_node = 0;

void init_dag()
{
    nr_dag_node = 0;
}

DagNode  new_leaf(Operand ope)
{
    DagNode p = (DagNode)malloc(sizeof(*p));
    memset(p, 0, sizeof(*p));
    p->type = DAG_LEAF;
    p->leaf.initial_value = ope;
    dag_buf[nr_dag_node++] = p;
    return p;
}

DagNode new_dagnode(IR_Type ir_type, DagNode left, DagNode right)
{
    DagNode p = (DagNode)malloc(sizeof(*p));
    assert(p != left && p != right);
    memset(p, 0, sizeof(*p));
    p->op.ir_type = ir_type;
    p->op.left = left;
    p->op.right = right;
    p->type = DAG_OP;
    dag_buf[nr_dag_node++] = p;
    return p;
}

int cmp_dag_node(DagNode first, DagNode second)
{
    if (first == NULL && second == NULL) {
        return true;
    } else if (first == NULL || second == NULL) {
        return false;
    }

    if (first->type != second->type) {
        return false;
    }

    if (first->type == DAG_OP) {
        if (first->op.ir_type != second->op.ir_type) {
            return false;
        }

        // 解引用不能被优化
        // 优化的结果会导致非代表元用代表元来赋值, 而地址值可能在别的变量中用于寻址赋值,
        // 所以代表元里存储的不一定是新的值.
        if (first->op.ir_type == IR_DEREF_R) {
            return false;
        }

        if (cmp_dag_node(first->op.left, second->op.left) && cmp_dag_node(first->op.right, second->op.right)) {
                return true;
        }

        // 交换律
        if (first->op.ir_type == IR_ADD || first->op.ir_type == IR_MUL) {
            return cmp_dag_node(first->op.left, second->op.right) && cmp_dag_node(first->op.right, second->op.left);
        }

        return false;
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
            return false;
        }
    }
    return false;
}

//
// 查找 (op, left, right) 三元组, 没有的话新建该结点
// 由于此处就在构造DAG结点了, 所以在这里进行模式匹配
//
void print_dag_(DagNode dag, int level);
void erase_identity(DagNode *out, IR_Type op, DagNode left, DagNode right);
int count = 0;
DagNode query_dag_node(IR_Type ir_type, DagNode left, DagNode right)
{
    count++;
    DagNode p = new_dagnode(ir_type, left, right);
    for (int i = nr_dag_node - 2; i >= 0; i--) {  // 回避新建的该结点
        if (cmp_dag_node(dag_buf[i], p)) {
            LOG("在第%d个DAG结点发现公共子表达式", i);
            free(p);
            nr_dag_node--;
            return dag_buf[i];
        }
    }

    //erase_identity(&p, ir_type, left, right);

    return p;
}

void erase_identity(DagNode *out, IR_Type op, DagNode left, DagNode right)
{
    // 模式匹配模板 单位元发现
#define INT_IDENTITY_CHECK(x, y) (x->type == OPE_INTEGER && x->integer == y)
#define FLOAT_IDENTITY_CHECK(x, y) (x->type == OPE_FLOAT && abs(x->real - y) < 1e-10)
#define STR(x) # x
#define FIND_IDENTITY(Left, Right, Identity)                                                                           \
    do {                                                                                                               \
        Operand init = Left->leaf.initial_value;                                                                       \
        if (Left->type == DAG_LEAF && is_const(init)                                                                   \
              && (INT_IDENTITY_CHECK(init, Identity) || FLOAT_IDENTITY_CHECK(init, Identity))) {                       \
            LOG("单位元发现: %s是%s", STR(Left), STR(Identity));                                                      \
            free(*out);                                                                                                \
            nr_dag_node--;                                                                                             \
            *out = Right;                                                                                              \
            return;\
        }                                                                                                              \
    } while (0)
    // 模式匹配 单位元发现
    // 利用falldown实现交换律
    switch (op) {
        case IR_ADD:
            FIND_IDENTITY(left, right, 0);
        case IR_SUB:
            FIND_IDENTITY(right, left, 0);
            break;
        case IR_MUL:
            FIND_IDENTITY(left, right, 1);
        case IR_DIV:
            FIND_IDENTITY(right, left, 1);
            break;
        default:
            break;
    }
#undef INT_IDENTITY_CHECK
#undef FLOAT_IDENTITY_CHECK
#undef FIND_IDENTITY
}

//
// 测试打印
//
void print_operand(Operand ope, char *str);
void print_dag_(DagNode dag, int level)
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
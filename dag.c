//
// Created by whz on 15-11-24.
//


//
// TODO 充满了内存泄漏
//

#include "dag.h"
#include "operand.h"
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

pDagNode new_leaf(Operand ope)
{
    pDagNode p = &dag_buf[nr_dag_node++];
    memset(p, 0, sizeof(*p));
    p->type = DAG_LEAF;
    p->leaf.initial_value = ope;
    return p;
}

pDagNode new_dagnode(IR_Type ir_type, pDagNode left, pDagNode right)
{
    pDagNode p = &dag_buf[nr_dag_node++];
    memset(p, 0, sizeof(*p));
    p->op.ir_type = ir_type;
    p->op.left = left;
    p->op.right = right;
    p->type = DAG_OP;
    return p;
}

int cmp_dag_node(pDagNode first, pDagNode second)
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
pDagNode erase_identity(IR_Type op, pDagNode left, pDagNode right);
pDagNode const_associativity(IR_Type op, pDagNode left, pDagNode right);
pDagNode query_dag_node(IR_Type ir_type, pDagNode left, pDagNode right)
{
    pDagNode p = new_dagnode(ir_type, left, right);
    for (int i = nr_dag_node - 2; i >= 0; i--) {  // 回避新建的该结点
        if (cmp_dag_node(&dag_buf[i], p)) {
            LOG("在第%d个DAG结点发现公共子表达式", i);
            return &dag_buf[i];
        }
    }

    pDagNode optimal;
    if ((optimal = const_associativity(p->op.ir_type, p->op.left, p->op.right))) {  // 结合律
        p = optimal;
    }

    if ((optimal = erase_identity(p->op.ir_type, p->op.left, p->op.right))) {  // 消除单位元
        p = optimal;
    }

    assert(p->type != DAG_OP || (IR_NOP <= p->op.ir_type && p->op.ir_type <= IR_WRITE));
    return p;
}

bool is_const_dag_node(pDagNode nd)
{
    return nd && nd->type == DAG_LEAF && is_const(nd->leaf.initial_value);
}

bool is_anti_op(IR_Type first, IR_Type second)
{
    return (first == IR_ADD && second == IR_SUB) ||
            (first == IR_SUB && second == IR_ADD) ||
            (first == IR_MUL && second == IR_DIV) ||
            (first == IR_DIV && second == IR_MUL);
}

bool is_same_op_level(IR_Type first, IR_Type second)
{
    switch (first) {
        case IR_ADD:
        case IR_SUB:
            return second == IR_ADD || second == IR_SUB;
        case IR_MUL:
        case IR_DIV:
            return second == IR_MUL || second == IR_DIV;
        default:
            return false;
    }
}

bool exchangable(IR_Type op)
{
    return op == IR_ADD || op == IR_MUL;
}

bool check_counter(IR_Type op, pDagNode left, pDagNode right) {
    if (op == IR_ADD && left->op.ir_type == op && right->op.ir_type == IR_SUB && cmp_dag_node(left->op.right, right->op.right)) {
        return true;
    } else if (op == IR_MUL && left->op.ir_type == op && right->op.ir_type == IR_DIV && cmp_dag_node(left->op.right, right->op.right)) {
        return true;
    } else {
        return false;
    }
}

// What the hell!
pDagNode const_associativity(IR_Type op, pDagNode left, pDagNode right)
{
    IR_Type top_op[4][4];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            top_op[i][j] = IR_NOP;
        }
    }
#define OFF(x) (x - IR_ADD)
    // 对于同级操作符+/-, *//, 可以无二义的按中序形式展开
    // 下表是在中序情况下, 将后两个操作数结合, 其操作符的变化情况
    // A op1 B op2 C = A op1 (B op3 C)
    // A op1 (B op2 C) = A op1 B op3 C
    //    op1          op2             op3
    top_op[OFF(IR_ADD)][OFF(IR_ADD)] = IR_ADD;
    top_op[OFF(IR_ADD)][OFF(IR_SUB)] = IR_SUB;
    top_op[OFF(IR_SUB)][OFF(IR_ADD)] = IR_SUB;
    top_op[OFF(IR_SUB)][OFF(IR_SUB)] = IR_ADD;

    top_op[OFF(IR_MUL)][OFF(IR_MUL)] = IR_MUL;
    top_op[OFF(IR_MUL)][OFF(IR_DIV)] = IR_DIV;
    top_op[OFF(IR_DIV)][OFF(IR_MUL)] = IR_DIV;
    top_op[OFF(IR_DIV)][OFF(IR_DIV)] = IR_MUL;
    // 噫, 搞了半天这表就是个逆运算取反......


    if (left == NULL || right == NULL || IR_ADD > op || op > IR_DIV) {
        return NULL;
    } else if (is_const_dag_node(left) && is_const_dag_node(right)) {
        LOG("左右皆为常量");
        Operand result = calc_const(op, left->leaf.initial_value, right->leaf.initial_value);
        return new_leaf(result);
    } else if (is_const_dag_node(left) && is_same_op_level(op, right->op.ir_type)) {
        if (!is_tmp(right->op.embody)) {
            LOG("不能折叠非临时变量");
            return NULL;
        }
        // 左边为常数, 右边为运算, 可应用结合律
        // 相当于后两个操作数结合的情况, 要考虑第二个操作符去掉括号后的变化
        IR_Type lop = op;  // 由于右边为运算, 所以左操作即本函数的参数op
        IR_Type rop = right->op.ir_type;
        Operand lope = left->leaf.initial_value;  // 可以确认左结点的DagNode可以拿出Operand
        if (is_const_dag_node(right->op.left)) {
            LOG("C1 op1 (C2 op1 V) -> C1 op1 C2 op3 V -> (C1 op1 C2) op3 V");
            Operand rlope = right->op.left->leaf.initial_value;  // 可以确认右结点的左结点可以拿出Operand
            Operand const_rst = calc_const(lop, lope, rlope);  // 左和右左中序相邻, 可以直接计算
            pDagNode leaf = new_leaf(const_rst);
            return new_dagnode(top_op[OFF(lop)][OFF(rop)], leaf, right->op.right);  // 第二个操作符为取消结合后的, 同样适用上表.
        } else if (is_const_dag_node(right->op.right)) {
            LOG("C1 op1 (V op2 C2) -> C1 op1 V op3 C2 -> (C1 op3 C2) op1 V");
            Operand rrope = right->op.right->leaf.initial_value;  // 可以确认右结点的右结点可以拿出Operand
            Operand const_rst = calc_const(top_op[OFF(lop)][OFF(rop)], lope, rrope);  // 获得取消结合后的操作符, 并且移动到前面
            pDagNode leaf = new_leaf(const_rst);
            return new_dagnode(lop, leaf, right->op.left);  // 操作符不需要处理
        } else {
            LOG("左边是常量, 右边没有常量, 无法结合");
            return NULL;
        }
    } else if (is_const_dag_node(right) && is_same_op_level(op, left->op.ir_type)) {
        if (!is_tmp(left->op.embody)) {
            LOG("不能折叠非临时变量");
            return NULL;
        }
        // 左边为运算, 右边为常数, 可应用结合律
        // 此模式相当于中序, 所以有一种情况只需要移位不需要查表
        // 但是另外一种情况要考虑结合后操作符的变化
        IR_Type rop = op;
        IR_Type lop = left->op.ir_type;
        Operand rope = right->leaf.initial_value;  // 可以确认右结点的DagNode可以拿出Operand

        if (is_const_dag_node(left->op.left)) {
            LOG("(C1 op1 V) op2 C2 -> (C1 op2 C2) op1 V");
            Operand llope = left->op.left->leaf.initial_value;
            Operand const_rst = calc_const(rop, llope, rope);
            pDagNode leaf = new_leaf(const_rst);
            return new_dagnode(lop, leaf, left->op.right); 
        } else if (is_const_dag_node(left->op.right)) {
            LOG("(V op1 C1) op2 C2 -> V op1 (C1 op3 C2)");
            Operand lrope = left->op.right->leaf.initial_value;
            Operand const_rst = calc_const(top_op[OFF(lop)][OFF(rop)], lrope, rope);
            pDagNode leaf = new_leaf(const_rst);
            return new_dagnode(lop, left->op.left, leaf);
        } else {
            LOG("右边是常量, 左边没有常量, 无法结合");
            return NULL;
        }
    } else {
        LOG("左右都不是常量");
        if (exchangable(op) && (check_counter(op, left, right) || check_counter(op, right, left))) {
            LOG("(A op B) op (C anti B) -> A op C");
            return new_dagnode(op, left->op.left, right->op.left);
        } else if (is_same_op_level(op, IR_ADD) && is_anti_op(op, left->op.ir_type) && cmp_dag_node(right, left->op.right)) {
            LOG("(A op B) anti B -> A");
            return left->op.left;
        } else if (is_same_op_level(op, IR_ADD) && is_anti_op(op, right->op.ir_type) && cmp_dag_node(left, right->op.right)) {
            LOG("A op (B anti A) -> B");
            return right->op.left;
        } else if (is_const_dag_node(left->op.right) && is_const_dag_node(right->op.right) &&
                is_same_op_level(op, left->op.ir_type) && is_same_op_level(op, right->op.ir_type)) {
            LOG("(A op1 C1) op2 (B op3 C2) -> (A op2 B) op1 C3");
            pDagNode v = new_dagnode(op, left->op.left, right->op.left);
            v->op.embody = new_operand(OPE_TEMP);
            Operand c = calc_const(top_op[OFF(left->op.ir_type)][OFF(right->op.ir_type)],
                                   left->op.right->leaf.initial_value, right->op.right->leaf.initial_value);

            return new_dagnode(left->op.ir_type, v, new_leaf(c));
        }

        if (left->op.ir_type == right->op.ir_type &&
                is_same_op_level(left->op.ir_type, IR_MUL) &&
                is_same_op_level(op, IR_ADD)) {
            LOG("分配率优化?");
            IR_Type high_level_op = left->op.ir_type;
            bool mul = high_level_op == IR_MUL;
            pDagNode ll = left->op.left;
            pDagNode lr = left->op.right;
            pDagNode rl = right->op.left;
            pDagNode rr = right->op.right;

            if (cmp_dag_node(lr, rr)) {
                pDagNode v = query_dag_node(op, ll, rl);
                v->op.embody = new_operand(OPE_TEMP);
                return new_dagnode(high_level_op, v, lr);
            } else if (mul && cmp_dag_node(ll, rl)) {
                pDagNode v = query_dag_node(op, lr, rr);
                v->op.embody = new_operand(OPE_TEMP);
                return new_dagnode(high_level_op, v, ll);
            } else if (mul && cmp_dag_node(lr, rl)) {
                pDagNode v = query_dag_node(op, ll, rr);
                v->op.embody = new_operand(OPE_TEMP);
                return new_dagnode(high_level_op, v, lr);
            } else if (mul && cmp_dag_node(ll, rr)) {
                pDagNode v = query_dag_node(op, lr, rl);
                v->op.embody = new_operand(OPE_TEMP);
                return new_dagnode(high_level_op, v, ll);
            } else {
                LOG("无法结合");
            }
        }
    }

    return NULL;
#undef OFF
}

pDagNode erase_identity(IR_Type op, pDagNode left, pDagNode right)
{
    // 模式匹配模板 单位元发现
#define INT_IDENTITY_CHECK(x, y) (x->type == OPE_INTEGER && x->integer == y)
#define FLOAT_IDENTITY_CHECK(x, y) (x->type == OPE_FLOAT && abs(x->real - y) < 1e-10)
#define STR(x) # x
#define FIND_IDENTITY(Left, Right, Identity)                                                                           \
    do {                                                                                                               \
        Operand init = Left->leaf.initial_value;                                                                       \
        if (Left->type == DAG_LEAF && is_const(init)                                                                   \
                && (INT_IDENTITY_CHECK(init, Identity) || FLOAT_IDENTITY_CHECK(init, Identity))) {                     \
            LOG("单位元发现: %s是%s %s is %p", STR(Left), STR(Identity), STR(Right), Right);                          \
            return Right;                                                                                               \
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
    return NULL;
#undef INT_IDENTITY_CHECK
#undef FLOAT_IDENTITY_CHECK
#undef FIND_IDENTITY
}

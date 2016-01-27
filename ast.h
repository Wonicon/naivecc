//
// Created by whz on 15-11-18.
//

#ifndef __AST_H__
#define __AST_H__

#include "node.h"

//
// 该模块主要用于简化语法分析树, 以方便遍历函数的实现, 以及提供显式的上下文信息.
// 从而方便访问者模式的实现.
// 之所以直接在基于 node_t 构造的语法分析树上简化是因为 node_t 已经保留了充分的信息和足够可用的(但是麻烦且不直观的)结构,
// 如果从头构造新结构体则, 信息的拷贝和结构的重塑相当的麻烦且具有风险.
// 新的产生式用枚举类型标注, 格式是 HEAD_is_BODY[_BODY].
// 新的产生式与旧的保持语义上的一致, 但是不过分追求定义的递归.
// 能够用重复的语法结构(各种List)都改用 sibling 做链表链接.
//


enum BinaryOpType {
    BI_ADD,
    BI_SUB,
    BI_MUL,
    BI_DIV,
    BI_AND,
    BI_OR,
    BI_EQ,
    BI_LT,
    BI_LE,
    BI_GT,
    BI_GE,
    BI_NE
};

Node create_tree(enum ProductionTag tag, int lineno, ...);
#endif //__AST_H__

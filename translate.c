//
// Created by whz on 15-11-18.
//

#include "translate.h"
#include "ir.h"
#include "node.h"
#include "cmm_symtab.h"
#include <assert.h>
#include <stdlib.h>

int translate_exp_is_const(const Node nd);

//
// 用switch-case实现对不同类型(tag)node的多分派
// 也可以用函数指针表来实现, 不过函数指针表对枚举值的依赖太强
// 所有的翻译函数返回生成指令的编号, 如果没有合适的翻译函数或出现异常, 则会返回 -1,
// 一般情况下指令是流式生成的, 也就是不需要回退.
//
int translate_dispatcher(const Node node) {
    switch (node->tag) {
        case EXP_is_INT:
        case EXP_is_FLOAT:
            return translate_exp_is_const(node);
        default: return -1;
    }
}

//
// 翻译表达式: 字面常量
//
int translate_exp_is_const(const Node nd) {
    assert(nd->tag == EXP_is_INT || nd->tag == EXP_is_FLOAT);

    Operand new_ope;
    switch (nd->child->tag) {
        case TERM_INT:
            new_ope = new_operand(OPE_INTEGER);
            new_ope->var.integer = nd->child->val.i;
            break;
        case TERM_FLOAT:
            new_ope = new_operand(OPE_FLOAT);
            new_ope->var.real = nd->child->val.f;
            break;
        default:
            return -1;
    }

    if (nd->dst != NULL) {
        return new_instr(IR_ASSIGN, new_ope, NULL, nd->dst);
    } else {
        free(new_ope);
        return -1;
    }
}

// 测试用函数
Operand p;
int indent = 0;
void traverse_(const Node node) {
    if (node == NULL) return;
    node->dst = p;
    printf("%*s", indent, "");
    puts(get_token_name(node->type));
    indent += 2;
    Node child = node->child;
    translate_dispatcher(node);
    while (child != NULL) {
        traverse_(child);
        child = child->sibling;
    }
    indent -= 2;
}

extern node_t *ast_tree;
void test_translate() {
    p = new_operand(OPE_VARIABLE);
    traverse_(ast_tree);
    print_instr(stdout);
}
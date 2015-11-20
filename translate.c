//
// Created by whz on 15-11-18.
//

#include "translate.h"
#include "ir.h"
#include "node.h"
#include "cmm_symtab.h"
#include <assert.h>
#include <stdlib.h>

#define TASK_2

#define FAIL_TO_GEN -1
#define NO_NEED_TO_GEN -2

enum TranslateState translate_state = FINE;

int translate_exp_is_const(Node nd);

int translate_exp_is_id(Node exp);

//
// 用switch-case实现对不同类型(tag)node的分派
// 也可以用函数指针表来实现, 不过函数指针表对枚举值的依赖太强
// 所有的翻译函数返回生成指令的编号, 如果没有合适的翻译函数或出现异常, 则会返回 -1.
// 如果根据策略主动不生成指令, 返回 -2.
// 一般情况下指令是流式生成的, 也就是不需要回退.
//
int translate_dispatcher(Node node) {
    switch (node->tag) {
        case EXP_is_INT:
        case EXP_is_FLOAT:
            return translate_exp_is_const(node);
        case EXP_is_ID:
            return translate_exp_is_id(node);
        default: return FAIL_TO_GEN;
    }
}

//
// [优化策略]
//
// 值类型可以直接返回而不需要进行赋值操作. 常数自不必说, 变量的地址都会在声明语句中分配.
// 但是由于上层不知道表达式的具体产生式, 所以依然会生成目标地址作为继承属性传递给子表达式.
// 但是这个目标地址也是一个操作数, 对于值类型情况, 只需要替换掉就行了, 不需要生成新的赋值指令
// 不过这要求上层在生成指令时, 使用的操作数应该从子结点里获取.
//

//
// 翻译表达式: 变量名
// 变量名的翻译需要注意的是数组类型或结构体类型变量返回的是地址而不是值
// 数组的地址是通过在声明语句中翻译 DEC 获得的, 对应的 Operand 为符号的 address 域所引用
// 函数名和域名是在产生式中直接获取的, 不需要在这里翻译代码, 否则属于非法情况.
//
int translate_exp_is_id(Node exp) {
    Node id = exp->child;
    assert(id->type == YY_ID);

    sym_ent_t *sym = query(id->val.s, 0);

    if (sym == NULL) {
        return FAIL_TO_GEN;
    }

    if (sym->type->class == CMM_STRUCT) {
        fprintf(stderr, "实验任务二: 不支持结构体类型");
        translate_state = UNSUPPORT;
    }

    // 替换不必要的目标地址
    if (exp->dst != NULL) {
        free(exp->dst);
    }
    exp->dst = sym->address;
    return NO_NEED_TO_GEN;
}

//
// 翻译表达式: 字面常量
//
int translate_exp_is_const(Node nd) {
    assert(nd->tag == EXP_is_INT || nd->tag == EXP_is_FLOAT);

    Operand const_ope;
    switch (nd->child->tag) {
        case TERM_INT:
            const_ope = new_operand(OPE_INTEGER);
            const_ope->var.integer = nd->child->val.i;
            break;
        case TERM_FLOAT:
            const_ope = new_operand(OPE_FLOAT);
            const_ope->var.real = nd->child->val.f;
            break;
        default:
            return FAIL_TO_GEN;
    }

    // 替换不必要的目标地址
    if (nd->dst != NULL) {
        free(nd->dst);
    }
    nd->dst = const_ope;
    return NO_NEED_TO_GEN;
}

// 测试用函数
Operand p;
int indent = 0;
void traverse_(Node node) {
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
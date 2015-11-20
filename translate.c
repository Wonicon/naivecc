//
// Created by whz on 15-11-18.
//

#include "translate.h"
#include "ir.h"
#include "cmm_symtab.h"
#include "node.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define TASK_2

enum TranslateState translate_state = FINE;

// TODO 每个 Compst 在跳转指令生成前可以计算所有常量
// TODO 左值解引用只能用在赋值操作中, 不能像 & 和右解引用那样随意嵌入
static void free_ope(Operand *ptr) {
    if (*ptr != NULL) {
        free(*ptr);
    }
    *ptr = NULL;
}

struct {
    enum IR_Type relop;
    const char *str;
    enum IR_Type anti;
} relop_dict[] = {
    { IR_BEQ, "==", IR_BNE },
    { IR_BLT, "<" , IR_BGE },
    { IR_BLE, "<=", IR_BGT },
    { IR_BGT, ">" , IR_BLE },
    { IR_BGE, ">=", IR_BLT },
    { IR_BNE, "!=", IR_BEQ }
};

#define LENGTH(x) (sizeof(x) / sizeof(*x))

int translate_exp_is_const(Node nd);

int translate_exp_is_id(Node exp);

int translate_exp_is_assign(Node assign_exp);

int translate_def_is_spec_dec(Node def);

int translate_dec_is_vardec(Node dec);

int translate_stmt_is_exp(Node stmt);

int translate_compst(Node compst);

int translate_func_head(Node func);

int translate_extdef_func(Node extdef);

int translate_ast(Node ast);

int translate_stmt_is_compst(Node stmt);

int translate_binary_operation(Node exp);

int translate_unary_operation(Node exp);

int translate_exp_is_exp(Node exp);

void intime_deref(Node exp);

int translate_exp_is_exp_idx(Node exp);

int translate_cond(Node exp);

int translate_cond_and(Node exp);

int translate_cond_or(Node exp);

int translate_cond_relop(Node exp);

int translate_ccond_not(Node exp);

int translate_cond_exp(Node exp);

int translate_return(Node exp);

//
// 用switch-case实现对不同类型(tag)node的分派
// 也可以用函数指针表来实现, 不过函数指针表对枚举值的依赖太强
// 所有的翻译函数返回生成指令的编号, 如果没有合适的翻译函数或出现异常, 则会返回 -1.
// 如果根据策略主动不生成指令, 返回 -2.
// 一般情况下指令是流式生成的, 也就是不需要回退.
//
int translate_dispatcher(Node node) {
    if (node == NULL) {
        return NO_NEED_TO_GEN;
    }
    switch (node->tag) {
        case PROG_is_EXTDEF:
            return translate_ast(node);
        case EXTDEF_is_SPEC_FUNC_COMPST:
            return translate_extdef_func(node);
        case FUNC_is_ID_VAR:
            return translate_func_head(node);
        case COMPST_is_DEF_STMT:
            return translate_compst(node);
        case DEC_is_VARDEC:
            return translate_dec_is_vardec(node);
        case DEF_is_SPEC_DEC:
            return translate_def_is_spec_dec(node);
        case STMT_is_COMPST:
            return translate_stmt_is_compst(node);
        case STMT_is_EXP:
            return translate_stmt_is_exp(node);
        case STMT_is_RETURN:
            return translate_return(node);
        case EXP_is_EXP:
            return translate_exp_is_exp(node);
        case EXP_is_BINARY:
            return translate_binary_operation(node);
        case EXP_is_UNARY:
            return translate_unary_operation(node);
        case EXP_is_INT:
        case EXP_is_FLOAT:
            return translate_exp_is_const(node);
        case EXP_is_ID:
            return translate_exp_is_id(node);
        case EXP_is_ASSIGN:
            return translate_exp_is_assign(node);
        case EXP_is_EXP_IDX:
            return translate_exp_is_exp_idx(node);
        case EXP_is_AND:
        case EXP_is_OR:
        case EXP_is_NOT:
        case EXP_is_RELOP: {
            node->label_true = new_operand(OPE_LABEL);
            node->label_false = new_operand(OPE_LABEL);

            if (node->dst != NULL) {
                Operand value_false = new_operand(OPE_INTEGER);
                value_false->var.integer = 0;
                new_instr(IR_ASSIGN, value_false, NULL, node->dst);
            }

            translate_cond(node);

            new_instr(IR_LABEL, node->label_true, NULL, NULL);

            if (node->dst != NULL) {
                Operand value_true = new_operand(OPE_INTEGER);
                value_true->var.integer = 1;
                new_instr(IR_ASSIGN, value_true, NULL, node->dst);
            }

            new_instr(IR_LABEL, node->label_false, NULL, NULL);

            return MULTI_INSTR;
        }
        default:
            return FAIL_TO_GEN;
    }
}

//
// 翻译返回语句
//
int translate_return(Node exp) {
    Node sub_exp = exp->child;
    sub_exp->dst = new_operand(OPE_TEMP);
    return new_instr(IR_RET, sub_exp->dst, NULL, NULL);
}

int translate_cond(Node exp) {
    switch (exp->tag) {
        case EXP_is_AND: return translate_cond_and(exp);
        case EXP_is_OR: return translate_cond_or(exp);
        case EXP_is_RELOP: return translate_cond_relop(exp);
        case EXP_is_NOT: return translate_ccond_not(exp);
        default: return translate_cond_exp(exp);
    }
}

int translate_cond_exp(Node exp) {
    exp->dst = new_operand(OPE_TEMP);
    translate_dispatcher(exp);
    Operand const_zero = new_operand(OPE_INTEGER);
    new_instr(IR_BNE, exp->dst, const_zero, exp->label_true);
    new_instr(IR_JMP, exp->label_false, NULL, NULL);
    return MULTI_INSTR;
}

//
// 在条件判断框架下翻译 NOT
//
int translate_ccond_not(Node exp) {
    Node sub_exp = exp->child;
    sub_exp->label_true = exp->label_false;
    sub_exp->label_false = exp->label_true;
    return translate_cond(sub_exp);
}

//
// 在条件判断框架下翻译 RELOP
// TODO 优化重点!
//
int translate_cond_relop(Node exp) {
    Node left = exp->child;
    Node right = left->sibling;

    left->dst = new_operand(OPE_TEMP);
    right->dst = new_operand(OPE_TEMP);

    // 获取结果值
    translate_dispatcher(left);
    translate_dispatcher(right);

    const char *op = exp->val.operator;
    enum IR_Type relop = IR_BEQ;
    for (int i = 0; i < LENGTH(relop_dict); ++i) {
        if (!strcmp(relop_dict[i].str, op)) {
            relop = relop_dict[i].relop;
        }
    }

    new_instr(relop, left->dst, right->dst, exp->label_true);

    new_instr(IR_JMP, exp->label_false, NULL, NULL);

    return MULTI_INSTR;
}

//
// 翻译 与 表达式
//
int translate_cond_and(Node exp) {
    Node left = exp->child;
    Node right = left->sibling;
    left->label_true = new_operand(OPE_LABEL);
    left->label_false = exp->label_false;
    right->label_true = exp->label_true;
    right->label_false = exp->label_false;

    // 这里产生了 left 相关的代码
    translate_cond(left);

    // 为真 非短路
    new_instr(IR_LABEL, left->label_true, NULL, NULL);

    // 继续执行 right 的代码
    return translate_cond(right);
}

//
// 翻译 或 表达式
//
int translate_cond_or(Node exp) {
    Node left = exp->child;
    Node right = left->sibling;
    left->label_true = exp->label_true;
    left->label_false = new_operand(OPE_LABEL);
    right->label_true = exp->label_true;
    right->label_false = exp->label_false;

    // 这里产生了 left 相关的代码
    translate_cond(left);

    // 为假 非短路
    new_instr(IR_LABEL, left->label_false, NULL, NULL);

    // 继续执行 right 的代码
    return translate_cond(right);
}

//
// 翻译下标表达式
//
int translate_exp_is_exp_idx(Node exp) {
    Node base = exp->child;
    Node idx = base->sibling;

    base->dst = new_operand(OPE_V_ADDR);
    idx->dst = new_operand(OPE_TEMP);

    translate_dispatcher(base);
    translate_dispatcher(idx);

    // 现在我们已经准备好了一个基地址和偏移量, 以及"当前"数组的类型
    // 为了进一步计算偏移量, 我们需要访问数组的基类型, 获得基类型的大小, 用当前下标去计算
    // 新的偏移量, 如果综合来的偏移量和下标有一个为非常量, 则要生成指令并转移操作数

    Operand offset = idx->dst;
    int size = base->dst->array_base->base->type_size;
    if (offset->type == OPE_INTEGER) {
        offset->var.integer = offset->var.integer * size;
    } else {
        Operand p = new_operand(OPE_TEMP);
        Operand array_size = new_operand(OPE_INTEGER);
        array_size->var.integer = size;
        new_instr(IR_MUL, offset, array_size, p);
        offset = p;  // 转移本层偏移量
    }

    if (base->dst->offset->type == OPE_INTEGER && offset->type == OPE_INTEGER) {
        offset->var.integer = offset->var.integer + base->dst->offset->var.integer;
    } else {
        Operand p = new_operand(OPE_TEMP);
        new_instr(IR_ADD, offset, base->dst->offset, p);
        offset = p;  // 再转移本层偏移量
    }

    // 现在我们就有了完整的偏移量
    if (exp->dst->type == OPE_V_ADDR) {
        // 说明在下标翻译过程中
        exp->dst->var.index = base->dst->var.index;          // 保证基地址一致
        exp->dst->array_base = base->dst->array_base->base;  // 数组降低一维
        exp->dst->offset = offset;                           // 转移经过一系列计算的偏移量
        return MULTI_INSTR;
    }

    // 进入这里说明是最后一个阶段, 按道理要进行解引用
    // 但是由于任何指令都可以内嵌解引用, 所以为了效率
    // 这里直接算一个地址就往回送 (如果老老实实解引用
    // 则还要多一条指令)
    // 由于第一个元素直接用变量名访问在虚拟机里也是允许的,
    // 所以总偏移为0时改成var送回去, 毕竟编号是一致的,
    // 否则, 要用一个临时变量存储地址, 生成加法指令, 然后改成约定的待转换类型(ADD)传回去.
    if (exp->dst->type == OPE_TEMP) {
        free(exp->dst);
    }

    if (offset->type == OPE_INTEGER && offset->var.integer == 0) {
        // 伪造变量操作数
        exp->dst = new_operand(OPE_NOT_USED);
        exp->dst->type = OPE_VAR;
        exp->dst->var.index = base->dst->var.index;
    } else {
        // 要生成加法指令
        exp->dst = new_operand(OPE_ADDR);
        new_instr(IR_ADD, base->dst, offset, exp->dst);
    }
    return 0;
}

//
// 翻译括号表达式
//
int translate_exp_is_exp(Node exp) {
    // 需要继承!
    exp->child->dst = exp->dst;
    translate_dispatcher(exp->child);
    // 还需要综合!
    exp->dst = exp->child->dst;
    return MULTI_INSTR;
}

//
// 翻译一元运算: 只有取负
//
int translate_unary_operation(Node exp) {
    Node rexp = exp->child;
    rexp->dst = new_operand(OPE_TEMP);

    // 常量计算
    if (translate_dispatcher(rexp) < 0) {
        Operand const_ope = new_operand(OPE_NOT_USED);
        if (rexp->dst->type == OPE_INTEGER) {
            const_ope->type = OPE_INTEGER;
            const_ope->var.integer = -rexp->dst->var.integer;
            free_ope(&exp->dst);
            exp->dst = const_ope;
            return NO_NEED_TO_GEN;
        } else if (rexp->dst->type == OPE_FLOAT) {
            const_ope->type = OPE_FLOAT;
            const_ope->var.real = -rexp->dst->var.real;
            free_ope(&exp->dst);
            exp->dst = const_ope;
            return NO_NEED_TO_GEN;
        } else {
            // 变量情况
            free(const_ope);
        }
    }

    // 无脑上 0
    Operand p = new_operand(OPE_INTEGER);
    p->var.integer = 0;
    return new_instr(IR_SUB, p, rexp->dst, exp->dst);
}

#define CALC(op, rs, rt, rd, type) do {\
    switch (op) {\
        case '+': rd->var.type = rs->var.type + rt->var.type; break;\
        case '-': rd->var.type = rs->var.type - rt->var.type; break;\
        case '*': rd->var.type = rs->var.type * rt->var.type; break;\
        case '/': rd->var.type = rs->var.type / rt->var.type; break;\
    }\
    free_ope(&exp->dst);\
    exp->dst = rd;\
    return NO_NEED_TO_GEN;\
} while (0)

int translate_binary_operation(Node exp) {
    // 没有目标地址, 不需要翻译
    if (exp->dst == NULL) {
        return NO_NEED_TO_GEN;
    }

    Node lexp = exp->child;
    Node rexp = lexp->sibling;
    lexp->dst = new_operand(OPE_TEMP);
    translate_dispatcher(lexp);
    rexp->dst = new_operand(OPE_TEMP);
    translate_dispatcher(rexp);
    intime_deref(lexp);
    intime_deref(rexp);

    // TODO 检查变量是否为常量

    // 常量计算

    Operand lope = lexp->dst;
    Operand rope = rexp->dst;
    assert(lope && rope);


    if (lope->type == rope->type) {
        Operand const_ope = new_operand(OPE_NOT_USED);
        switch (lope->type) {
            case OPE_INTEGER:
                const_ope->type = OPE_INTEGER;
                CALC(exp->val.operator[0], lope, rope, const_ope, integer);
            case OPE_FLOAT:
                const_ope->type = OPE_FLOAT;
                CALC(exp->val.operator[0], lope, rope, const_ope, real);
            default: free(const_ope);
        }
    }

    switch (exp->val.operator[0]) {
        case '+': return new_instr(IR_ADD, lope, rope, exp->dst);
        case '-': return new_instr(IR_SUB, lope, rope, exp->dst);
        case '*': return new_instr(IR_DIV, lope, rope, exp->dst);
        case '/': return new_instr(IR_MUL, lope, rope, exp->dst);
        default: assert(0);
    }
}
#undef CALC

int translate_ast(Node ast) {
    Node extdef = ast->child;
    while (extdef != NULL) {
        translate_dispatcher(extdef);
        extdef = extdef->sibling;
    }
    return MULTI_INSTR;
}

int translate_extdef_func(Node extdef) {
    Node spec = extdef->child;
    Node func = spec->sibling;
    translate_dispatcher(func);
    Node compst = func->sibling;
    translate_dispatcher(compst);
    return MULTI_INSTR;
}

//
// 翻译函数: 主要是生成参数声明指令 PARAM
//
int translate_func_head(Node func) {
    Node funcname = func->child;
    Node param = funcname->sibling;

    // 声明函数头
    Operand rs = new_operand(OPE_FUNC);
    rs->var.funcname = funcname->val.s;
    new_instr(IR_FUNC, rs, NULL, NULL);

    // 声明变量
    while (param != NULL) {
        Node spec = param->child;
        // 无脑找变量名
        Node id = spec->sibling->child;
        while (id->type != YY_ID) {
            id = id->child;
        }
        sym_ent_t *sym = query(id->val.s, 0);
        sym->address = new_operand(OPE_VAR);
        // 实际的大小是在调用者那边说明
        new_instr(IR_PRARM, sym->address, NULL, NULL);
        param = param->sibling;
    }
    return MULTI_INSTR;
}

//
// 翻译复合语句: 纯粹的遍历框架
//
int translate_compst(Node compst) {
    Node child = compst->child;
    while (child != NULL) {
        translate_dispatcher(child);
        child = child->sibling;
    }
    return MULTI_INSTR;
}

//
// 翻译复合语句: 为了 dispatcher 的和谐统一......
//
int translate_stmt_is_compst(Node stmt) {
    return translate_dispatcher(stmt->child);
}

//
// 翻译表达式: 如果没有赋值之类的, 这条基本不需要生成指令了
//
int translate_stmt_is_exp(Node stmt) {
    translate_dispatcher(stmt->child);
    return MULTI_INSTR;
}

//
// 翻译定义: 找 ID
// dec 可以简单实现, 只要找数组定义就行了
//
int translate_dec_is_vardec(Node dec) {
    Node vardec = dec->child;
    Node iterator = vardec->child;
    if (iterator->type == YY_ID) {  // 普通变量声明, 分配空间就好了.
        sym_ent_t *sym = query(iterator->val.s, 0);
        assert(sym->type->type_size == 4);
        sym->address = new_operand(OPE_VAR);
        return NO_NEED_TO_GEN;
    }

    while (iterator->type != YY_ID) {
        iterator = iterator->child;
    }

    sym_ent_t *sym = query(iterator->val.s, 0);
    sym->address = new_operand(OPE_VAR);
    Operand size = new_operand(OPE_INTEGER);
    size->var.integer = sym->type->type_size;
    return new_instr(IR_DEC, sym->address, size, NULL);
}

//
// 翻译定义: 主要是用来遍历 declist 的
//
int translate_def_is_spec_dec(Node def) {
    Node spec = def->child;
    Node dec = spec->sibling;
    while (dec != NULL) {
        translate_dispatcher(dec);
        dec = dec->sibling;
    }
    return MULTI_INSTR;
}

//
// 翻译表达式: 赋值语句
// 对于赋值语句, 如果左边的表达式不是值类型(数组和结构体0偏移的域也可以直接用值类型)
// 那么就是数组或者结构体计算出来的偏移地址, 这时候是需要进行解引用操作
//
int translate_exp_is_assign(Node assign_exp) {
    assert(assign_exp && assign_exp->tag == EXP_is_ASSIGN);

    Node lexp = assign_exp->child;
    Node rexp = lexp->sibling;

    // 左值有两种情况:
    //   1.变量
    //   2.地址
    // 其中变量由于是常量, 所以会释放提供的目标操作数并取而代之
    // 地址运算应该是完全由下标表达式和成员访问表达式接管的,
    // 常规的表达式(注意与指令生成的做区分)不应该遇到地址操作数,
    // 如果遇到了, 可以即刻解引用. 由于地址的这种特性, 它也适合
    // 直接取代直接提供的目标操作数, 直接返回.
    lexp->dst = new_operand(OPE_TEMP);
    translate_dispatcher(lexp);
    rexp->dst = new_operand(OPE_TEMP);
    translate_dispatcher(rexp);

    intime_deref(lexp);
    intime_deref(rexp);  // 如果 rexp 直接是 array[...] 则会直接返回地址

    // TODO 更准确地判断赋值左右的等价性
    // TODO 这里可能会发生访问违例
    if (lexp->dst->type == rexp->dst->type && lexp->dst->var.index == rexp->dst->var.index) {
        free_ope(&lexp->dst);
        free_ope(&rexp->dst);
        return NO_NEED_TO_GEN;
    }

    return new_instr(IR_ASSIGN, rexp->dst, NULL, lexp->dst);
}

// 即时解引用, 如果算出结果是地址, 那么可以直接在指令中解引用
void intime_deref(Node exp) {
    if (exp->dst->type == OPE_ADDR) {
        Operand p = new_operand(OPE_DEREF);
        p->var.index = exp->dst->var.index;
        exp->dst = p;
        // 这边不需要考虑考虑 free
        // 因为需要替换的操作数在 translate 过程中已经替换过了
        // 这里只是为了能在指令里解引用而做了区分.
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

#ifdef TASK_2
    if (sym->type->class == CMM_STRUCT) {
        fprintf(stderr, "实验任务二: 不支持结构体类型");
        translate_state = UNSUPPORT;
    }
#endif

    // 上层希望存储到一个地址变量, 说明现在在寻址模式
    if (exp->dst && exp->dst->type == OPE_V_ADDR) {
        exp->dst->var.index = sym->address->var.index;  // 虽然类型不同, 但是编号保持一致
        exp->dst->offset = new_operand(OPE_INTEGER);    // 偏移量将来可能变成临时变量
        exp->dst->offset->var.integer = 0;              // 当前偏移量为常数 0
        exp->dst->array_base = sym->type;               // 数组类型, 在上层使用 base 获得信息
        return NO_NEED_TO_GEN;
    }

    // 替换不必要的目标地址
    // 如果赋值语句结点重复进入, 则被 free 掉的不是无用的 temp,
    // 而可能是到处引用的变量!
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
    // 如果赋值语句结点重复进入, 则被 free 掉的不是无用的 temp,
    // 而可能是到处引用的变量!
    if (nd->dst != NULL) {
        free(nd->dst);
    }
    nd->dst = const_ope;
    return NO_NEED_TO_GEN;
}

// 测试用函数
extern node_t *ast_tree;
void test_translate() {
    translate_ast(ast_tree);
    print_instr(stdout);
}
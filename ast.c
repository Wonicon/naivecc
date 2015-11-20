//
// Created by whz on 15-11-18.
//

#include "ast.h"
#include "node.h"
#include <assert.h>
#include <stdlib.h>

//
// 语法分析树结构体的转换构造函数
//
static Node trans_node(const Node nd, enum ProductionTag tag) {
    Node p = (Node)malloc(sizeof(node_t));
    *p = *nd;
    p->tag = tag;
    p->child = NULL;
    p->sibling = NULL;
    return p;
}

void print_production_tag(const Node nd) {
    switch (nd->tag) {
        case STMT_is_EXP: puts("stmt -> exp;"); break;
        case STMT_is_IF: puts("stmt -> if ( exp ) stmt"); break;
        case STMT_is_IF_ELSE: puts("stmt -> if ( exp ) stmt else stmt"); break;
        case STMT_is_WHILE: puts("stmt -> while ( exp ) stmt else stmt"); break;
        case STMT_is_COMPST: puts("stmt -> { stmt... }"); break;
        case STMT_is_RETURN: puts("stmt -> return exp;"); break;
        default: ;
    }
}

Node simplify_func_call(const Node exp);
Node simplify_binary(const Node exp);
Node simplify_vardec(const Node vardec);
//
// 简化表达式
//
Node simplify_exp(const Node exp) {
    assert(exp->type == YY_Exp);

    switch (exp->child->type) {
        case YY_INT:
        {
            Node p = trans_node(exp, EXP_is_INT);
            p->child = trans_node(exp->child, TERM_INT);
            return p;
        }
        case YY_FLOAT:
        {
            Node p = trans_node(exp, EXP_is_FLOAT);
            p->child = trans_node(exp->child, TERM_FLOAT);
            return p;
        }
        case YY_MINUS:
        {
            Node p = trans_node(exp, EXP_is_UNARY);
            p->val.operator = "-";
            p->child = simplify_exp(exp->child->sibling);
            return p;
        }
        case YY_NOT:
        {
            Node p = trans_node(exp, EXP_is_UNARY);
            p->val.operator = "!";
            p->child = simplify_exp(exp->child->sibling);
            return p;
        }
        case YY_LP:
        {
            return simplify_exp(exp->child->sibling);
        }
        case YY_ID:
        {
            if (exp->child->sibling == NULL) {
                Node p = trans_node(exp, EXP_is_ID);
                p->child = trans_node(exp->child, TERM_ID);
                return p;
            } else {
                return simplify_func_call(exp);
            }
        }
        case YY_Exp:
        {
            return simplify_binary(exp);
        }
        default:
            assert(0);
    }
}

//
// 简化函数调用
//
Node simplify_func_call(const Node exp) {
    Node p = trans_node(exp, EXP_is_ID_ARG);
    p->child = trans_node(exp->child, TERM_ID);
    Node args = exp->child->sibling->sibling;
    Node tail = p->child;
    // TODO Ugly
    while (1) {
        assert(args->type == YY_Args);
        tail->sibling = trans_node(args, ARG_is_EXP);
        tail = tail->sibling;
        tail->child = simplify_exp(args->child);
        if (args->child->sibling != NULL) {
            args = args->child->sibling->sibling;
        } else {
            break;
        }
    }
    return p;
}

//
// 简化二元运算, 包含比较和下标
// 但是比较在代码生成时行为不同, 所以还是要区分
//
Node simplify_binary(const Node exp) {
    Node operator = exp->child->sibling;
    Node p;
    switch (operator->type) {
        case YY_LB:
            p = trans_node(exp, EXP_is_EXP_IDX);
            break;
        case YY_RELOP:
            p = trans_node(exp, EXP_is_RELOP);
            break;
        case YY_AND:
            p = trans_node(exp, EXP_is_AND);
            break;
        case YY_OR:
            p = trans_node(exp, EXP_is_OR);
            break;
        case YY_ASSIGNOP:
            p = trans_node(exp, EXP_is_ASSIGN);
            break;
        case YY_PLUS:
        case YY_MINUS:
        case YY_STAR:
        case YY_DIV:
            p = trans_node(exp, EXP_is_BINARY);
            break;
        case YY_DOT:
            p = trans_node(exp, EXP_is_EXP_FIELD);
            p->child = simplify_exp(exp->child);
            p->child->sibling = operator->sibling;
            return p;
        default:
            assert(0);
    }
    p->child = simplify_exp(exp->child);
    p->val = exp->child->sibling->val;
    p->child->sibling = simplify_exp(exp->child->sibling->sibling);
    return p;
}

Node simplify_compst(const Node compst);
Node simplify_stmt(const Node stmt) {
    assert(stmt && stmt->type == YY_Stmt);
    switch (stmt->child->type) {
        case YY_Exp:
        {
            Node p = trans_node(stmt, STMT_is_EXP);
            p->child = simplify_exp(stmt->child);
            return p;
        }
        case YY_WHILE:
        {
            Node p = trans_node(stmt, STMT_is_WHILE);
            p->child = simplify_exp(stmt->child->sibling->sibling);
            p->child->sibling = simplify_stmt(stmt->child->sibling->sibling->sibling->sibling);
            return p;
        }
        case YY_RETURN:
        {
            Node p = trans_node(stmt, STMT_is_RETURN);
            p->child = simplify_exp(stmt->child->sibling);
            return p;
        }
        case YY_CompSt:
        {
            Node p = trans_node(stmt, STMT_is_COMPST);
            p->child = simplify_compst(stmt->child);
            return p;
        }
        case YY_IF:
        {
            Node if_cond = stmt->child->sibling->sibling;
            Node p = trans_node(stmt, STMT_is_IF);
            p->child = simplify_exp(if_cond);
            p->child->sibling = simplify_stmt(if_cond->sibling->sibling);
            Node if_end = if_cond->sibling->sibling->sibling;
            if (if_end) {
                p->tag = STMT_is_IF_ELSE;  // 注意是修改 tag 而不是 type
                p->child->sibling->sibling = simplify_stmt(if_end->sibling);
            }
            return p;
        }
        default:
            assert(0);
    }
}

//
// 简化 dec, 主要是递归简化, 以及有初始化的时候去掉 ASSIGNOP
//
Node simplify_dec(const Node dec) {
    assert(dec && dec->type == YY_Dec);
    // 获取产生式各结点
    Node vardec = dec->child;
    Node initializer = vardec->sibling ? vardec->sibling->sibling : NULL;
    Node new_vardec = simplify_vardec(vardec);

    Node new_dec;
    if (initializer) {
        new_dec = trans_node(dec, DEC_is_VARDEC_INITIALIZATION);
        new_dec->child = new_vardec;
        new_dec->child->sibling = simplify_exp(initializer);
    } else {
        new_dec = trans_node(dec, DEC_is_VARDEC);
        new_dec->child = new_vardec;
    }

    return new_dec;
}

Node simplify_def(const Node def) {
    assert(def && def->type == YY_Def);
    // 获取产生式各结点
    Node specifier = def->child;
    Node declist = specifier->sibling;
    Node dec = declist->child;

    Node new_def = trans_node(def, DEF_is_SPEC_DEC);
    new_def->child = specifier;  // TODO 暂时没有简化 Specifier 的需求
    Node iterator = new_def->child;
    while (dec->sibling) {
        iterator->sibling = simplify_dec(dec);
        iterator = iterator->sibling;
        dec = dec->sibling->sibling->child;
    }
    iterator->sibling = simplify_dec(dec);
    return new_def;
}

//
// List 类通用简化函数, 用于将递归定义的表结构转化为链表形式
// callback 为针对单点的简化函数
//
Node simplify_list(const Node list, Node (*callback)(const Node)) {
    if (list == NULL) {
        return NULL;
    }
    // 由于可以对应多种 list, 所以没有检查类型标签,
    // 但是可以靠 callback 里的断言做一些检测
    Node child = list->child;
    Node node = callback(child);
    assert(node->child);  // 这种结点不会是终结符
    node->sibling = simplify_list(child->sibling, callback);
    return node;
}

Node simplify_compst(const Node compst) {
    assert(compst && compst->type == YY_CompSt);
    Node compst_simplified = trans_node(compst, COMPST_is_DEF_STMT);
    Node *iterator = &compst_simplified->child;  // 链表迭代器, 用于尾部链接.
    // 由于 Compst 里 DefList 和 StmtList 都可能为 NULL,
    // 所以这里使用非常宽松的子节点遍历. 遍历的目标是将 Def 和 Stmt 直接链接在 Compst.
    Node element = compst->child;
    while (element != NULL) {
        if (element->type == YY_DefList) {
            *iterator = simplify_list(element, simplify_def);
        } else if (element->type == YY_StmtList) {
            *iterator = simplify_list(element, simplify_stmt);
        }

        // list 简化函数返回的简化链表的头部, 我们需要遍历到尾部.
        // 找到为 NULL 的 sibling 指针, 记录下它的地址.
        while (*iterator) {
            iterator = &(*iterator)->sibling;
        }

        element = element->sibling;
    }

    // 检查合法型
    iterator = &compst_simplified->child;
    while (*iterator) {
        Node node = *iterator;
        assert(node->type == YY_Def || node->type == YY_Stmt);
        iterator = &node->sibling;
    }

    return compst_simplified;
}

//
// 简化 vardec
// 主要是去掉数组定义的括号, 大小通过自身的 val 来表示
//
Node simplify_vardec(const Node vardec) {
    assert(vardec && vardec->type == YY_VarDec);
    Node child = vardec->child;
    Node p;
    if (child->type == YY_ID) {
        p = trans_node(vardec, VARDEC_is_ID);
        p->child = trans_node(child, TERM_ID);
    } else if (child->type == YY_VarDec) {
        p = trans_node(vardec, VARDEC_is_VARDEC_SIZE);
        p->child = simplify_vardec(child);
        p->val.i = child->sibling->sibling->val.i;
    } else {
        assert(0);
    }
    return p;
}

//
// 简化 FuncDec
// FuncDec 的 VarList 转化为 var 的 链表
//
Node simplify_fundec(const Node funcdec) {
    assert(funcdec && funcdec->type == YY_FunDec);
    Node id = funcdec->child;
    Node varlist= id->sibling->sibling->type == YY_RP ? NULL : id->sibling->sibling;
    Node new_func = trans_node(funcdec, FUNC_is_ID_VAR);
    new_func->child = trans_node(id, TERM_ID);
    Node *iterator = &new_func->child->sibling;
    while (varlist != NULL) {
        Node paramdec = varlist->child;
        Node spec = paramdec->child;
        Node vardec = simplify_vardec(spec->sibling);
        Node node = trans_node(paramdec, VAR_is_SPEC_VARDEC);
        node->child = trans_node(spec, UNSIMPLIFIED);  // TODO
        node->child->sibling = vardec;

        *iterator = node;
        iterator = &node->sibling;

        varlist = paramdec->sibling == NULL ? NULL : paramdec->sibling->sibling;
    }

    return new_func;
}

Node simplify_extdef(const Node extdef) {
    assert(extdef && extdef->type == YY_ExtDef);
    Node ret = NULL;
    if (extdef->child->sibling->type == YY_FunDec) {
        ret = trans_node(extdef, EXTDEF_is_SPEC_FUNC_COMPST);
        ret->child = trans_node(extdef->child, UNSIMPLIFIED);  // TODO
        ret->child->sibling = simplify_fundec(extdef->child->sibling);
        ret->child->sibling->sibling = simplify_compst(extdef->child->sibling->sibling);
    }
    return ret;
}

Node simplify_tree(const Node prog) {
    Node p = trans_node(prog, PROG_is_EXTDEF);
    Node extdef_list = prog->child;
    Node *iterator = &p->child;
    while (extdef_list != NULL) {
        *iterator = simplify_extdef(extdef_list->child);
        if (*iterator) {
            iterator = &(*iterator)->sibling;
        }

        extdef_list = extdef_list->child->sibling;
    }
    return p;
}
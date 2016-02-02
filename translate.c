#include "translate.h"
#include "node.h"
#include "ir.h"
#include "operand.h"
#include "cmm-symtab.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>


extern Node prog;
extern FILE *asm_file;


TranslateState translate_state = FINE;


// Used when translation meets an unexpected syntax tree (root).
// Such syntax cannot be translated, so we change the global state
// and intterrupt the code generation after translation.
static void trans_default(Node node)
{
    translate_state = UNSUPPORT;
}


typedef void (*trans_visitor)(Node);

static trans_visitor trans_visitors[];

#define translate_dispatcher(node) \
    ((node) ? (trans_visitors[(node)->tag] ? trans_visitors[node->tag](node) : trans_default(node)) : (void)0)


/////////////////////////////////////////////////////////////////////
//  Interface
/////////////////////////////////////////////////////////////////////


void translate()
{
    translate_dispatcher(prog);

#ifdef DEBUG
    FILE *fp = fopen("test.ir", "w");
#else
    FILE *fp = stdout;
#endif

    if (translate_state == FINE) {
        print_instr(fp);
    }
    else {
        fputs("Cannot translate: Code contains variables or parameters of structure type.", stderr);
    }
}


/////////////////////////////////////////////////////////////////////
//  Program translation
/////////////////////////////////////////////////////////////////////


static void translate_ast(Node ast)
{
    Node extdef = ast->child;
    while (extdef != NULL) {
        translate_dispatcher(extdef);
        extdef = extdef->sibling;
    }
}


/////////////////////////////////////////////////////////////////////
//  Utility functions
/////////////////////////////////////////////////////////////////////


// TODO 每个 Compst 在跳转指令生成前可以计算所有常量
// TODO 左值解引用只能用在赋值操作中, 不能像 & 和右解引用那样随意嵌入
static void free_ope(Operand *ptr)
{
    if (*ptr != NULL) {
        free(*ptr);
    }
    *ptr = NULL;
}


// 如果算出结果是地址, 临时生成变量来接收
static void try_deref(Node exp)
{
    if (exp->dst->type == OPE_ADDR) {
        Operand tmp = new_operand(OPE_TEMP);
        LOG("子表达式返回值为地址, 用临时变量%s接受其解引用值", print_operand(tmp));
        new_instr(IR_DEREF_R, exp->dst, NULL, tmp);
        exp->dst = tmp;
    }
}


/////////////////////////////////////////////////////////////////////
//  Expression translation
/////////////////////////////////////////////////////////////////////


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
static void translate_exp_is_id(Node exp)
{
    Node id = exp->child;
    const Symbol *sym = query(id->val.s);
    if (sym == NULL) {
        return;
    }

    // Directly return the id's value
    if (exp->dst && exp->dst->type == OPE_TEMP) {
        free(exp->dst);
    }

    if (sym->type->class == CMM_STRUCT) {
        exp->dst = new_operand(OPE_ADDR);
        exp->base = exp->dst;
        exp->dst->base_type = sym->type;
        new_instr(IR_ADDR, sym->address, NULL, exp->dst);
    }
    else if (sym->type->class == CMM_ARRAY) {
        exp->dst = new_operand(OPE_INTEGER);
        exp->dst->integer = 0;
        exp->base = new_operand(OPE_ADDR);
        new_instr(IR_ADDR, sym->address, NULL, exp->base);
    }
    else {
        exp->dst = sym->address;
    }
}


//
// 翻译表达式: 字面常量
//
static void translate_exp_is_const(Node nd)
{
    assert(nd->tag == EXP_is_INT || nd->tag == EXP_is_FLOAT);

    Operand const_ope;
    switch (nd->child->tag) {
        case TERM_INT:
            const_ope = new_operand(OPE_INTEGER);
            const_ope->integer = nd->child->val.i;
            break;
        case TERM_FLOAT:
            const_ope = new_operand(OPE_FLOAT);
            const_ope->real = nd->child->val.f;
            break;
        default:
            return;
    }

    // 替换不必要的目标地址
    // 如果赋值语句结点重复进入, 则被 free 掉的不是无用的 temp,
    // 而可能是到处引用的变量!
    if (nd->dst != NULL) {
        free(nd->dst);
        nd->dst = NULL;
    }
    nd->dst = const_ope;
}


//
// 翻译表达式: 赋值语句
// 对于赋值语句, 如果左边的表达式不是值类型(数组和结构体0偏移的域也可以直接用值类型)
// 那么就是数组或者结构体计算出来的偏移地址, 这时候是需要进行解引用操作
//
// [优化策略]
// 很多指令自带赋值功能, 虽然这些运算指令的左值不能解引用, 但是对于如下表达式:
//     a = b + c;
// 我们可以直接生成指令 va := vb + vc
// 我们目前朴素的赋值语句翻译(11月21日)会将上面的 c-- 代码翻译成这样:
//     t1 := vb + vc
//     va := t1
// 由于我们先分析的左值表达式, 所以可以很明确的知道下面是要赋值给变量还是使用解引用指令,
// 解引用的场合不可避免地要生成中间代码来传值.
// 但是我们不能直接把变量操作数传递给右值表达式, 因为右值表达式在知道自己是常量的情况下会替换掉继承的操作数,
// 所以我们采取的策略是给一个无关紧要的临时变量, 待右值指令生成完毕后, 考察其类型, 如果不是常量,
// 必然会使用继承的操作数, 这时候可以修改该操作数的内容, 将其变换成左值变量. 不能使用直接修改指令的方法,
// 因为像条件表达式这种, 很可能会在多处使用继承的目标操作数.
//
static void translate_exp_is_assign(Node assign_exp)
{
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

    try_deref(rexp);  // 如果 rexp 直接是 array[...] 则会直接返回地址

    if (assign_exp->dst) {
        LOG("表达式连续赋值");
        LOG("左边: %s", print_operand(assign_exp->dst));
        LOG("右边: %s", print_operand(lexp->dst));
        free(assign_exp->dst);
        assign_exp->dst = lexp->dst;
    }

    // [优化] 当左值为变量而右值为运算指令时, 将右值的目标操作数转化为变量
    if (lexp->dst->type == OPE_VAR && rexp->dst->type == OPE_TEMP) {
        LOG("左值为变量(编号%d), 直接赋值", lexp->dst->index);
        replace_operand_global(lexp->dst, rexp->dst);
        // 这里实际上保证了不会出现如下的情景:
        //     t := *a
        //     v := t
        // 这个情景如果不替换, 则会非常危险, 因为 =* 只能和 t 相关联, 这样内联替换才是正确的.
        // 因为 t 是数组访问的显式表达式产生的, 这样的显式表达式想怎么依赖就怎么依赖.
        // 但是如果一个变量和 t 直接关联, 会后面与该变量相关的指令都会去引用 *a, 则是错误的, 因为 *a
        // 可以在未知的情况下被改变.
    }
    else if (lexp->dst->type == OPE_ADDR) {   // 左边是引用
        new_instr(IR_DEREF_L, lexp->dst, rexp->dst, NULL);
    }
    else {
        LOG("直接的赋值情况应该不会发生了");
        new_instr(IR_ASSIGN, rexp->dst, NULL, lexp->dst);
    }
}


//
// 翻译下标表达式
//
static void translate_exp_is_exp_idx(Node exp)
{
    if (exp->dst == NULL) {
        return;
    }

    Node base = exp->child;
    base->dst = new_operand(OPE_TEMP);
    translate_dispatcher(base);

    Node idx = base->sibling;
    idx->dst = new_operand(OPE_TEMP);
    translate_dispatcher(idx);
    try_deref(idx);  // 如果是数组做下标的话, 则不能忽视解引用

    // 现在我们已经准备好了一个基地址和偏移量, 以及"当前"数组的类型
    // 为了进一步计算偏移量, 我们需要访问数组的基类型, 获得基类型的大小, 用当前下标去计算
    // 新的偏移量, 如果综合来的偏移量和下标有一个为非常量, 则要生成指令并转移操作数

    Operand offset = idx->dst;

    // 计算本层偏移
    int size = exp->sema.type->type_size;
    if (offset->type == OPE_INTEGER) {
        offset->integer = offset->integer * size;
    }
    else {
        Operand p = new_operand(OPE_ADDR);
        Operand array_size = new_operand(OPE_INTEGER);
        array_size->integer = size;
        new_instr(IR_MUL, offset, array_size, p);
        offset = p;  // 转移本层偏移量
    }

    // 如果 base->base 为空, 那么就是在非 id 处获得了数组的地址值,
    // 那么 base->dst 才是需要的值.
    if (base->base == NULL) {
        WARN("Line %d: 没有从 id 获得数组地址", exp->lineno);
        base->base = base->dst;
        base->dst = new_operand(OPE_INTEGER);
        base->dst->integer = 0;
    }

    Operand addr = base->dst;

    // 计算总偏移
    if (addr->type == OPE_INTEGER && offset->type == OPE_INTEGER) {
        LOG("Line %d: 地址偏移为常数, 直接计算", exp->lineno);
        addr->integer += offset->integer;
    }
    else if (addr->type == OPE_INTEGER && addr->integer == 0) {
        LOG("Line %d: 旧偏移量为常数0, 直接更新", exp->lineno);
        addr = offset;
    }
    else if (offset->type == OPE_INTEGER && offset->integer == 0) {
        LOG("Line %d: 新增偏移量为常数0, 不更新", exp->lineno);
    }
    else {
        Operand p = new_operand(OPE_ADDR);
        new_instr(IR_ADD, addr, offset, p);
        addr = p;  // 再转移本层偏移量
    }

    if (exp->dst && exp->dst->type == OPE_TEMP) {
        free(exp->dst);
    }
    else {
        WARN("没有来自上层exp(行号: %d)的目标操作数, 这不符合常理", exp->lineno);
    }

    // 现在我们就有了完整的偏移量
    if (exp->sema.type->class == CMM_ARRAY) {
        // 说明在下标翻译过程中
        exp->dst = addr;
        exp->base = base->base;
    }
    else {
        // 进入这里说明是最后一个阶段, 要将地址进行解引用
        if (addr->type == OPE_INTEGER && addr->integer == 0) {
            LOG("Line %d: 计算出引用偏移量为 0, 不生成加法指令", exp->lineno);
            exp->dst = new_operand(OPE_ADDR);  // 区分变量数组和参数数组
            exp->dst->base_type = exp->sema.type;   // 多维数组
            new_instr(IR_ASSIGN, (base->base ?: base->dst), NULL, exp->dst);
        }
        else {
            // 要生成加法指令
            exp->dst = new_operand(OPE_ADDR);
            exp->dst->base_type = exp->sema.type;  // 多维数组
            new_instr(IR_ADD, (base->base ?: base->dst), addr, exp->dst);
        }
    }
}


//
// 翻译括号表达式
//
static void translate_exp_is_exp(Node exp)
{
    // 需要继承!
    exp->child->dst = exp->dst;
    translate_dispatcher(exp->child);
    // 还需要综合!
    exp->dst = exp->child->dst;
}


//
// 翻译一元运算: 只有取负
//
static void translate_unary_operation(Node exp)
{
    Node rexp = exp->child;
    rexp->dst = new_operand(OPE_TEMP);

    // 常量计算
    Operand const_ope = new_operand(OPE_NOT_USED);
    if (rexp->dst->type == OPE_INTEGER) {
        const_ope->type = OPE_INTEGER;
        const_ope->integer = -rexp->dst->integer;
        free_ope(&exp->dst);
        exp->dst = const_ope;
    }
    else if (rexp->dst->type == OPE_FLOAT) {
        const_ope->type = OPE_FLOAT;
        const_ope->real = -rexp->dst->real;
        free_ope(&exp->dst);
        exp->dst = const_ope;
    }
    else {
        // 变量情况
        free(const_ope);
        Operand p = new_operand(OPE_INTEGER);
        p->integer = 0;
        new_instr(IR_SUB, p, rexp->dst, exp->dst);
    }
}

#define CALC(op, rs, rt, rd, type) do {\
    switch (op) {\
        case '+': rd->type = rs->type + rt->type; break;\
        case '-': rd->type = rs->type - rt->type; break;\
        case '*': rd->type = rs->type * rt->type; break;\
        case '/': rd->type = rs->type / rt->type; break;\
    }\
    free_ope(&exp->dst);\
    exp->dst = rd;\
    return;\
} while (0)

static void translate_binary_operation(Node exp)
{
    // 没有目标地址, 不需要翻译
    if (exp->dst == NULL) {
        return;
    }

    Node lexp = exp->child;
    Node rexp = lexp->sibling;
    lexp->dst = new_operand(OPE_TEMP);
    translate_dispatcher(lexp);
    rexp->dst = new_operand(OPE_TEMP);
    translate_dispatcher(rexp);
    try_deref(lexp);
    try_deref(rexp);

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
        case '+': new_instr(IR_ADD, lope, rope, exp->dst); break;
        case '-': new_instr(IR_SUB, lope, rope, exp->dst); break;
        case '*': new_instr(IR_MUL, lope, rope, exp->dst); break;
        case '/': new_instr(IR_DIV, lope, rope, exp->dst); break;
        default: assert(0);
    }
}
#undef CALC


// Translate exp -> exp.field
// This translation will set exp->dst to an addr operand
// Remember to dereference it
static void translate_exp_is_exp_field(Node exp)
{
    Node struc = exp->child;
    Node field = struc->sibling;
    // Recursive translation of exp.field and array will return address
    struc->dst = new_operand(OPE_ADDR);
    translate_dispatcher(struc);
    // The semantic type of this exp node is set in semantic analysis,
    // So we directly use it.
    const Symbol *sym = query_without_fallback(field->val.s, struc->sema.type->field_table);
    if (sym == NULL) {
        PANIC("Unexpected non-exisiting field %s at line %d\n", field->val.s, field->lineno);
    }
    else {
        Operand offset = new_operand(OPE_INTEGER);
        offset->integer = sym->offset;
        if (exp->dst && exp->dst->type == OPE_TEMP) {
            free(exp->dst);
        }
        exp->dst = new_operand(OPE_ADDR);
        new_instr(IR_ADD, struc->dst, offset, exp->dst);
    }
}


static void pass_arg(Node arg)
{
    if (arg == NULL) {
        return;
    }
    arg->dst = new_operand(OPE_TEMP);
    translate_dispatcher(arg);

    pass_arg(arg->sibling);

    Operand p = arg->dst;
    if (p->base_type && (p->base_type->class == CMM_ARRAY || p->base_type->class == CMM_STRUCT)) {
        // 按照测试样例, 数组要传地址
        LOG("Reference parameter");
        assert(p->type == OPE_REF || p->type == OPE_ADDR);
        if (p->type == OPE_REF) {
            arg->dst = new_operand(OPE_ADDR);
            new_instr(IR_ADDR, p, NULL, arg->dst);
        }
    }
    else {
        try_deref(arg);
    }
    new_instr(IR_ARG, arg->dst, NULL, NULL);
}


static void translate_call(Node call)
{
    Node func = call->child;
    Node arg = func->sibling;

    if (call->dst == NULL) {
        call->dst = new_operand(OPE_TEMP);
    }

    if (!strcmp(func->val.s, "read")) {
        new_instr(IR_READ, NULL, NULL, call->dst);
    }
    else if (!strcmp(func->val.s, "write")) {
        arg->dst = new_operand(OPE_TEMP);
        translate_dispatcher(arg);
        try_deref(arg);  // 这里的思路和return是类似的
        new_instr(IR_WRITE, arg->dst, NULL, NULL);
    }
    else {  // Common function call
        pass_arg(arg);
        if (call->dst == NULL) {
            call->dst = new_operand(OPE_TEMP);
        }
        Operand f = new_operand(OPE_FUNC);
        f->name = func->val.s;
        new_instr(IR_CALL, f, NULL, call->dst);
    }
}


// The expression is translated as normal,
// but its value needs to be used to change the control flow.
static void translate_cond_exp(Node exp)
{
    exp->dst = new_operand(OPE_TEMP);
    translate_dispatcher(exp);
    Operand const_zero = new_operand(OPE_INTEGER);
    const_zero->integer = 0;
    new_instr(IR_BNE, exp->dst, const_zero, exp->label_true);
    new_instr(IR_JMP, exp->label_false, NULL, NULL);
}


//
// 在条件判断框架下翻译 NOT
//
static void translate_cond(Node);
// NOT changes the control flow directly,
// which does not need to see expression as data.
// Therefore we use translate_cond, not translate_dispatcher.
static void translate_cond_not(Node exp)
{
    Node sub_exp = exp->child;
    sub_exp->label_true = exp->label_false;
    sub_exp->label_false = exp->label_true;
    translate_cond(sub_exp);
}


//
// 在条件判断框架下翻译 RELOP
// TODO 优化重点!
//
static void translate_cond_relop(Node exp)
{
    Node left = exp->child;
    Node right = left->sibling;

    left->dst = new_operand(OPE_TEMP);
    right->dst = new_operand(OPE_TEMP);

    // 获取结果值
    translate_dispatcher(left);
    translate_dispatcher(right);

    try_deref(left);
    try_deref(right);

    const char *op = exp->val.operator;
    IR_Type relop = get_relop(op);

    new_instr(relop, left->dst, right->dst, exp->label_true);

    new_instr(IR_JMP, exp->label_false, NULL, NULL);
}


//
// 翻译 与 表达式
//
static void translate_cond_and(Node exp)
{
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
    translate_cond(right);
}


//
// 翻译 或 表达式
//
static void translate_cond_or(Node exp)
{
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
    translate_cond(right);
}


static void translate_cond(Node exp)
{
    switch (exp->tag) {
        case EXP_is_AND:
            translate_cond_and(exp);
            break;
        case EXP_is_OR:
            translate_cond_or(exp);
            break;
        case EXP_is_RELOP:
            translate_cond_relop(exp);
            break;
        case EXP_is_NOT:
            translate_cond_not(exp);
            break;
        default:
            translate_cond_exp(exp);
    }
}


// Logic expressions in the 'if' or 'while' statements
// just change the control flow, but if they appear in normal
// expressions, they do return a value. The value's changing
// is like a control flow. Here we prepare such an operand to let
// the logic expression change the flow of its value's changing.
static void translate_cond_prepare(Node node)
{
    node->label_true = new_operand(OPE_LABEL);
    node->label_false = new_operand(OPE_LABEL);

    if (node->dst != NULL) {
        if (node->dst->type == OPE_TEMP) {
            free(node->dst);
            node->dst = new_operand(OPE_BOOL);
        }
        Operand value_false = new_operand(OPE_INTEGER);
        value_false->integer = 0;
        new_instr(IR_ASSIGN, value_false, NULL, node->dst);
    }

    translate_cond(node);

    new_instr(IR_LABEL, node->label_true, NULL, NULL);

    if (node->dst != NULL) {
        Operand value_true = new_operand(OPE_INTEGER);
        value_true->integer = 1;
        new_instr(IR_ASSIGN, value_true, NULL, node->dst);
    }

    new_instr(IR_LABEL, node->label_false, NULL, NULL);
}


/////////////////////////////////////////////////////////////////////
//  Statements
/////////////////////////////////////////////////////////////////////


static void translate_for(Node stmt)
{
    Node init_exp = stmt->child;
    Node cond_exp = init_exp->sibling;
    Node step_exp = cond_exp->sibling;
    Node loop_stmt = step_exp->sibling;

    translate_dispatcher(init_exp);
    
    Operand begin = new_operand(OPE_LABEL);
    cond_exp->label_true = new_operand(OPE_LABEL);
    cond_exp->label_false = new_operand(OPE_LABEL);
    loop_stmt->label_true = new_operand(OPE_LABEL);  // act as loop_stmt's next label
    loop_stmt->label_false = cond_exp->label_false;
    // TODO test the case that loop_stmt is immediately the if-statement or relop-exp

    new_instr(IR_LABEL, begin, NULL, NULL);

    translate_cond(cond_exp);  // We are not really need the value of cond_exp

    new_instr(IR_LABEL, cond_exp->label_true, NULL, NULL);

    translate_dispatcher(loop_stmt);

    new_instr(IR_LABEL, loop_stmt->label_true, NULL, NULL);

    translate_dispatcher(step_exp);

    new_instr(IR_JMP, begin, NULL, NULL);

    new_instr(IR_LABEL, cond_exp->label_false, NULL, NULL);
}


static void translate_while(Node stmt)
{
    Node cond = stmt->child;
    Node loop = cond->sibling;

    cond->label_true = new_operand(OPE_LABEL);
    cond->label_false = new_operand(OPE_LABEL);
    Operand begin = new_operand(OPE_LABEL);

    new_instr(IR_LABEL, begin, NULL, NULL);

    translate_cond(cond);

    new_instr(IR_LABEL, cond->label_true, NULL, NULL);

    translate_dispatcher(loop);

    new_instr(IR_JMP, begin, NULL, NULL);

    new_instr(IR_LABEL, cond->label_false, NULL, NULL);
}


static void translate_if_else(Node exp)
{
    Node cond = exp->child;
    Node true_stmt = cond->sibling;
    Node false_stmt = true_stmt->sibling;

    cond->label_true = new_operand(OPE_LABEL);
    cond->label_false = new_operand(OPE_LABEL);
    Operand next = new_operand(OPE_LABEL);

    translate_cond(cond);

    new_instr(IR_LABEL, cond->label_true, NULL, NULL);

    translate_dispatcher(true_stmt);

    new_instr(IR_JMP, next, NULL, NULL);

    new_instr(IR_LABEL, cond->label_false, NULL, NULL);

    translate_dispatcher(false_stmt);

    new_instr(IR_LABEL, next, NULL, NULL);
}


static void translate_if(Node exp)
{
    Node cond = exp->child;
    Node stmt = cond->sibling;
    cond->label_true = new_operand(OPE_LABEL);
    cond->label_false = new_operand(OPE_LABEL);
    translate_cond(cond);
    new_instr(IR_LABEL, cond->label_true, NULL, NULL);
    translate_dispatcher(stmt);
    new_instr(IR_LABEL, cond->label_false, NULL, NULL);
}


//
// 翻译返回语句
//
static void translate_return(Node exp)
{
    Node sub_exp = exp->child;
    sub_exp->dst = new_operand(OPE_TEMP);
    translate_dispatcher(sub_exp);
    try_deref(sub_exp);
    new_instr(IR_RET, sub_exp->dst, NULL, NULL);
}


//
// 翻译复合语句: 纯粹的遍历框架
//
static void translate_compst(Node compst)
{
    Node child = compst->child;
    while (child != NULL) {
        translate_dispatcher(child);
        child = child->sibling;
    }
}


//
// 翻译复合语句
//
static void translate_stmt_is_compst(Node stmt)
{
    assert(stmt->sema.symtab != NULL);
    push_symtab(stmt->sema.symtab);

    translate_dispatcher(stmt->child);

    Symbol **symtab = pop_symtab();
    assert(symtab == stmt->sema.symtab);
}


//
// 翻译表达式: 如果没有赋值之类的, 这条基本不需要生成指令了
//
static void translate_stmt_is_exp(Node stmt)
{
    translate_dispatcher(stmt->child);
}


/////////////////////////////////////////////////////////////////////
//  Definition translation
/////////////////////////////////////////////////////////////////////


static void translate_extdef_spec(Node extdef)
{
}


static void translate_extdef_func(Node extdef)
{
    push_symtab(extdef->sema.symtab);
    Node spec = extdef->child;
    Node func = spec->sibling;
    translate_dispatcher(func);
    Node compst = func->sibling;
    translate_dispatcher(compst);
    
    Symbol **symtab = pop_symtab();
    assert(symtab == extdef->sema.symtab);
}


//
// 翻译函数: 主要是生成参数声明指令 PARAM
//
static void translate_func_head(Node func)
{
    Node funcname = func->child;
    Node param = funcname->sibling;

    // 声明函数头
    Operand rs = new_operand(OPE_FUNC);
    rs->name = funcname->val.s;
    new_instr(IR_FUNC, rs, NULL, NULL);

    // 声明变量
    while (param != NULL) {
        Node spec = param->child;
        // 无脑找变量名
        Node id = spec->sibling;
        while (id->tag != TERM_ID) {
            id = id->child;
        }
        // TODO eliminate coercion
        Symbol *sym = (Symbol *)query(id->val.s);
        if (sym->type->class == CMM_ARRAY) {
            sym->address = new_operand(OPE_ADDR);
            sym->address->base_type = sym->type;
        }
        else {
            sym->address = new_operand(OPE_VAR);
        }
        // 实际的大小是在调用者那边说明
        new_instr(IR_PARAM, sym->address, NULL, NULL);
        param = param->sibling;
    }
}


//
// 翻译定义: 找 ID
// dec 可以简单实现, 只要找数组定义就行了
//
static void translate_dec_is_vardec(Node dec)
{
    Node vardec = dec->child;
    Node iterator = vardec->child;

    while (iterator->tag != TERM_ID) {
        iterator = iterator->child;
    }

    // TODO eliminate coercion
    Symbol *sym = (Symbol *)query(iterator->val.s);

    if (!typecmp(sym->type, BASIC_INT) && !typecmp(sym->type, BASIC_FLOAT)) {
        sym->address = new_operand(OPE_REF);
        sym->address->size = sym->type->type_size;
        Operand size = new_operand(OPE_INTEGER);
        size->integer = sym->type->type_size;
        new_instr(IR_DEC, sym->address, size, NULL);
    }
    else {
        sym->address = new_operand(OPE_VAR);
    }

    sym->address->base_type = sym->type;

    if (vardec->sibling != NULL) {
        // Translate initialization expression.
        vardec->sibling->dst = new_operand(OPE_TEMP);
        translate_dispatcher(vardec->sibling);
        try_deref(vardec->sibling);
        // [优化] 当左值为变量而右值为运算指令时, 将右值的目标操作数转化为变量
        if (sym->address->type == OPE_VAR && vardec->sibling->dst->type == OPE_TEMP) {
            LOG("初始化: 左值为变量(编号%d), 直接赋值", sym->address->index);
            replace_operand_global(sym->address, vardec->sibling->dst);
        }
        else {
            LOG("初始化: 直接的赋值");
            new_instr(IR_ASSIGN, vardec->sibling->dst, NULL, sym->address);
        }
    }
}


//
// 翻译定义: 主要是用来遍历 declist 的
//
static void translate_def_is_spec_dec(Node def)
{
    Node spec = def->child;
    Node dec = spec->sibling;
    while (dec != NULL) {
        translate_dispatcher(dec);
        dec = dec->sibling;
    }
}


static trans_visitor trans_visitors[] =
{
    [PROG_is_EXTDEF]               = translate_ast,
    [EXTDEF_is_SPEC]               = translate_extdef_spec,
    [EXTDEF_is_SPEC_FUNC_COMPST]   = translate_extdef_func,
    [FUNC_is_ID_VAR]               = translate_func_head,
    [COMPST_is_DEF_STMT]           = translate_compst,
    [DEC_is_VARDEC]                = translate_dec_is_vardec,
    [DEC_is_VARDEC_INITIALIZATION] = translate_dec_is_vardec,
    [DEF_is_SPEC_DEC]              = translate_def_is_spec_dec,
    [STMT_is_COMPST]               = translate_stmt_is_compst,
    [STMT_is_EXP]                  = translate_stmt_is_exp,
    [STMT_is_RETURN]               = translate_return,
    [STMT_is_IF]                   = translate_if,
    [STMT_is_IF_ELSE]              = translate_if_else,
    [STMT_is_FOR]                  = translate_for,
    [STMT_is_WHILE]                = translate_while,
    [EXP_is_EXP]                   = translate_exp_is_exp,
    [EXP_is_BINARY]                = translate_binary_operation,
    [EXP_is_EXP_FIELD]             = translate_exp_is_exp_field,
    [EXP_is_UNARY]                 = translate_unary_operation,
    [EXP_is_INT]                   = translate_exp_is_const,
    [EXP_is_FLOAT]                 = translate_exp_is_const,
    [EXP_is_ID]                    = translate_exp_is_id,
    [EXP_is_ID_ARG]                = translate_call,
    [EXP_is_ASSIGN]                = translate_exp_is_assign,
    [EXP_is_EXP_IDX]               = translate_exp_is_exp_idx,
    [EXP_is_AND]                   = translate_cond_prepare,
    [EXP_is_OR]                    = translate_cond_prepare,
    [EXP_is_NOT]                   = translate_cond_prepare,
    [EXP_is_RELOP]                 = translate_cond_prepare,
};


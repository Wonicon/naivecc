//
// Created by whz on 15-11-20.
//

#include "ir.h"
#include "operand.h"
#include "basic-block.h"
#include "dag.h"
#include "asm.h"
#include "register.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern FILE *asm_file;  // Stream to store assembly code.

#ifdef INLINE_REPLACE
bool inline_deref = true;
bool inline_addr = true;
#else
bool inline_deref = false;
bool inline_addr = false;
#endif

static Block blk_buf[MAX_LINE];
static int nr_blk;

// 指令缓冲区
static IR instr_buffer[MAX_LINE];
// 已经生成的指令数量
int nr_instr;

// 操作数答应缓冲区
static char rs_s[NAME_LEN];
static char rt_s[NAME_LEN];
static char rd_s[NAME_LEN];

struct {
    IR_Type relop;
    const char *str;
    IR_Type anti;
} relop_dict[] = {
    { IR_BEQ, "==", IR_BNE },
    { IR_BLT, "<" , IR_BGE },
    { IR_BLE, "<=", IR_BGT },
    { IR_BGT, ">" , IR_BLE },
    { IR_BGE, ">=", IR_BLT },
    { IR_BNE, "!=", IR_BEQ }
};

#define LENGTH(x) (sizeof(x) / sizeof(*x))

IR ir_from_dag[MAX_LINE];
int nr_ir_from_dag = 0;
extern DagNode dag_buf[];
extern int dagnode_count;

//
// 中间代码构造函数
// 返回 IR 在缓冲区中的下标
//
void new_instr_(IR *pIR, IR_Type type, Operand rs, Operand rt, Operand rd) {
    assert(nr_instr < MAX_LINE);

    pIR->type = type;
    pIR->rs = rs;
    pIR->rt = rt;
    pIR->rd = rd;
}

int new_instr(IR_Type type, Operand rs, Operand rt, Operand rd) {
    new_instr_(&instr_buffer[nr_instr], type, rs, rt, rd);
    return nr_instr++;
}

static const char *ir_format[] = {
    [IR_NOP]     = "",                     // NOP
    [IR_LABEL]   = "%sLABEL %s :",         // LABEL
    [IR_FUNC]    = "%sFUNCTION %s :",      // FUNCTION
    [IR_ASSIGN]  = "%s := %s",             // ASSIGN
    [IR_ADD]     = "%s := %s + %s",        // ADD
    [IR_SUB]     = "%s := %s - %s",        // SUB
    [IR_MUL]     = "%s := %s * %s",        // MUL
    [IR_DIV]     = "%s := %s / %s",        // DIV
    [IR_ADDR]    = "%s := &%s",            // ADDR
    [IR_DEREF_R] = "%s := *%s",            // DEREF_R
    [IR_DEREF_L] = "%s*%s := %s",          // DEREF_L, 虽然地址在左边, 但是是参数
    [IR_JMP]     = "%sGOTO %s",            // JMP
    [IR_BEQ]     = "IF %s == %s GOTO %s",  // BEQ
    [IR_BLT]     = "IF %s < %s GOTO %s",   // BLT
    [IR_BLE]     = "IF %s <= %s GOTO %s",  // BLE
    [IR_BGT]     = "IF %s > %s GOTO %s",   // BGT
    [IR_BGE]     = "IF %s >= %s GOTO %s",  // BGE
    [IR_BNE]     = "IF %s != %s GOTO %s",  // BNE
    [IR_RET]     = "%sRETURN %s",          // RETURN
    [IR_DEC]     = "%sDEC %s %s",          // DEC, 第一个 %s 过滤 rd_s
    [IR_ARG]     = "%sARG %s",             // Pass argument, 第一个 %s 过滤 rd_s
    [IR_CALL]    = "%s := CALL %s",        // CALL
    [IR_PRARM]   = "%sPARAM %s",           // DEC PARAM, 第一个 %s 过滤 rd_s
    [IR_READ]    = "READ %s",              // READ
    [IR_WRITE]   = "%sWRITE %s",           // WRITE, 第一个 %s 过滤 rd_s, 输出语义不用 rd
};

//
// 单条指令打印函数
//
const char *ir_to_s(IR *pir) {
    static char buf[120];
    strcpy(rd_s, print_operand(pir->rd));
    strcpy(rs_s, print_operand(pir->rs));
    strcpy(rt_s, print_operand(pir->rt));

    // 约定 BEQ 和 BNE 包围所有 Branch 指令
    if (IR_BEQ <= pir->type && pir->type <= IR_BNE) {
        sprintf(buf, ir_format[pir->type], rs_s, rt_s, rd_s);  // 交换顺序
    } else if (pir->type == IR_DEC) {  // 规划干不过特例
        sprintf(buf, ir_format[pir->type], rd_s, rs_s, rt_s + 1);
    } else {
        sprintf(buf, ir_format[pir->type], rd_s, rs_s, rt_s);
    }

    return buf;
}

void print_single_instr(IR instr, FILE *file) {
    fprintf(file, "%s", ir_to_s(&instr));

    if (instr.type != IR_NOP) {
        fprintf(file, "\n");
    }
}


//
// Calculate all variables' offset to the function entry
//

#define MAP_SIZE 4096

int calc_offset(IR buf[], int index, int n)
{
    static bool exists[MAP_SIZE];
    static IR *curr = NULL;

    TEST(n <= MAP_SIZE, "exceed exists-map size");

    if (index >= n) {
        return 0;  // meaningless
    }

    if (buf[index].type == IR_FUNC) {
        memset(exists, 0, sizeof(exists));
        curr = &buf[index];
        curr->rs->size = 0;
    } else {
        for (int i = 0; i < 3; i++) {
            Operand ope = buf[index].operand[i];
            if (ope == NULL) {
                continue;
            }

            switch (ope->type) {
            case OPE_VAR:
            case OPE_REF:
            case OPE_TEMP:
            case OPE_BOOL:
            case OPE_ADDR:
                if (!exists[ope->index]) {
                    curr->rs->size += ope->size;
                    ope->address = curr->rs->size;  // Calc afterwards because the stack grows from high to low
                    exists[ope->index] = true;
                }
                break;
            default:
                ;
            }
        }
    }

    return calc_offset(buf, index + 1, n);
}



//
// 打印指令缓冲区中所有的已生成指令
//
void preprocess_ir();
void optimize_in_block();
void inline_replace(IR buf[], int nr);
int compress_ir(IR buf[], int n);

void print_instr(FILE *file) {
    // 相当于窥孔优化
    preprocess_ir();

    calc_offset(instr_buffer, 0, nr_instr);

    optimize_in_block();

#ifdef DEBUG
    for (int i = 0; i < nr_instr; i++) {
        print_single_instr(instr_buffer[i], file);
    }
    fclose(file);
#endif


    if (inline_deref && inline_addr) {
        inline_replace(ir_from_dag, nr_ir_from_dag);
        nr_ir_from_dag = compress_ir(ir_from_dag, nr_ir_from_dag);
    }

    // Generate assembly code
    for (int i = 0; i < nr_blk; i++) {
        LOG("A new block");
        fprintf(asm_file, "# basic block\n");
        Block *blk = &blk_buf[i];
        int j;
        for (j = blk->start; j < blk->end - 1; j++) {
            LOG("ir %d", j + 1);
            gen_asm(instr_buffer + j);
        }
        push_all();
        gen_asm(instr_buffer + j);
        clear_reg_state();
    }
}

//
// 预处理工具 - 全局替换操作数, 无论其出现在哪个位置
//

typedef enum {
    REP_SRC = (1 << 0),  // 只替换源操作数
    REP_DST = (1 << 1),  // 只替换目标操作数
    REP_BLK = (1 << 2),  // 只在块内替换
} RepOpeMode;

//
// 替换操作数, 返回替换数量
//
int replace_operand(Operand newbie, Operand old, RepOpeMode mode) {
    int rep_count = 0;
    for (int i = 0; i < nr_instr; i++) {
        IR *pIR = &instr_buffer[i];
        if (pIR->type == IR_NOP) {
            continue;
        }
        // TODO 检查 basic block
        if (pIR->rs == old && (mode & REP_SRC)) {
            pIR->rs = newbie;
            rep_count++;
        }
        if (pIR->rt == old && (mode & REP_SRC)) {
            pIR->rt = newbie;
            rep_count++;
        }
        if (pIR->rd == old && (mode & REP_DST)) {
            pIR->rd = newbie;
            rep_count++;
        }
    }
    return rep_count;
}

//
// 全局替换操作数
//
int replace_operand_global(Operand newbie, Operand old) {
    return replace_operand(newbie, old, REP_SRC | REP_DST | REP_BLK);
}

//
// 检查是否为分支类指令
//
int is_branch(IR *pIR) {
    return IR_BEQ <= pIR->type && pIR->type <= IR_BNE;
}

//
// 检查是否为跳转类指令
//
bool can_jump(IR *pIR) {
    if (is_branch(pIR)) {
        return true;
    } else {
        switch (pIR->type) {
            case IR_JMP:
                return true;
            default:
                return false;
        }
    }
}

//
// relop 字典使用接口
//
#define search_relop_common(name, field, type, miss_val) \
    type name(IR_Type relop) {                           \
        for (int i = 0; i < LENGTH(relop_dict); i++) {   \
            if (relop_dict[i].relop == relop) {          \
                return relop_dict[i].field;              \
            }                                            \
        }                                                \
        return miss_val;                                 \
    }

search_relop_common(get_relop_symbol, str, const char *, NULL)

search_relop_common(get_relop_anti, anti, IR_Type, OPE_NOT_USED)

    IR_Type get_relop(const char *sym) {
        for (int i = 0; i < LENGTH(relop_dict); i++) {
            if (!strcmp(relop_dict[i].str, sym)) {
                return relop_dict[i].relop;
            }
        }
        return IR_NOP;
    }

void deref_label(IR *pIR) {
    assert(pIR->type == IR_LABEL);
    pIR->rs->label_ref_cnt--;
    if (pIR->rs->label_ref_cnt == 0) {
        pIR->type = IR_NOP;
        free(pIR->rs);
        pIR->rs = NULL;
    }
}

//
// 压缩指令, 删除NOP
//
int compress_ir(IR instr[], int n) {
    int slow = 0;
    for (int fast = 0; fast < n; fast++) {
        if (instr[fast].type != IR_NOP) {
            instr[slow++] = instr[fast];
        }
    }
    return slow;
}

//
// 预处理 IR
//
void preprocess_ir() {
    IR *pIR = &instr_buffer[0];

    // Label 的引用计数
    for (int i = 0; i < nr_instr; i++) {
        if (is_branch(pIR)) {
            pIR->rd->label_ref_cnt++;
        } else if (pIR->type == IR_JMP) {
            pIR->rs->label_ref_cnt++;
        }
        pIR++;
    }

    // 简单的模式处理: Label true 就在 GOTO false 下面
    // 以及 goto 后面就是对应的 label
    pIR = &instr_buffer[0];
    for (int i = 0; i < nr_instr - 2; i++) {
        if (is_branch(pIR) &&
                (pIR + 1)->type == IR_JMP &&
                (pIR + 2)->type == IR_LABEL &&
                pIR->rd == (pIR + 2)->rs) {
            pIR->type = get_relop_anti(pIR->type);
            deref_label(pIR + 2);
            pIR->rd = (pIR + 1)->rs;
            (pIR + 1)->type = IR_NOP;
        } else if (pIR->type == IR_JMP &&
                (pIR + 1)->type == IR_LABEL &&
                (pIR + 1)->rs == pIR->rs) {
            // goto 后面就是对应的 label
            pIR->type = IR_NOP;
            deref_label(pIR + 1);
        }
        pIR++;
    }

    // 简单的模式处理: 连续 Label 归一
    IR *preLabel = NULL;
    pIR = &instr_buffer[0];
    for (int i = 0; i < nr_instr; i++) {
        if (pIR->type == IR_LABEL && preLabel == NULL) {
            preLabel = pIR;
        } else if (pIR->type != IR_LABEL) {
            preLabel = NULL;
        } else {
            // 连续 Label
            preLabel->rs->label_ref_cnt += pIR->rs->label_ref_cnt;
            replace_operand_global(preLabel->rs, pIR->rs);
            pIR->type = IR_NOP;
            pIR->rs = NULL;
        }
        pIR++;
    }

    // 第一次压缩
    nr_instr = compress_ir(instr_buffer, nr_instr);

    // 标签编号语义化
    pIR = &instr_buffer[0];
    for (int i = 0; i < nr_instr; i++) {
        if (pIR->type == IR_LABEL) {
            pIR->rs->index = i;
        }
        pIR++;
    }
}

//
// 控制流分析工具
// 翻译期的优化我只能进行简单的常数折叠, 以及利用中间指令特性减少不必要的中间变量
// (机器指令生成时估计要遭报应, 就像我一开始偷懒只建立 concrete ast 一样)
// 更多优化还是要靠对指令的直接分析来决定, 毕竟还是不太敢在生成条件表达式时就对省略分支......
// 关于控制流分析的主要知识现在来自 http://www.cs.utexas.edu/users/mckinley/380C
//

//
// 预处理
//   预处理主要干以下工作:
//   1. 将 Label 的编号替换为指令编号, 方便阅读 TODO 确保所有标号都是通过指针传递, 而不是成员赋值转换
//   2. 稍有尝试的人都能看出来:
//        IF cond GOTO L0
//        GOTO L1
//        LABEL L0
//      可以替换成:
//        IF !cond GOTO L1
//      当然可能要确保 L0 的引用只有这一处, 否则还是要保留下 L0 的 TODO 计算所有标签的引用次数
//   3. (可选)将 2 产生的 NOP 通过移动数组删除(那样子标签编号要最后做)
//

//
// 分析基本块: 活跃性分析
// end 不可取
//
void optimize_liveness(int start, int end) {
    for (int i = end - 1; i >= start; i--) {
        IR *ir = &instr_buffer[i];

        // 保存当前状态
        for (int k = 0; k < NR_OPE; k++) {
            if (ir->operand[k]) {
                ir->info[k].liveness = ir->operand[k]->liveness;
                ir->info[k].next_use = ir->operand[k]->next_use;
            }
        }

        if (ir->rd && is_tmp(ir->rd)) {
            ir->rd->liveness = DISALIVE;
            ir->rd->next_use = NO_USE;
        }

        for (int k = 0; k < NR_OPE; k++) {
            if (k != RD_IDX && ir->operand[k] && is_tmp(ir->operand[k])) {
                ir->operand[k]->liveness = ALIVE;
                ir->operand[k]->next_use = i;
            }
        }
    }
}

static void gen_dag_from_instr(IR *pIR) {
    LOG("转换: %s", ir_to_s(pIR));

    Operand rs = pIR->rs;
    Operand rt = pIR->rt;
    Operand rd = pIR->rd;

    if (rs && !rs->dep) {
        LOG("rs: %s新建叶子", print_operand(rs));
        rs->dep = new_leaf(rs);
        add_depend(rs->dep, rs);
    }
    if (rt && !rt->dep) {
        LOG("rt: %s新建叶子", print_operand(rt));
        rt->dep = new_leaf(rt);
        add_depend(rt->dep, rt);
    }

    // 虽然 rd 对应的操作数可能会与 rs / rt 相同
    // 但是我们用于搜索的依据的是 rs / rt 的依赖结点, 如果操作数相同,
    // 依赖结点最后会被更新, 就是另外一个搜索依据了.
    if (rd && !can_jump(pIR)) {
        if (rd->dep) {
            LOG("取消rd: %s的引用", print_operand(rd));
            rd->dep->ref_count--;
            delete_depend(rd);
        }
        if (pIR->type == IR_ASSIGN && rs) {
            LOG("赋值语句, 传递依赖");
            if (is_always_live(rd) && is_tmp(rs) && rs->dep->op == IR_DEREF_R) {
                rd->dep = new_dagnode(rs->dep->op, rs->dep->left, rs->dep->right);  // 右解引用不会被当做公共子表达式
                TEST(0, "我就看看");
            } else {
                rd->dep = rs->dep;
            }
        } else {
            if (pIR->type == IR_CALL || pIR->type == IR_READ) {
                rd->dep = new_dagnode(pIR->type, rs ? rs->dep : NULL, rt ? rt->dep : NULL);
            } else {
                rd->dep = query_dag_node(pIR->type, rs ? rs->dep : NULL, rt ? rt->dep : NULL);
                if (is_always_live(rd) && query_operand_depending_on(rd->dep) && rd->dep->op == IR_DEREF_R) {
                    WARN("夭寿, 依赖到了将被替换的变量(%s)的解引用", print_operand(rd->dep->embody));
                    rd->dep = new_dagnode(rd->dep->op, rd->dep->left, rd->dep->right);
                }
            }
        }
        add_depend(rd->dep, rd);
        pIR->depend = rd->dep;
        if (is_always_live(rd) || pIR->type == IR_CALL || pIR->type == IR_READ) {
            rd->dep->ref_count++;
        }
    } else {
        // 没有目标操作数的指令必须被生成
        pIR->depend = new_dagnode(pIR->type, rs ? rs->dep : NULL, rt ? rt->dep : NULL);
        if (can_jump(pIR)) {
            LOG("加入%s", print_operand(pIR->rd));
            add_depend(pIR->depend, pIR->rd);
        }
        pIR->depend->embody = pIR->rd;
        pIR->depend->ref_count = 1;
    }
}

void gen_dag(IR buf[], int start, int end) {
    init_dag();
    for (int i = start; i < end; i++) {
        gen_dag_from_instr(&buf[i]);
    }
}

int new_dag_ir(IR_Type type, Operand rs, Operand rt, Operand rd) {
    new_instr_(&ir_from_dag[nr_ir_from_dag], type, rs, rt, rd);
    return nr_ir_from_dag++;
}

//
// 将 a := *b 和 a := &b 内联到指令中
// 这会破坏操作数的依赖, 所以必须放到所有优化(包括多趟)之后
//
void log_ir(IR buf[], int start, int end) {
#ifndef DEBUG
    return;
#else
    printf(WARN_COLOR);
    for (int i = start; i < end; i++) {
        print_single_instr(ir_from_dag[i], stdout);
    }
    printf(END);
#endif
}

void inline_replace(IR buf[], int nr) {
    for (int i = 0; i < nr; i++) {
        IR *pir = &buf[i];
        if (pir->rd && is_always_live(pir->rd)) {  // 变量不能被修改!
            continue;
        }
        if (pir->type == IR_DEREF_R) {
            LOG("发现右解引用 %s", ir_to_s(pir));
            pir->type = IR_NOP;
            if (pir->rs->type == OPE_ADDR) {
                pir->rd->type = OPE_DEREF;
            } else if (pir->rs->type == OPE_REFADDR) {
                pir->rd->type = OPE_REF;
            } else {
                PANIC("右解引用的操作数不是地址");
            }
            pir->rd->index = pir->rs->index;
            log_ir(buf, i, nr);
        } else if (pir->type == IR_ADDR) {
            LOG("发现取地址 %s", ir_to_s(pir));
            pir->type = IR_NOP;
            pir->rd->type = OPE_REFADDR;
            pir->rd->index = pir->rs->index;
            log_ir(buf, i, nr);
        }
    }

    // 消除 *&r
    for (int i = 0; i < nr; i++) {
        IR *pir = &buf[i];
        if (pir->rs && pir->rs->type == OPE_REFADDR && pir->type == IR_DEREF_L) {
            LOG("HIT DEREF_L");
            log_ir(buf, i, i + 1);
            TEST(pir->rd == NULL, "DEREF_L没有目标操作数");
            pir->type = IR_ASSIGN;
            pir->rd = new_operand(OPE_NOT_USED);  // 防止对引用变量产生副作用
            pir->rd->type = OPE_REF;
            pir->rd->index = pir->rs->index;
            pir->rs = pir->rt;
            pir->rt = NULL;
        } else if (pir->rs && pir->rs->type == OPE_REFADDR && pir->type == IR_DEREF_R) {
            LOG("HIT DEREF_R");
            log_ir(buf, i, i + 1);
            TEST(pir->rt == NULL, "DEREF_L没有第二个操作数");
            pir->type = IR_ASSIGN;
            Operand ref = new_operand(OPE_NOT_USED);  // 防止对引用变量产生副作用
            ref->type = OPE_REF;
            ref->index = pir->rs->index;
            pir->rs = ref;
        }
    }
}

Operand gen_single_instr_from_dag(pDagNode dag) {
    if (dag == NULL) {
        return NULL;
    }

    if (dag->type == DAG_LEAF) {
        Operand old_init = dag->initial_value;
        Operand current = query_operand_depending_on(dag);
        //if (old_init != current && current !=) {
        if (old_init != current) {
            LOG("原操作数%s已经不保有其初始值", print_operand(old_init));
            if (current == NULL) {
                WARN("预期外的空指针初始值操作数, 检查是否出现了自赋值");
                current = old_init;
            } else {
                LOG("用保存了初始值的%s代替", print_operand(current));
            }

            dag->initial_value = current;
        }
        return old_init;  // 叶结点用于返回初始值
    } else if (dag->type == DAG_OP && !dag->has_gen) {
        dag->embody = query_operand_depending_on(dag);
        if (dag->embody == NULL && (dag->op == IR_ADD || dag->op == IR_MUL || dag->op == IR_SUB || dag->op == IR_DIV)) {
            WARN("FUCK");
            dag->embody = new_operand(OPE_TEMP);
            add_depend(dag, dag->embody);
        }
        new_dag_ir(dag->op, gen_single_instr_from_dag(dag->left), gen_single_instr_from_dag(dag->right), dag->embody);
        dag->has_gen = 1;  // 防止重复生成
        return dag->embody;  // 使用统一的代表操作数, 提供后续优化机会
    } else {
        dag->embody = query_operand_depending_on(dag);
        return dag->embody;
    }
}

void gen_instr_from_dag(int start, int end) {
    for (int i = start; i < end; i++) {
        IR *p = &instr_buffer[i];
        if (p->depend->ref_count > 0 || p->type == IR_CALL || p->type == IR_READ) {
            pDagNode dag;
            if (p->rd && (p->type != IR_CALL && p->type != IR_READ)) {
                dag = query_dagnode_depended_on(p->rd);
            } else {
                dag = p->depend;
                if (!query_operand_depending_on(dag) && p->rd) {
                    add_depend(p->depend, p->rd);
                }
            }
            Operand dst = gen_single_instr_from_dag(dag);
            if (p->rd && is_always_live(p->rd)  && p->rd != dst) {
                new_dag_ir(IR_ASSIGN, dst, NULL, p->rd);
                if (p->depend != query_dagnode_depended_on(p->rd)) {
                    LOG("引用已经改变, 不生成: %s", ir_to_s(&ir_from_dag[nr_ir_from_dag - 1]));
                    nr_ir_from_dag--;
                } else {
                    LOG("弥补变量赋值 No.%d : %s", nr_ir_from_dag, ir_to_s(&ir_from_dag[nr_ir_from_dag - 1]));
                }
            }
        }
    }

    // 清理操作数
    for (int i = start; i < end; i++) {
        IR *p = &instr_buffer[i];
        for (int j = 0; j < 3; j++) {
            if (p->operand[j]) {
                p->operand[j]->dep = NULL;
            }
        }
    }
}

//
// 打印基本块
//
void optimize_in_block() {
    nr_blk = block_partition(blk_buf, instr_buffer, nr_instr);
    for (int i = 0; i < nr_blk; i++) {
        int beg = blk_buf[i].start;
        int end = blk_buf[i].end;
        optimize_liveness(beg, end);

        // Temparally avoid dangerous in-block DAG optimization

        // gen_dag(instr_buffer, beg, end);
        // gen_instr_from_dag(beg, end);
    }
}

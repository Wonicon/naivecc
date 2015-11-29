//
// Created by whz on 15-11-20.
//

#include "ir.h"
#include "dag.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define NAME_LEN 4096

typedef struct Block_ *Block;

typedef struct Block_ {
    int start;
    int end;
    Block *next_true;
    Block *next_false;
} Block_;

static Block_ blk_buf[MAX_LINE];
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

IR ir_from_dag[128];
int nr_ir_from_dag = 0;
extern DagNode dag_buf[];
extern int nr_dag_node;


//
// 操作数构造函数
//
Operand new_operand(Ope_Type type) {
    static int nr_var = 0;
    static int nr_ref = 0;
    static int nr_tmp = 0;
    static int nr_addr = 0;
    static int nr_label = 0;

    Operand p = (Operand)malloc(sizeof(struct Operand_));
    p->type = type;
    switch (type) {
        case OPE_VAR:
            p->liveness = ALIVE;
            p->index = nr_var++;
            break;
        case OPE_REF:
            p->liveness = ALIVE;
            p->index = nr_ref++;
            break;
        case OPE_TEMP:
            p->index = nr_tmp++;
            break;
        case OPE_ADDR:
            p->index = nr_addr++;
            break;
        case OPE_LABEL:
            p->liveness = 1;
            p->label = nr_label++;
            break;
        case OPE_FUNC:
            p->liveness = 1;
            break;
        default:
            break;
    }
    return p;
}

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

//
// 答应操作数, 为了变量和标签工厂函数可以简单实现, 以及方便比较,
// 变量和标签都存储为整型, 在打印操作数的时候加上统一前缀变成合法的变量名
// NOP指令答应为空字符串, 希望将来可以自动过滤.
//
void print_operand(Operand ope, char *str) {
    if (ope == NULL) {
        sprintf(str, "%s", "");
        return;
    }
    switch (ope->type) {
        case OPE_VAR:     sprintf(str, "v%d",  ope->index);    break;
        case OPE_REF:     sprintf(str, "r%d",  ope->index);    break;
        case OPE_FUNC:    sprintf(str, "%s",   ope->name);     break;
        case OPE_TEMP:    sprintf(str, "t%d",  ope->index);    break;
        case OPE_ADDR:    sprintf(str, "a%d",  ope->index);    break;
        case OPE_DEREF:   sprintf(str, "*a%d", ope->index);    break;
        case OPE_FLOAT:   sprintf(str, "#%f",  ope->real);     break;
        case OPE_LABEL:   sprintf(str, "L%d",  ope->label);    break;
        case OPE_INTEGER: sprintf(str, "#%d",  ope->integer);  break;
        case OPE_REFADDR: sprintf(str, "&r%d", ope->index);    break;
        default:          sprintf(str, "%s",   "");
    }
}

static const char *ir_format[] = {
    "",                     // NOP
    "%sLABEL %s :",         // LABEL
    "%sFUNCTION %s :",      // FUNCTION
    "%s := %s",             // ASSIGN
    "%s := %s + %s",        // ADD
    "%s := %s - %s",        // SUB
    "%s := %s * %s",        // MUL
    "%s := %s / %s",        // DIV
    "%s := &%s",            // ADDR
    "%s := *%s",            // DEREF_R
    "%s*%s := %s",          // DEREF_L, 虽然地址在左边, 但是是参数
    "%sGOTO %s",            // JMP
    "IF %s == %s GOTO %s",  // BEQ
    "IF %s < %s GOTO %s",   // BLT
    "IF %s <= %s GOTO %s",  // BLE
    "IF %s > %s GOTO %s",   // BGT
    "IF %s >= %s GOTO %s",  // BGE
    "IF %s != %s GOTO %s",  // BNE
    "%sRETURN %s",          // RETURN
    "%sDEC %s %s",          // DEC, 第一个 %s 过滤 rd_s
    "%sARG %s",             // Pass argument, 第一个 %s 过滤 rd_s
    "%s := CALL %s",        // CALL
    "%sPARAM %s",           // DEC PARAM, 第一个 %s 过滤 rd_s
    "READ %s",              // READ
    "%sWRITE %s",           // WRITE, 第一个 %s 过滤 rd_s, 输出语义不用 rd
};

//
// 单条指令打印函数
//
void print_single_instr(IR instr, FILE *file) {
    print_operand(instr.rd, rd_s);
    print_operand(instr.rs, rs_s);
    print_operand(instr.rt, rt_s);

    // 约定 BEQ 和 BNE 包围所有 Branch 指令
    if (IR_BEQ <= instr.type && instr.type <= IR_BNE) {
        fprintf(file, ir_format[instr.type], rs_s, rt_s, rd_s);  // 交换顺序
    } else if (instr.type == IR_DEC) {  // 规划干不过特例
        fprintf(file, ir_format[instr.type], rd_s, rs_s, rt_s + 1);

    } else {
        fprintf(file, ir_format[instr.type], rd_s, rs_s, rt_s);
    }

    if (instr.type != IR_NOP) {
        fprintf(file, "\n");
    }
}

//
// 打印指令缓冲区中所有的已生成指令
//
void preprocess_ir();
void print_block();
void inline_replace(IR buf[], int nr);
void print_instr(FILE *file)
{
    preprocess_ir();
#ifdef DEBUG
    print_block();
#endif
    for (int i = 0; i < nr_instr; i++) {
        print_single_instr(instr_buffer[i], file);
    }

    inline_replace(ir_from_dag, nr_ir_from_dag);

    FILE *fp = fopen("dag.ir", "w");
    for (int i = 0; i < nr_ir_from_dag; i++) {
        print_single_instr(ir_from_dag[i], fp);
    }
    fclose(fp);
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
bool can_jump(IR *pIR) {
    if (is_branch(pIR)) {
        return true;
    } else {
        switch (pIR->type) {
            case IR_JMP:
            case IR_CALL:
            case IR_WRITE:
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
    for (int i = 0; i < nr_instr; i++) {
        if (instr_buffer[i].type == IR_NOP) {
            for (int j = i; j < nr_instr; j++) {
                instr_buffer[j] = instr_buffer[j + 1];
            }
            nr_instr--;
        }
    }

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
// Leader 检查
//   Leader 是划分 basic block 的依据, 一条指令是 leader, 当且仅当:
//   1. 程序的第一条指令(确定?)
//   2. 跳转类指令的目标(就是 Label)
//   3. 跳转类指令后面的那条指令
//
int is_leader(int ir_idx) {
    return ir_idx == 0 ||
            instr_buffer[ir_idx].type == IR_LABEL ||
            instr_buffer[ir_idx].type == IR_FUNC ||
            instr_buffer[ir_idx].type == IR_READ ||
            can_jump(&instr_buffer[ir_idx - 1]);
}

int is_tmp(Operand ope) {
    if (ope == NULL) {
        return 0;
    } else {
        return ope->type == OPE_ADDR ||
                ope->type == OPE_TEMP ||
                ope->type == OPE_DEREF;
    }
}

//
// 划分基本块
//
void block_partition() {
    nr_blk = 0;
    for (int i = 0; i < nr_instr; i++) {
        if (is_leader(i)) {
            blk_buf[nr_blk].start = i;
            if (nr_blk > 0) {
                blk_buf[nr_blk - 1].end = i;
            }
            nr_blk++;
        }
        instr_buffer[i].block = nr_blk;
    }
    blk_buf[nr_blk - 1].end = nr_instr;
}

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

static void gen_dag_from_instr(IR *pIR)
{
    LOG("转换:");
    print_single_instr(*pIR, stdout);
    Operand rs = pIR->rs;
    Operand rt = pIR->rt;
    Operand rd = pIR->rd;
    if (rs && !rs->dep) {
        LOG("rs新建叶子");
        rs->dep = new_leaf(rs);
    }
    if (rt && !rt->dep) {
        LOG("rt新建叶子");
        rt->dep = new_leaf(rt);
    }

    if (rd) {
        if (pIR->type == IR_ASSIGN && rs) {
            LOG("赋值语句, 传递依赖");
            rd->dep = rs->dep;
        } else {
            rd->dep = query_dag_node(pIR->type, rs ? rs->dep : NULL, rt ? rt->dep : NULL);
            if (rd->dep->op.embody == NULL) {
                rd->dep->op.embody = rd;
            } else {
                LOG("已经有代表操作数");
            }
        }
    }
}

void gen_dag(int start, int end)
{
    init_dag();
    for (int i = start; i < end; i++) {
        gen_dag_from_instr(&instr_buffer[i]);
    }
}

int new_dag_ir(IR_Type type, Operand rs, Operand rt, Operand rd)
{
    new_instr_(&ir_from_dag[nr_ir_from_dag], type, rs, rt, rd);
    return nr_ir_from_dag++;
}

Operand gen_from_dag_(DagNode dag)
{
    if (dag == NULL) {
        return NULL;
    } else if (dag->type == DAG_LEAF) {
        return dag->leaf.initial_value;  // 叶结点用于返回初始值
    } else if (dag->type == DAG_OP && !dag->op.has_gen) {
        new_dag_ir(dag->op.ir_type, gen_from_dag_(dag->op.left), gen_from_dag_(dag->op.right), dag->op.embody);
        dag->op.has_gen = 1;  // 防止重复生成
        return dag->op.embody;  // 使用统一的代表操作数, 提供后续优化机会
    } else {
        return dag->op.embody;
    }
}

//
// 将 a := *b 和 a := &b 内联到指令中
//
void inline_replace(IR buf[], int nr)
{
    for (int i = 0; i < nr; i++) {
        IR *pir = &buf[i];
        if (pir->rd && (pir->rd->type == OPE_VAR || pir->rd->type == OPE_REF)) {  // 变量不能被修改!
            continue;
        }
        if (pir->type == IR_DEREF_R) {
            pir->type = IR_NOP;
            pir->rd->type = OPE_DEREF;
            pir->rd->index = pir->rs->index;
        } else if (pir->type == IR_ADDR) {
            pir->type = IR_NOP;
            pir->rd->type = OPE_REFADDR;
            pir->rd->index = pir->rs->index;
        }
    }
}

void gen_from_dag(int start, int end)
{
    for (int i = start; i < end; i++) {
        IR *p = &instr_buffer[i];
        if (p->rd) {
            gen_from_dag_(p->rd->dep);
            if (p->rd->type == OPE_VAR && p->rd->dep->op.embody != p->rd) {  // 变量出口活跃!?
#ifdef DEBUG
                char strbuf[64];
                print_operand(p->rd, strbuf);
                LOG("%s是变量且不是依赖表达式的代表, 生成赋值语句", strbuf);
#endif
                new_instr_(&ir_from_dag[nr_ir_from_dag++], IR_ASSIGN,
                           p->rd->dep->type == DAG_LEAF ? p->rd->dep->leaf.initial_value : p->rd->dep->op.embody,
                           NULL, p->rd);
            }
        } else {
            new_instr_(&ir_from_dag[nr_ir_from_dag++],
                       p->type,
                       p->rs ? gen_from_dag_(p->rs->dep) : NULL,
                       p->rt ? gen_from_dag_(p->rt->dep) : NULL,
                       NULL);
        }
    }

    for (int i = start; i < end; i++) {
        IR *p = &instr_buffer[i];
        for (int j = 0; j < 3; j++) {
            if (p->operand[j]) {
                p->operand[j]->dep = NULL;
            }
        }
    }
    for (int i = 0; i < nr_dag_node; i++) {
        free(dag_buf[i]);
        dag_buf[i] = NULL;
    }
}

//
// 打印基本块
//
void print_block() {
    block_partition();

    for (int i = 0; i < nr_blk; i++) {
        optimize_liveness(blk_buf[i].start, blk_buf[i].end);
        gen_dag(blk_buf[i].start, blk_buf[i].end);
        gen_from_dag(blk_buf[i].start, blk_buf[i].end);
    }
#if 1
    int current_block = 0;

    int num_buf_sz = 16;
    char num_buf[num_buf_sz];
    char head[] = "####### BLOCK ";
    char tail[] = "######################";
    int tail_len = (int)strlen(tail);

    for (int i = 0; i < nr_instr; i++) {
        IR *pIR = &instr_buffer[i];
        if (pIR->block != current_block) {
            current_block = pIR->block;
            int num_len = sprintf(num_buf, "%d ", current_block);
            printf("%s%s%*s\n", head, num_buf, tail_len - num_len, tail);
        }
        print_single_instr(instr_buffer[i], stdout);
    }
#endif
}
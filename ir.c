#include "ir.h"
#include "operand.h"
#include "basic-block.h"
#include "asm.h"
#include "register.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>


extern FILE *asm_file;  // Stream to store assembly code.


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

//
// 中间代码构造函数
// 返回 IR 在缓冲区中的下标
//
void new_instr_(IR *pIR, IR_Type type, Operand rs, Operand rt, Operand rd)
{
    assert(nr_instr < MAX_LINE);

    pIR->type = type;
    pIR->rs = rs;
    pIR->rt = rt;
    pIR->rd = rd;
}

int new_instr(IR_Type type, Operand rs, Operand rt, Operand rd)
{
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
    [IR_PARAM]   = "%sPARAM %s",           // DEC PARAM, 第一个 %s 过滤 rd_s
    [IR_READ]    = "READ %s",              // READ
    [IR_WRITE]   = "%sWRITE %s",           // WRITE, 第一个 %s 过滤 rd_s, 输出语义不用 rd
};

//
// 单条指令打印函数
//
const char *ir_to_s(IR *pir)
{
    static char buf[120];
    strcpy(rd_s, print_operand(pir->rd));
    strcpy(rs_s, print_operand(pir->rs));
    strcpy(rt_s, print_operand(pir->rt));

    // 约定 BEQ 和 BNE 包围所有 Branch 指令
    if (IR_BEQ <= pir->type && pir->type <= IR_BNE) {
        sprintf(buf, ir_format[pir->type], rs_s, rt_s, rd_s);  // 交换顺序
    }
    else if (pir->type == IR_DEC) {  // 规划干不过特例
        sprintf(buf, ir_format[pir->type], rd_s, rs_s, rt_s + 1);
    }
    else {
        sprintf(buf, ir_format[pir->type], rd_s, rs_s, rt_s);
    }

    return buf;
}

void print_single_instr(IR instr, FILE *file)
{
    fprintf(file, "%s", ir_to_s(&instr));

    if (instr.type != IR_NOP) {
        fprintf(file, "\n");
    }
}


//
// Calculate all variables' offset to the function entry
// Check whether the function has subroutines.
//

#define MAP_SIZE 4096

int in_func_check(IR buf[], int index, int n)
{
    static bool exists[MAP_SIZE];
    static IR *curr = NULL;
    static int param_size;

    TEST(n <= MAP_SIZE, "exceed exists-map size");

    if (index >= n) {
        return 0;  // meaningless
    }

    IR_Type type = buf[index].type;

    if (type == IR_FUNC) {
        memset(exists, 0, sizeof(exists));
        curr = &buf[index];
        curr->rs->size = 0;
        param_size = 0;
    }
    else if (type != IR_PARAM) {
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

        IR_Type type = buf[index].type;

        if (type == IR_CALL || type == IR_READ || type == IR_WRITE) {
            curr->rs->has_subroutine = true;
        }
    }
    else {
        curr->rs->nr_arg++;
        buf[index].rs->is_param = true;
        buf[index].rs->address = - param_size;
        param_size += 4;
        exists[buf[index].rs->index] = true;
    }

    return in_func_check(buf, index + 1, n);
}


//
// 打印指令缓冲区中所有的已生成指令
//
void preprocess_ir();
void optimize_in_block();
int compress_ir(IR buf[], int n);

void print_instr(FILE *file)
{
    // 相当于窥孔优化
    preprocess_ir();

    in_func_check(instr_buffer, 0, nr_instr);

    optimize_in_block();

#ifdef DEBUG
    for (int i = 0; i < nr_instr; i++) {
        print_single_instr(instr_buffer[i], file);
    }
    fclose(file);
#endif

    ////////////////////////////////////////////////////////
    //  Generate assembly code
    ////////////////////////////////////////////////////////

    // Predefined functions

    FILE *predef = fopen("predefine.S", "r");
    char linebuf[128];  // 128 is enough?
    while (fgets(linebuf, 128, predef)) {
        fputs(linebuf, asm_file);
    }

    // Handle each basic block

    for (int i = 0; i < nr_blk; i++) {

        fprintf(asm_file, "#########################\n");
        fprintf(asm_file, "###    basic block    ###\n");
        fprintf(asm_file, "#########################\n");

        Block *blk = &blk_buf[i];

        int j;
        for (j = blk->start; j < blk->end - 1; j++) {
            IR *ir = instr_buffer + j;

            // Update destination's liveness information
            //
            // We may use the destination's next_use field to judge whether it is worth generating.
            // But we should not update the next_use information for the source.
            //
            // Given the case that this instruction is the last one use its rs, indexed by X.
            // The previous next_use information for rs indicates that rs's next referrence is at X.
            // And X stores the updated next_use information indicating that rs is no longer needed.
            //
            // If X uses another source operand which need loading into register, it is highly possible
            // that the currently used source register will be overrided, leading to a wrong result.
            //
            // A more effective solution is dividing the gen_asm into two parts. The 1st part just ensures
            // source operands having been loaded into registers. Then update the next use information.
            // Therefore the destination can still be judged and can use the source operands' register as well

            if (ir->rd) ir->rd->liveness = ir->rd_info.liveness, ir->rd->next_use = ir->rd_info.next_use;

            gen_asm(ir);

            // Update operand information
            if (ir->rs) ir->rs->liveness = ir->rs_info.liveness, ir->rs->next_use = ir->rs_info.next_use;
            if (ir->rt) ir->rt->liveness = ir->rt_info.liveness, ir->rt->next_use = ir->rt_info.next_use;

        }

        // Handle the last IR. We should choose a proper time to spill the value into memory.

        if (can_jump(instr_buffer + j)) {
            push_all();  // jump instr just load data, they don't change data.
            gen_asm(instr_buffer + j);
        }
        else if (instr_buffer[j].type != IR_RET) {
            gen_asm(instr_buffer + j);  // May change variables
            push_all();
        }
        else {
            gen_asm(instr_buffer + j);  // Local variables do not need to store when return
        }

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
int replace_operand(Operand newbie, Operand old, RepOpeMode mode)
{
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
int replace_operand_global(Operand newbie, Operand old)
{
    return replace_operand(newbie, old, REP_SRC | REP_DST | REP_BLK);
}

//
// 检查是否为分支类指令
//
int is_branch(IR *pIR)
{
    return IR_BEQ <= pIR->type && pIR->type <= IR_BNE;
}

//
// 检查是否为跳转类指令
//
bool can_jump(IR *pIR)
{
    if (is_branch(pIR)) {
        return true;
    }
    else if (pIR->type == IR_JMP) {
        return true;
    }
    else {
        return false;
    }
}

//
// relop 字典使用接口
//
const char *get_relop_symbol(IR_Type relop)
{
    for (int i = 0; i < LENGTH(relop_dict); i++) {
        if (relop_dict[i].relop == relop) {
            return relop_dict[i].str;
        }
    }
    return NULL;
}

IR_Type get_relop_anti(IR_Type relop)
{
    for (int i = 0; i < LENGTH(relop_dict); i++) {
        if (relop_dict[i].relop == relop) {
            return relop_dict[i].anti;
        }
    }
    return IR_NOP;
}

IR_Type get_relop(const char *sym)
{
    for (int i = 0; i < LENGTH(relop_dict); i++) {
        if (!strcmp(relop_dict[i].str, sym)) {
            return relop_dict[i].relop;
        }
    }
    return IR_NOP;
}

//
// 控制流分析工具
// 翻译期的优化我只能进行简单的常数折叠
// 更多优化还是要靠对指令的直接分析来决定, 毕竟还是不太敢在生成条件表达式时就对省略分支......
// 关于控制流分析的主要知识现在来自 http://www.cs.utexas.edu/users/mckinley/380C
//

// 对 LABEL 进行引用计数管理
void deref_label(IR *pIR)
{
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
int compress_ir(IR instr[], int n)
{
    int size = 0;
    for (int index = 0; index < n; index++) {
        if (instr[index].type != IR_NOP) {
            instr[size++] = instr[index];
        }
    }
    return size;
}

//
// 预处理
//   预处理主要干以下工作:
//   1. 将 Label 的编号替换为指令编号, 方便阅读
//   2. 将下面的模式:
//        IF cond GOTO L0
//        GOTO L1
//        LABEL L0
//      替换成:
//        IF !cond GOTO L1
//      当然可能要确保 L0 的引用只有这一处, 否则还是要保留 LABEL L0 的
//   3. 将 2 产生的 NOP 通过移动数组删除(那样子标签编号要最后做)
//
void preprocess_ir()
{
    IR *pIR = &instr_buffer[0];

    // Label 的引用计数
    for (int i = 0; i < nr_instr; i++) {
        if (is_branch(pIR)) {
            pIR->rd->label_ref_cnt++;
        }
        else if (pIR->type == IR_JMP) {
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
        }
        else if (pIR->type == IR_JMP &&
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
        }
        else if (pIR->type != IR_LABEL) {
            preLabel = NULL;
        }
        else {
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
}

//
// 分析基本块: 活跃性分析
// end 不可取
//

void optimize_liveness(int start, int end)
{
    // Init
    for (int i = end - 1; i >= start; i--) {
        IR *ir = &instr_buffer[i];

        for (int k = 0; k < NR_OPE; k++) {
            Operand ope = ir->operand[k];

            if (ope == NULL) {
                continue;
            }

            if (ope->type == OPE_VAR || ope->type == OPE_BOOL) {
                ope->liveness = ALIVE;
            }
            else {
                ope->liveness = DISALIVE;
            }

            ope->next_use = MAX_LINE;
        }
    }

    // Iteration
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

//
// 基本块
//
void optimize_in_block()
{
    nr_blk = block_partition(blk_buf, instr_buffer, nr_instr);
    for (int i = 0; i < nr_blk; i++) {
        int beg = blk_buf[i].start;
        int end = blk_buf[i].end;
        optimize_liveness(beg, end);
    }
}


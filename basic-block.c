//
// Created by whz on 15-12-3.
//

#include "basic-block.h"
#include "lib.h"
#include "ir.h"
#include "operand.h"
#include <string.h>

void reset_block(Block block[], int nr_block)
{
    memset(block, 0, sizeof(block[0]) * nr_block);
}

//
// Leader 检查
//   Leader 是划分 basic block 的依据, 一条指令是 leader, 当且仅当:
//   1. 程序的第一条指令(确定?)
//   2. 跳转类指令的目标(就是 Label)
//   3. 跳转类指令后面的那条指令
//
static bool is_leader(IR instr[], int ir_idx)
{
    return ir_idx == 0 ||
           instr[ir_idx].type == IR_LABEL ||
           instr[ir_idx].type == IR_FUNC ||
           can_jump(&instr[ir_idx - 1]);
}

//
// 划分基本块
//
int block_partition(Block block[], IR instr[], int n)
{
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (is_leader(instr, i)) {
            block[count].start = i;
            block[count].index = count;
            if (count > 0) {
                block[count - 1].end = i;
            }
            count++;
        }
        instr[i].block = count - 1;
    }
    block[count - 1].end = n;
    return count;
}

// 找到需要的LABEL, 返回LABEL所在指令缓冲的下标
static int find_label(Operand label, IR instr[], int nr_instr)
{
    if (label == NULL) {
        return nr_instr;
    }

    for (int i = 0; i < nr_instr; i++) {
        IR *ir = &instr[i];
        if (ir->type == IR_LABEL && cmp_operand(ir->rs, label)) {
            return i;
        }
    }
    return nr_instr;
}

void construct_cfg(Block block[], int nr_block, IR instr[], int nr_instr)
{
    for (int idx_blk = 0; idx_blk < nr_block; idx_blk++) {
        Block *blk = &block[idx_blk];

        bool return_flag = false;
        for (int i = blk->end - 1; i >= blk->start; i--) {
            if (instr[i].type == IR_RET) {
                blk->follow = blk->branch = -1;
                return_flag = true;
            }
        }
        if (return_flag) {
            continue;
        }

        // 默认下落, 存储的是block的下标
        blk->follow = idx_blk + 1;
        blk->branch = idx_blk + 1;

        // 只需要检查最后一条指令
        IR *ir = &instr[blk->end - 1];

        // 获取LABEL
        Operand label;
        if (is_branch(ir)) {
            label = ir->rd;
        }
        else if (ir->type == IR_JMP) {
            label = ir->rs;
        }
        else {
            label = NULL;
        }

        // 搜索LABEL对应下标
        int idx_label = find_label(label, instr, nr_instr);
        if (idx_label < nr_instr) {
            LOG("基本块%d找到跳转目标%s", idx_blk, print_operand(label));
            blk->branch = instr[idx_label].block;
            if (ir->type == IR_JMP) {
                blk->follow = blk->branch;
            }
        }

    }
}

void cfg_to_dot(const char *filename, Block block[], int nr_block)
{
    FILE *fp = fopen(filename, "w");
    fprintf(fp, "digraph cfg {\n");
    for (int i = 0; i < nr_block; i++) {
        Block *blk = &block[i];
        fprintf(fp, "%*sb%d [label=\"Block %d from %d to %d\"];\n", 4, "", blk->index, blk->index, blk->start + 1, blk->end);
    }
    for (int i = 0; i < nr_block; i++) {
        Block *blk = &block[i];

        if (blk->follow == -1) {
            continue;
        }

        fprintf(fp, "%*sb%d -> b%d;\n", 4, "", blk->index, blk->follow);
        if (blk->follow != blk->branch) {
            fprintf(fp, "%*sb%d -> b%d;\n", 4, "", blk->index, blk->branch);
        }
    }
    fprintf(fp, "}\n");
    fclose(fp);
}

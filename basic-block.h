//
// Created by whz on 15-12-3.
//

#ifndef NJU_COMPILER_2015_BASIC_BLOCK_H
#define NJU_COMPILER_2015_BASIC_BLOCK_H

#include "ir.h"

typedef struct Block {
    int index;
    int start;
    int end;
    union {
        struct {
            int follow;
            int branch;
        };
        int next[2];
    };
} Block;

void reset_block(Block block[], int nr_block);

int block_partition(Block block[], IR instr[], int n);

void construct_cfg(Block block[], int nr_block, IR instr[], int nr_instr);

void cfg_to_dot(const char *filename, Block block[], int nr_block);

#endif //NJU_COMPILER_2015_BASIC_BLOCK_H

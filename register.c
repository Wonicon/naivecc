//
// Created by whz on 15-12-24.
//

#include "register.h"
#include "asm.h"
#include "lib.h"
#include "ir.h"
#include "operand.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


extern FILE *asm_file;


enum reg {
    ZERO,
    AT,
    V0, V1,
    A0, A1, A2, A3,
    T0, T1, T2, T3, T4, T5, T6, T7,
    S0, S1, S2, S3, S4, S5, S6, S7,
    T8, T9,
    K0, k1,
    GP,
    SP,
    S8,
    RA
};


const char *reg_s[] = {
    "$zero",                                                  // $0
    "$at",                                                    // $1
    "$v0", "$v1",                                             // $2  ~ $3
    "$a0", "$a1", "$a2", "$a3",                               // $4  ~ $7
    "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",   // $8  ~ $15
    "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",   // $16 ~ $23
    "$t8", "$t9",                                             // $24 ~ $25
    "$k0", "$k1",                                             // $26 ~ $27
    "$gp",                                                    // $28
    "$sp",                                                    // $29
    "$s8",                                                    // $30
    "$ra"                                                     // $31
};


#define NR_REG (sizeof(reg_s) / sizeof(reg_s[0]))


#define NR_SAVE ((int)(S7 - S0))

#define MAX_VAR 4096


int sp_offset = 0;  // Always positive, [-n]($fp) == [offset - n]($sp) where offset == $fp - $sp


Operand ope_in_reg[NR_REG];  // Record which register stores which operand, null if none.


int dirty[NR_REG];  // True if $sx is written


void set_dirty(int index)
{
    dirty[index] = 1;
}


const char *reg_to_s(int index)
{
    return reg_s[index];
}


int get_reg(int start, int end)  // [start, end]
{
    int victim = MAX_LINE;           // The one to be replaced
    int victim_next_use = -1;  // The limit of instruction buffer

    int i;  // Need to use the break index

    for (i = start; i <= end; i++) {
        Operand ope = ope_in_reg[i];

        if (ope == NULL) {  // An empty register
            break;
        }
        else if (victim_next_use < ope->next_use) {
            victim = i;
            victim_next_use = ope->next_use;
        }
    }

    if (i <= end) {  // Find empty register
        return i;
    }
    else {
        TEST(start <= victim && victim <= end && ope_in_reg[victim], "Victim should be updated");
        Operand vic = ope_in_reg[victim];
        if (vic->next_use != MAX_LINE || vic->liveness) {
            if (vic->type == OPE_TEMP || vic->type == OPE_ADDR) {
                WARN("Back up temporary variable");
            }
            emit_asm(sw, "%s, %d($sp)  # Back up victim", reg_s[victim], sp_offset - ope_in_reg[victim]->address);
        }
        ope_in_reg[victim] = NULL;
        return victim;
    }
}


//
// Allocate a register to the given register.
//
// Registers are selected by the follow priority:
//   1. Empty register
//   2. Register with a value that is the least currently needed.
//

void remove_value(Operand ope)
{
    for (int i = 0; i <= NR_REG; i++) {
        if (ope_in_reg[i] == ope) {
            ope_in_reg[i] = NULL;
        }
    }
}


int allocate(Operand ope)
{
    TEST(ope, "Operand is null");

    int reg;
    
    remove_value(ope);  // Must remove ope's value stored in register, otherwise `ensure' will return old one.

    // Select a suitable register group
    switch (ope->type) {
    case OPE_VAR:
    case OPE_BOOL:
        reg = get_reg(S0, S7);
        break;
    case OPE_TEMP:
    case OPE_ADDR:
    case OPE_INTEGER:
        reg = get_reg(T0, T7);
        break;
    default:
        PANIC("Unexpected operand type when allocating registers");
    }

    LOG("Allocate %s to register %s", print_operand(ope), reg_to_s(reg));
    ope_in_reg[reg] = ope;

    return reg;
}


//
// Ensure an operand's value is in a register,
// otherwise emit a load instruction
//

int ensure(Operand ope)
{
    TEST(ope, "Operand is null");

    for (int i = 0; i < NR_REG; i++) {
        if (ope_in_reg[i] && cmp_operand(ope, ope_in_reg[i])) {
            LOG("Find %s at %s", print_operand(ope), reg_to_s(i));
            return i;
        }
    }

    int result = allocate(ope);  // The reg name string to be printed.

    if (is_const(ope)) {
        emit_asm(li, "%s, %d", reg_s[result], ope->integer); // Jump '#' required by ir
    }
    else {
        emit_asm(lw, "%s, %d($sp)  # sp_offset %d addr %d",
                reg_s[result], sp_offset - ope->address, sp_offset, ope->address);
    }

    return result;
}


//
// Push variables and bools onto stak
//
// Address and temp are live only in one basic block
//

void push_all()
{
    for (int i = 0; i < NR_REG; i++) {
        Operand ope = ope_in_reg[i];
        if (ope != NULL && dirty[i] && (ope->next_use != MAX_LINE || ope->liveness)) {
            // Use next_use to avoid store dead temporary variables.
            // Use liveness to promise that user-defined variables are backed up.
            emit_asm(sw, "%s, %d($sp)  # push %s", reg_s[i], sp_offset - ope->address, print_operand(ope));
        }
    }
}


//
// Clear the operands' value stored in registers
//
// This function should be called at the end of a basic block.
//

void clear_reg_state()
{
    memset(ope_in_reg, 0, sizeof(ope_in_reg));
    memset(dirty, 0, sizeof(dirty));
}


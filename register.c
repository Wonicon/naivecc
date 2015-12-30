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


#define MAX_VAR 4096


int sp_offset = 0;  // Always positive, [-n]($fp) == [offset - n]($sp) where offset == $fp - $sp


Operand ope_in_reg[NR_REG];  // Record which register stores which operand, null if none.


int arg_reg_idx = A0;


int get_reg(int start, int end)  // [start, end]
{
    int victim = MAX_LINE;           // The one to be replaced
    int victim_next_use = -1;  // The limit of instruction buffer

    int i;  // Need to use the break index

    for (i = start; i <= end; i++) {
        AUTO(ope, ope_in_reg[i]);

        if (ope == NULL) {
            // An empty register or register store useless value
            break;
        } else if (victim_next_use < ope->next_use) {
            victim = i;
            victim_next_use = ope->next_use;
        }
    }

    if (i <= end) {  // Find empty register
        return i;
    } else {
        TEST(start <= victim && victim <= end && ope_in_reg[victim], "Victim should be updated");
        AUTO(vic, ope_in_reg[victim]);
        if (vic->next_use != MAX_LINE) {
            if (vic->type == OPE_TEMP || vic->type == OPE_ADDR) {
                WARN("Back up temporary variable");
            }
            emit_asm(sw, "%s, %d($sp)  # Back up victim", reg_s[victim], ope_in_reg[victim]->address);
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


const char *allocate(Operand ope)
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

    ope_in_reg[reg] = ope;

    return reg_s[reg];
}


//
// Ensure an operand's value is in a register,
// otherwise emit a load instruction
//

const char *ensure(Operand ope)
{
    TEST(ope, "Operand is null");

    for (int i = 0; i < NR_REG; i++) {
        if (ope_in_reg[i] && cmp_operand(ope, ope_in_reg[i])) {
            return reg_s[i];
        }
    }

    const char *result = allocate(ope);  // The reg name string to be printed.

    if (is_const(ope)) {
        emit_asm(li, "%s, %s", result, print_operand(ope) + 1); // Jump '#' required by ir
    } else {
        emit_asm(lw, "%s, %d($sp)  # sp_offset %d addr %d",
                result, sp_offset - ope->address, sp_offset, ope->address);
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
    for (int i = S0; i <= S7; i++) {
        AUTO(ope, ope_in_reg[i]);
        if (ope != NULL) {
            emit_asm(sw, "%s, %d($sp)  # push %s", reg_s[i], sp_offset - ope->address, print_operand(ope));
        }
    }
}


//
// Clear the key-value map
//
// This function should be called at the end of a basic block,
//

void clear_reg_state()
{
    for (int i = ZERO; i < A0; i++) {
        ope_in_reg[i] = NULL;
    }

    for (int i = T0; i < RA; i++) {
        ope_in_reg[i] = NULL;
    }
}


//
// Clear register for a function
//

void clear_reg_state_in_function()
{
    memset(ope_in_reg, 0, sizeof(ope_in_reg));
    arg_reg_idx = A0;
}


//
// Store the first 4 arguments
//

#if 0
void pass_arg(Operand ope)
{
    if (arg_reg_idx <= A3) {
        ope_in_reg[arg_reg_idx++] = ope;
    }
}
#endif


//
// Flush arguments in $a0 ~ $a3
//

void flush_arg(Operand ope)
{
    for (int i = A0; i <= A3; i++) {
        AUTO(arg, ope_in_reg[i]);
        if (arg != NULL) {
            TEST(arg->is_param, "A0 ~ A3 should only be param");
            emit_asm(sw, "%s, %d($sp)", reg_s[i], arg->address);
        }
    }
}


//
// Retrieve the first 4 arguments
//

void retrieve_arg(Operand ope)
{
}

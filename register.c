//
// Created by whz on 15-12-24.
//

#include "register.h"
#include "lib.h"
#include "ir.h"
#include "operand.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


extern FILE *asm_file;


const char *register_name[] = {
    "$zero",
    "$at",
    "$v0", "$v1",
    "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "%t6", "$t7",
    "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
    "$t8", "$t9",
    "$k0", "$k1",
    "$gp", "$sp", "$s8", "$ra"
};


typedef struct RegVarPair *pRegVarPair;
typedef struct RegVarPair
{
    Operand ope;
    int reg_index;
    pRegVarPair prev;
    pRegVarPair next;
} RegVarPair, *pRegVarPair;


RegVarPair header = { NULL, -1, NULL, NULL };
pRegVarPair reg_state = &header;


//
// Allocate a register to the given register.
// Registers are preferred by the follow priority:
//   1. Empty register
//   2. Register with a value that is the least currently needed.
//

char *allocate(Operand ope)
{
    TEST(ope, "ope is null");
    static int index = 1;
    pRegVarPair p = malloc(sizeof(RegVarPair));
    memset(p, 0, sizeof(RegVarPair));
    p->ope = ope;
    p->reg_index = index++;
    p->prev = reg_state;
    p->next = reg_state->next;
    reg_state->next = p;

    char *temp = malloc(32);
    sprintf(temp, "$t%d", p->reg_index);
    return temp;
}


//
// Ensure an operand's value is in a register,
// otherwise emit a load instruction
//

char *ensure(Operand ope)
{
    TEST(ope, "ope is null");
    for (pRegVarPair p = reg_state->next; p != NULL; p = p->next) {
        if (cmp_operand(ope, p->ope)) {
            // Just leak the memory
            char *temp = malloc(32);
            sprintf(temp, "$t%d", p->reg_index);
            return temp;
            //return register_name[reg_state.buffer[i].reg_index];
        }
    }

    char *result = allocate(ope);  // The reg name string to be printed.

    if (is_const(ope)) {
        WARN("Allocate a register to a const number");
        emit_asm(li, "%s, %s", result, print_operand(ope) + 1); // Jump '#' required by ir
    } else {
        // TODO make offset accurate
        emit_asm(lw, "%s, %d($sp)", result, ope->address);
    }

    return result;
}


//
// Clear the key-value map
//
// This function should be called at the beginning or the end of a basic block,
// as well as all register values being pushed into the stack
//

void clear_reg_state()
{
    pRegVarPair curr = reg_state->next;

    while (curr != NULL) {
        pRegVarPair tmp = curr;
        curr = curr->next;
        // Push to stack.
        emit_asm(sw, "%s, %d($sp)", print_operand(tmp->ope), tmp->ope->address);
        free(tmp);
    }

    reg_state->next = NULL;
}

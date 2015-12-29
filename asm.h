//
// Created by whz on 15-12-24.
//

#ifndef NJU_COMPILER_2015_ASM_H
#define NJU_COMPILER_2015_ASM_H

#include "ir.h"

void gen_asm(IR *ir);

extern Operand curr_func;

// Common asm print format
#define emit_asm(instr, format, ...) \
    fprintf(asm_file, "  %-*s" format "\n", 7, str(instr), ## __VA_ARGS__)

#endif //NJU_COMPILER_2015_ASM_H

//
// Created by whz on 15-12-24.
//

#ifndef NJU_COMPILER_2015_ASM_H
#define NJU_COMPILER_2015_ASM_H

#include "ir.h"
#include "stdio.h"

void gen_asm(IR *ir);

extern FILE *asm_file;  // Store the final assembly code

extern FILE *func_file; // Store the temporary assembly code

extern Operand curr_func;

extern int sp_offset;

// Common asm print format
#define emit_asm(instr, format, ...) \
    fprintf(asm_file, "  %-*s" format "\n", 7, str(instr), ## __VA_ARGS__)

#endif //NJU_COMPILER_2015_ASM_H

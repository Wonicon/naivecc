//
// Created by whz on 15-12-24.
//
// The module to translate intermediate representation to assembly code
// Architecture: MIPS32
//

#include "asm.h"
#include "register.h"
#include "ir.h"
#include "lib.h"
#include "operand.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>


extern FILE *asm_file;

Operand curr_func = NULL;

void gen_asm_label(IR *ir)
{
    fprintf(asm_file, "%s:\n", print_operand(ir->rs));
    if (ir->type == IR_FUNC) {
        // Spare stack space
        curr_func = ir->rs;
        sp_offset = curr_func->size;
        emit_asm(addi, "$sp, $sp, %d  # only for variables, not records", -ir->rs->size);
    }
}


void gen_asm_assign(IR *ir)
{
    char *src = ensure(ir->rs);
    char *dst = allocate(ir->rd);
    emit_asm(move, "%s, %s", dst, src);
}


// ir->rt should be OPE_INTEGER
static inline void gen_asm_addi(Operand dst, Operand src, int imm)
{
    char *first = ensure(src);
    char *dest = allocate(dst);
    emit_asm(addi, "%s, %s, %d", dest, first, imm);
}


void gen_asm_add(IR *ir)
{
    if (ir->rt->type == OPE_INTEGER) {
        gen_asm_addi(ir->rd, ir->rs, ir->rt->integer);
    } else if (ir->rs->type == OPE_INTEGER) {
        gen_asm_addi(ir->rd, ir->rt, ir->rs->integer);
    } else {
        char *first = ensure(ir->rs);
        char *second = ensure(ir->rt);
        char *dst = allocate(ir->rd);
        emit_asm(add, "%s, %s, %s", dst, first, second);
    }
}


void gen_asm_sub(IR *ir)
{
    if (ir->rt->type == OPE_INTEGER) {
        gen_asm_addi(ir->rd, ir->rs, -ir->rt->integer);
    } else if (ir->rs->type == OPE_INTEGER) {
        gen_asm_addi(ir->rd, ir->rt, -ir->rs->integer);
    } else {
        char *first = ensure(ir->rs);
        char *second = ensure(ir->rt);
        char *dst = allocate(ir->rd);
        emit_asm(sub, "%s, %s, %s", dst, first, second);
    }
}


void gen_asm_mul(IR *ir)
{
    char *y = ensure(ir->rs);
    char *z = ensure(ir->rt);
    char *x = allocate(ir->rd);
    emit_asm(mul, "%s, %s, %s", x, y, z);
}


void gen_asm_div(IR *ir)
{
    char *y = ensure(ir->rs);
    char *z = ensure(ir->rt);
    char *x = allocate(ir->rd);
    emit_asm(div, "%s, %s", y, z);
    emit_asm(mflo, "%s", x);
}


void gen_asm_load(IR *ir)
{
    char *y = ensure(ir->rs);
    char *x = allocate(ir->rd);
    emit_asm(lw, "%s, 0(%s)", x, y);
}


void gen_asm_store(IR *ir)
{
    char *y = ensure(ir->rt);
    char *x = ensure(ir->rs);
    emit_asm(sw, "%s, 0(%s)", y, x);
}


void gen_asm_goto(IR *ir)
{
    emit_asm(j, "%s", print_operand(ir->rs));
}


void gen_asm_call(IR *ir)
{
    emit_asm(sw, "sw $ra, 0($sp)");  // ?
    emit_asm(jal, "%s", ir->rs->name);
    char *x = allocate(ir->rd);
    emit_asm(move, "$v0, %s", x);
}


void gen_asm_return(IR *ir)
{
    char *x = ensure(ir->rs);
    emit_asm(addiu, "$sp, $sp, %d  # release stack space", curr_func->size);
    emit_asm(move, "%s, $v0  # prepare return value", x);
    emit_asm(jr, "$ra");
}


void gen_asm_br(IR *ir)
{
    char *x = ensure(ir->rs);
    char *y = ensure(ir->rt);
    switch (ir->type) {
        case IR_BEQ: emit_asm(beq, "%s, %s, %s", x, y, print_operand(ir->rd)); break;
        case IR_BNE: emit_asm(bne, "%s, %s, %s", x, y, print_operand(ir->rd)); break;
        case IR_BGT: emit_asm(bgt, "%s, %s, %s", x, y, print_operand(ir->rd)); break;
        case IR_BLT: emit_asm(blt, "%s, %s, %s", x, y, print_operand(ir->rd)); break;
        case IR_BGE: emit_asm(bge, "%s, %s, %s", x, y, print_operand(ir->rd)); break;
        case IR_BLE: emit_asm(ble, "%s, %s, %s", x, y, print_operand(ir->rd)); break;
        default: assert(0);
    }
}


void gen_asm_dec(IR *ir)
{
    return;
}


void gen_asm_addr(IR *ir)
{
    char *x = allocate(ir->rd);
    emit_asm(addiu, "%s, $sp, %d  # get %s's address", x, sp_offset - ir->rs->address, print_operand(ir->rs));
}


typedef void(*trans_handler)(IR *);

trans_handler handler[NR_IR_TYPE] = {
    [IR_DEC]     = gen_asm_dec,
    [IR_FUNC]    = gen_asm_label,
    [IR_ASSIGN]  = gen_asm_assign,
    [IR_ADD]     = gen_asm_add,
    [IR_SUB]     = gen_asm_sub,
    [IR_MUL]     = gen_asm_mul,
    [IR_DIV]     = gen_asm_div,
    [IR_LABEL]   = gen_asm_label,
    [IR_DEREF_R] = gen_asm_load,
    [IR_DEREF_L] = gen_asm_store,
    [IR_JMP]     = gen_asm_goto,
    [IR_CALL]    = gen_asm_call,
    [IR_ADDR]    = gen_asm_addr,
    [IR_RET]     = gen_asm_return,
    [IR_BEQ]     = gen_asm_br,
    [IR_BLE]     = gen_asm_br,
    [IR_BGT]     = gen_asm_br,
    [IR_BGE]     = gen_asm_br,
    [IR_BNE]     = gen_asm_br,
    [IR_BLT]     = gen_asm_br
};

void gen_asm(IR *ir)
{
    handler[ir->type](ir);
}

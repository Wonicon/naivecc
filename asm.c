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


FILE *asm_file  = NULL;  // Store the final assembly code

FILE *func_file = NULL;  // Store the temporary assembly code

Operand curr_func = NULL;


int nr_arg = 0;  // The number of arguments have been encounterded, referred when translating call


void gen_asm_label(IR *ir)
{
    fprintf(asm_file, "%s:\n", print_operand(ir->rs));
}


void gen_asm_func(IR *ir)
{
    fprintf(asm_file, "%s:\n", print_operand(ir->rs));
    // Spare stack space
    curr_func = ir->rs;
    sp_offset = curr_func->size;

    int ra = curr_func->has_subroutine ? 4 : 0;

    emit_asm(addi, "$sp, $sp, %d  # only for variables, not records", -ir->rs->size - ra);

    if (curr_func->has_subroutine) {
        emit_asm(sw, "$ra, %d($sp)  # Save return address", sp_offset);
    }
}


void gen_asm_assign(IR *ir)
{
    int src = ensure(ir->rs);
    int dst = allocate(ir->rd);
    set_dirty(dst);
    emit_asm(move, "%s, %s", reg_to_s(dst), reg_to_s(src));
}


// ir->rt should be OPE_INTEGER
void gen_asm_addi(Operand dst, Operand src, int imm)
{
    int first = ensure(src);
    int dest = allocate(dst);
    set_dirty(dest);
    emit_asm(addi, "%s, %s, %d", reg_to_s(dest), reg_to_s(first), imm);
}


void gen_asm_add(IR *ir)
{
    if (ir->rt->type == OPE_INTEGER) {
        gen_asm_addi(ir->rd, ir->rs, ir->rt->integer);
    }
    else if (ir->rs->type == OPE_INTEGER) {
        gen_asm_addi(ir->rd, ir->rt, ir->rs->integer);
    }
    else {
        int first = ensure(ir->rs);
        int second = ensure(ir->rt);
        int dst = allocate(ir->rd);
        set_dirty(dst);
        emit_asm(add, "%s, %s, %s", reg_to_s(dst), reg_to_s(first), reg_to_s(second));
    }
}


void gen_asm_sub(IR *ir)
{
    // Note, sub cannot exchange!

    if (ir->rt->type == OPE_INTEGER) {
        gen_asm_addi(ir->rd, ir->rs, -ir->rt->integer);
    }
    else {
        int first = ensure(ir->rs);
        int second = ensure(ir->rt);
        int dst = allocate(ir->rd);
        set_dirty(dst);
        emit_asm(sub, "%s, %s, %s", reg_to_s(dst), reg_to_s(first), reg_to_s(second));
    }
}


void gen_asm_mul(IR *ir)
{
    int y = ensure(ir->rs);
    int z = ensure(ir->rt);
    int x = allocate(ir->rd);
    set_dirty(x);
    emit_asm(mul, "%s, %s, %s", reg_to_s(x), reg_to_s(y), reg_to_s(z));
}


void gen_asm_div(IR *ir)
{
    int y = ensure(ir->rs);
    int z = ensure(ir->rt);
    int x = allocate(ir->rd);
    set_dirty(x);
    emit_asm(div, "%s, %s", reg_to_s(y), reg_to_s(z));
    emit_asm(mflo, "%s", reg_to_s(x));
}


void gen_asm_load(IR *ir)
{
    int y = ensure(ir->rs);
    int x = allocate(ir->rd);
    set_dirty(x);
    emit_asm(lw, "%s, 0(%s)", reg_to_s(x), reg_to_s(y));
}


void gen_asm_store(IR *ir)
{
    int y = ensure(ir->rt);
    int x = ensure(ir->rs);
    emit_asm(sw, "%s, 0(%s)", reg_to_s(y), reg_to_s(x));
}


void gen_asm_goto(IR *ir)
{
    emit_asm(j, "%s", print_operand(ir->rs));
}


void gen_asm_arg(IR *ir)  // Not really emit code, but update the state.
{
    nr_arg++;
}


void gen_asm_call(IR *ir)
{
    // Open space for used save registers and arguments

    int offset = nr_arg * 4;
    emit_asm(addi, "$sp, $sp, -%d  # Open space for save and arguments", offset);

    sp_offset += offset;

    push_all();

    IR *arg = ir;  // IR is stored consecutively


    // Push all arguments onto stack, which is more like x86 ;-)
    for (int i = 1; i <= nr_arg; i++) {

        do { arg--; } while (arg->type != IR_ARG);  // ARG may not be consecutive,
                                                    // so we use a iteration to find the first ARG
                                                    // before the current IR.
                                                    // It is expected that there always have enough
                                                    // ARG IRs that match [nr_arg]

        int y = ensure(arg->rs);
        emit_asm(sw, "%s, %d($sp)", reg_to_s(y), (i - 1) * 4);

    }

    emit_asm(jal, "%s", ir->rs->name);

    clear_reg_state();

    int x = allocate(ir->rd);
    set_dirty(x);

    if (ir->rd->next_use != MAX_LINE || ir->rd->liveness) {
        emit_asm(move, "%s, $v0", reg_to_s(x));
    }

    emit_asm(addiu, "$sp, $sp, %d  # Drawback save and arguments space", offset);

    sp_offset -= offset;

    nr_arg = 0;  // After translating the call, clear arg state
}


//
// Update parameter's address
// Now we know whether the function has saved return address
// So we can fix the address of parameters.
//

void gen_asm_param(IR *ir)
{
    if (curr_func->has_subroutine) {
        ir->rs->address -= 4;
    }
}


void gen_asm_return(IR *ir)
{
    if (curr_func->has_subroutine) {
        emit_asm(lw, "$ra, %d($sp)  # retrieve return address", sp_offset);
    }
    
    int x = ensure(ir->rs);

    int size = curr_func->has_subroutine ? curr_func->size + 4 : curr_func->size;
    emit_asm(addiu, "$sp, $sp, %d  # release stack space", size);
    emit_asm(move, "$v0, %s  # prepare return value", reg_to_s(x));
    emit_asm(jr, "$ra");
}


void gen_asm_br(IR *ir)
{
    int x = ensure(ir->rs);
    int y = ensure(ir->rt);
    switch (ir->type) {
        case IR_BEQ: emit_asm(beq, "%s, %s, %s", reg_to_s(x), reg_to_s(y), print_operand(ir->rd)); break;
        case IR_BNE: emit_asm(bne, "%s, %s, %s", reg_to_s(x), reg_to_s(y), print_operand(ir->rd)); break;
        case IR_BGT: emit_asm(bgt, "%s, %s, %s", reg_to_s(x), reg_to_s(y), print_operand(ir->rd)); break;
        case IR_BLT: emit_asm(blt, "%s, %s, %s", reg_to_s(x), reg_to_s(y), print_operand(ir->rd)); break;
        case IR_BGE: emit_asm(bge, "%s, %s, %s", reg_to_s(x), reg_to_s(y), print_operand(ir->rd)); break;
        case IR_BLE: emit_asm(ble, "%s, %s, %s", reg_to_s(x), reg_to_s(y), print_operand(ir->rd)); break;
        default: assert(0);
    }
}


void gen_asm_dec(IR *ir)
{
    return;
}


void gen_asm_addr(IR *ir)
{
    int x = allocate(ir->rd);
    set_dirty(x);
    emit_asm(addiu, "%s, $sp, %d  # get %s's address", reg_to_s(x), sp_offset - ir->rs->address, print_operand(ir->rs));
}


void gen_asm_write(IR *ir)
{
    int x = ensure(ir->rs);
    emit_asm(move, "$a0, %s", reg_to_s(x));
    emit_asm(jal, "write");
}


void gen_asm_read(IR *ir)
{
    int x = allocate(ir->rd);
    set_dirty(x);
    emit_asm(jal, "read");
    emit_asm(move, "%s, $v0", reg_to_s(x));
}


typedef void(*trans_handler)(IR *);

trans_handler handler[NR_IR_TYPE] = {
    [IR_DEC]     = gen_asm_dec,
    [IR_FUNC]    = gen_asm_func,
    [IR_ASSIGN]  = gen_asm_assign,
    [IR_ADD]     = gen_asm_add,
    [IR_SUB]     = gen_asm_sub,
    [IR_MUL]     = gen_asm_mul,
    [IR_DIV]     = gen_asm_div,
    [IR_LABEL]   = gen_asm_label,
    [IR_DEREF_R] = gen_asm_load,
    [IR_DEREF_L] = gen_asm_store,
    [IR_JMP]     = gen_asm_goto,
    [IR_ARG]     = gen_asm_arg,
    [IR_CALL]    = gen_asm_call,
    [IR_ADDR]    = gen_asm_addr,
    [IR_READ]    = gen_asm_read,
    [IR_WRITE]   = gen_asm_write,
    [IR_PARAM]   = gen_asm_param,
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
    fprintf(asm_file, "# %s\n", ir_to_s(ir));
    handler[ir->type](ir);
}


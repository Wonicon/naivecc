//
// Created by whz on 15-11-18.
//

#include "translate.h"
#include <stdio.h>
#define MAX_LINE 2048

// static struct IR instr[MAX_LINE];
// int nr_instr;

static char rs_s[MAX_LINE];
static char rt_s[MAX_LINE];
static char rd_s[MAX_LINE];

void print_operand(Operand ope, char *str) {
    switch (ope->type) {
        case OPE_INTEGER:  sprintf(str, "#%d", ope->var.integer); break;
        case OPE_FLOAT:    sprintf(str, "#%f", ope->var.real); break;
        case OPE_LABEL:    sprintf(str, "%s", ope->var.label); break;
        case OPE_VARIABLE: sprintf(str, "var%d", ope->var.index); break;
        default: sprintf(str, "%s", "");
    }
}

const char *ir_format[] = {
    "LABEL %s :",           // LABEL
    "FUNCTION %s :",        // FUNCTION
    "%s := %s",             // ASSIGN
    "%s := %s + %s",        // ADD
    "%s := %s - %s",        // SUB
    "%s := %s * %s",        // MUL
    "%s := %s / %s",        // DIV
    "%s := &%s",            // ADDR
    "%s := *%s",            // DEREF_R
    "*%s := %s",            // DEREF_L
    "GOTO %s",              // JMP
    "IF %s == %s GOTO %s",  // BEQ
    "IF %s < %s GOTO %s",   // BLT
    "IF %s <= %s GOTO %s",  // BLE
    "IF %s > %s GOTO %s",   // BGT
    "IF %s >= %s GOTO %s",  // BGE
    "IF %s != %s GOTO %s",  // BNE
    "RETURN %s",            // RETURN
    "%sDEC %s [%s]",        // MALLOC, 第一个 %s 过滤 rd_s
    "%sARG %s",             // Pass argument, 第一个 %s 过滤 rd_s
    "%s := CALL %s",        // CALL
    "%sPARAM %s",           // DEC PARAM, 第一个 %s 过滤 rd_s
    "READ %s",              // READ
    "%sWRITE %s",           // WRITE, 第一个 %s 过滤 rd_s, 输出语义不用 rd
    ""                      // NOP
};

void preprint_operand(struct IR instr) {
    print_operand(instr.rd, rd_s);
    print_operand(instr.rs, rs_s);
    print_operand(instr.rt, rt_s);

    // 约定 BEQ 和 BNE 包围所有 Branch 指令
    if (IR_BEQ <= instr.type && instr.type <= IR_BNE) {
        printf(ir_format[instr.type], rs_s, rt_s, rd_s);  // 交换顺序
    } else {
        printf(ir_format[instr.type], rd_s, rs_s, rt_s);
    }

    printf("\n");
}

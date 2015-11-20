//
// Created by whz on 15-11-20.
//

#include "ir.h"
#include <stdlib.h>
#include <assert.h>

#define MAX_LINE 2048
#define NAME_LEN 128
// 指令缓冲区
static struct IR instr_buffer[MAX_LINE];

// 已经生成的指令数量
int nr_instr;

// 操作数答应缓冲区
static char rs_s[NAME_LEN];
static char rt_s[NAME_LEN];
static char rd_s[NAME_LEN];

//
// 操作数构造函数
//
int new_variable();
int new_temp();
int new_addr();
int new_lable();
Operand new_operand(enum Ope_Type type) {
    Operand p = (Operand)malloc(sizeof(struct Operand_));
    p->type = type;
    switch (type) {
        case OPE_VAR:
            p->var.index = new_variable();
        case OPE_TEMP:
            p->var.index = new_temp();
            break;
        case OPE_ADDR:
            p->var.index = new_addr();
            break;
        case OPE_LABEL:
            p->var.label = new_lable();
            break;
        default:
            break;
    }
    return p;
}

//
// 中间代码构造函数
// 返回 IR 在缓冲区中的下标
//
int new_instr(enum IR_Type type, Operand rs, Operand rt, Operand rd) {
    assert(nr_instr < MAX_LINE);

    instr_buffer[nr_instr].type = type;
    instr_buffer[nr_instr].rs = rs;
    instr_buffer[nr_instr].rt = rt;
    instr_buffer[nr_instr].rd = rd;
    return nr_instr++;
}

//
// 答应操作数, 为了变量和标签工厂函数可以简单实现, 以及方便比较,
// 变量和标签都存储为整型, 在打印操作数的时候加上统一前缀变成合法的变量名
// NOP指令答应为空字符串, 希望将来可以自动过滤.
//
void print_operand(Operand ope, char *str) {
    if (ope == NULL) {
        sprintf(str, "%s", "");
        return;
    }
    switch (ope->type) {
        case OPE_VAR:     sprintf(str, "v%d",  ope->var.index);    break;
        case OPE_FUNC:    sprintf(str, "%s",   ope->var.funcname); break;
        case OPE_TEMP:    sprintf(str, "t%d",  ope->var.index);    break;
        case OPE_ADDR:    sprintf(str, "a%d",  ope->var.index);    break;
        case OPE_DEREF:   sprintf(str, "*a%d", ope->var.index);    break;
        case OPE_FLOAT:   sprintf(str, "#%f",  ope->var.real);     break;
        case OPE_LABEL:   sprintf(str, "L%d",  ope->var.label);    break;
        case OPE_V_ADDR:  sprintf(str, "&v%d", ope->var.index);    break;
        case OPE_INTEGER: sprintf(str, "#%d",  ope->var.integer);  break;
        default:          sprintf(str, "%s",   "");
    }
}

static const char *ir_format[] = {
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
    "%sDEC %s %s",          // DEC, 第一个 %s 过滤 rd_s
    "%sARG %s",             // Pass argument, 第一个 %s 过滤 rd_s
    "%s := CALL %s",        // CALL
    "%sPARAM %s",           // DEC PARAM, 第一个 %s 过滤 rd_s
    "READ %s",              // READ
    "%sWRITE %s",           // WRITE, 第一个 %s 过滤 rd_s, 输出语义不用 rd
    ""                      // NOP
};

//
// 单条指令打印函数
//
void print_single_instr(struct IR instr, FILE *file) {
    print_operand(instr.rd, rd_s);
    print_operand(instr.rs, rs_s);
    print_operand(instr.rt, rt_s);

    // 约定 BEQ 和 BNE 包围所有 Branch 指令
    if (IR_BEQ <= instr.type && instr.type <= IR_BNE) {
        fprintf(file, ir_format[instr.type], rs_s, rt_s, rd_s);  // 交换顺序
    } else {
        fprintf(file, ir_format[instr.type], rd_s, rs_s, rt_s);
    }

    if (instr.type != IR_NOP) {
        fprintf(file, "\n");
    }
}

//
// 打印指令缓冲区中所有的已生成指令
//
void print_instr(FILE *file) {
    for (int i = 0; i < nr_instr; ++i) {
        print_single_instr(instr_buffer[i], file);
    }
}

//
// 提供新的变量名(数字编号)
//
int new_variable() {
    static int index = 0;
    return index++;
}

//
// 提供新的临时变量(数字编号)
//
int new_temp() {
    static int index = 0;
    return index++;
}

//
// 提供新的临时地址(数字编号)
//
int new_addr() {
    static int index = 0;
    return index++;
}

//
// 提供新的标签(数字编号)
//
int new_lable() {
    static int index= 0;
    return index++;
}

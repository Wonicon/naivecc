//
// Created by whz on 15-12-24.
//

#ifndef NJU_COMPILER_2015_REGISTER_H
#define NJU_COMPILER_2015_REGISTER_H

#include "operand.h"

const char *ensure(Operand ope);
const char *allocate(Operand ope);

void clear_reg_state();

void push_all();

void clear_reg_state_in_function();

void pass_arg(Operand ope);

extern int sp_offset;

#endif //NJU_COMPILER_2015_REGISTER_H

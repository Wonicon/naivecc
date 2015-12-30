//
// Created by whz on 15-12-24.
//

#ifndef NJU_COMPILER_2015_REGISTER_H
#define NJU_COMPILER_2015_REGISTER_H

#include "operand.h"

int ensure(Operand ope);

int allocate(Operand ope);

void clear_reg_state();

void push_all();

void set_dirty(int index);

const char *reg_to_s(int index);

#endif //NJU_COMPILER_2015_REGISTER_H

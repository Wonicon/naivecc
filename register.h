//
// Created by whz on 15-12-24.
//

#ifndef NJU_COMPILER_2015_REGISTER_H
#define NJU_COMPILER_2015_REGISTER_H

#include "operand.h"

char *ensure(Operand ope);
char *allocate(Operand ope);

void clear_reg_state();

void push_all();

#endif //NJU_COMPILER_2015_REGISTER_H

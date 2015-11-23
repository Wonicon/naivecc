//
// Created by whz on 15-11-24.
//

#include "dag.h"
#include <stdlib.h>

DagNode new_dagnode(int ir_idx, IR_Type ir_type, DagNode rs, DagNode rt) {
    DagNode p = (DagNode)malloc(sizeof(*p));
    p->ir_idx = ir_idx;
    p->ir_type = ir_type;
    p->rs_dep = rs;
    p->rt_dep = rt;
    return p;
}

void free_dag(Operand ope) {
    if (ope == NULL) {
        return;
    }
    free(ope->dep);
    ope->dep = NULL;
}
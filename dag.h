//
// Created by whz on 15-11-24.
//

#ifndef __DAG_H__
#define __DAG_H__

#include "ir.h"

typedef struct DagNode_ *DagNode;

struct DagNode_ {
    int ir_idx;
    IR_Type ir_type;
    DagNode parent;
    DagNode rs_dep;
    DagNode rt_dep;
};

#endif // __DAG_H__

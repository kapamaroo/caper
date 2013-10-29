#ifndef __ANALYSIS_H__
#define __ANALYSIS_H__

#include "datatypes.h"

enum analysis_type {
    A_KVL,  //Kirchhof's Voltage Law analysis
    A_KCL,  //Kirchhof's Current Law analysis
};

struct analysis_info {
    enum analysis_type type;
    int error;

    union {
        unsigned long node_size;
        unsigned long n;
    };

    union {
        unsigned long el_size;
        unsigned long e;
    };

    int *A;     //nodes x elements array (without the ground node)
    int *At;    //transposed A

    double *v;  //node's voltage relative to ground
                //('node_size' elements)

    double *u;  //element's voltage from vplus(+) to vminus(-) nodes
                //('el_size' elements)

    double *i;  //element's current
                //('el_size' elements)
};

void analyse(struct netlist_info *netlist, struct analysis_info *analysis,
             enum analysis_type type);

#endif

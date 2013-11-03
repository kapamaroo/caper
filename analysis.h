#ifndef __ANALYSIS_H__
#define __ANALYSIS_H__

#include "datatypes.h"

struct analysis_info {
    int error;

    union {
        unsigned long node_size;
        unsigned long n;
    };

    union {
        unsigned long el_size;
        unsigned long e;
    };

    unsigned long el_group1_size;
    unsigned long el_group2_size;

    int *A;     //nodes x elements array (without the ground node)
    int *At;    //transposed A

    int *A1;
    int *A1t;

    int *A2;
    int *A2t;

    double *v;  //node's voltage relative to ground
                //('node_size' elements)

    double *u;  //element's voltage from vplus(+) to vminus(-) nodes
                //('el_size' elements)

    double *i;  //element's current
                //('el_size' elements)
};

void analyse_kvl(struct netlist_info *netlist, struct analysis_info *analysis);
void analyse_kcl(struct netlist_info *netlist, struct analysis_info *analysis);
void analyse_mna(struct netlist_info *netlist, struct analysis_info *analysis);

void print_int_array(unsigned long row, unsigned long col, int *p);

#endif

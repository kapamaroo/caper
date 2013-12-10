#ifndef __ANALYSIS_H__
#define __ANALYSIS_H__

#include "datatypes.h"

struct analysis_info {
    int error;

    union {
        unsigned long node_size;  //without the ground node
        unsigned long n;
    };

    union {
        unsigned long el_size;
        unsigned long e;
    };

    unsigned long el_group1_size;
    unsigned long el_group2_size;

    dfloat_t *v;  //node's voltage relative to ground
                  //('node_size' elements)

    dfloat_t *u;  //element's voltage from vplus(+) to vminus(-) nodes
                  //('el_size' elements)

    dfloat_t *x;  //solution vector

    dfloat_t *mna_matrix;
    dfloat_t *mna_vector;
};

void analyse_mna(struct netlist_info *netlist, struct analysis_info *analysis);

void print_int_array(unsigned long row, unsigned long col, int *p);
void print_char_int_array(unsigned long row, unsigned long col, char *p);
void print_dfloat_array(unsigned long row, unsigned long col, dfloat_t *p);

#endif

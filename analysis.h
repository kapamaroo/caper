#ifndef __ANALYSIS_H__
#define __ANALYSIS_H__

#include "datatypes.h"
#include <gsl/gsl_permutation.h>
#include "csparse/csparse.h"

struct analysis_info {
    int error;

    unsigned long n;  //number of nodes without the ground node
    unsigned long e;  //number of branches

    unsigned long el_group1_size;
    unsigned long el_group2_size;

#if 0
    dfloat_t *v;  //node's voltage relative to ground
                  //('node_size' elements)

    dfloat_t *u;  //element's voltage from vplus(+) to vminus(-) nodes
                  //('el_size' elements)
#endif

    dfloat_t *x;  //solution vector

    gsl_permutation *LU_perm;

    dfloat_t *mna_matrix;
    dfloat_t *mna_vector;

    dfloat_t *transient_matrix;

    //sparse matrix members
    cs *cs_mna_matrix;
    csn *cs_mna_N;
    css *cs_mna_S;
};

void analyse_mna(struct netlist_info *netlist, struct analysis_info *analysis);

void print_dfloat_array(unsigned long row, unsigned long col, dfloat_t *p);

#endif

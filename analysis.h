#ifndef __ANALYSIS_H__
#define __ANALYSIS_H__

#include "datatypes.h"
#include <gsl/gsl_permutation.h>
#include "csparse/csparse.h"

enum solver {
    S_LU = 0,
    S_SPD,
    S_ITER,
    S_SPD_ITER,

    S_LU_SPARSE,
    S_SPD_SPARSE,
    S_ITER_SPARSE,
    S_SPD_ITER_SPARSE
};

enum transient_method {
    T_NONE = 0,
    T_TR,  //trapezoidal
    T_BE   //backward-euler
};

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

    dfloat_t *decomp;

    //sparse matrix members
    cs *cs_mna_matrix;        //G
    cs *cs_transient_matrix;  //C
    csn *cs_mna_N;
    css *cs_mna_S;

    int use_sparse;
    enum solver _solver;
    enum transient_method _transient_method;
    dfloat_t tol;
};

void analyse_mna(struct netlist_info *netlist, struct analysis_info *analysis);

void print_dfloat_array(unsigned long row, unsigned long col, dfloat_t *p);

#endif

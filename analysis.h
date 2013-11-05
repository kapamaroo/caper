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

    char *A;     //nodes x elements array (without the ground node)
    char *At;    //transposed A

    char *A1;
    char *A1t;

    char *A2;
    char *A2t;

    double *G;  //resistance matrix (m1 x m1) diagonal
    double *C;  //capacity matrix (m1 x m1) diagonal
    double *L;  //self inductor matrix  (m2 x m2) diagonal
    double *S1; //group1 current source vector (m1 x 1)
    double *S2; //group2 voltage source vector (m2 x 1)

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
void print_char_int_array(unsigned long row, unsigned long col, char *p);
void print_double_array(unsigned long row, unsigned long col, double *p);

#endif

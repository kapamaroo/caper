#include "analysis.h"

#include <stdlib.h>
#include <assert.h>
#include <errno.h>

void analyse_kvl(struct netlist_info *netlist, struct analysis_info *analysis) {
    assert(netlist);
    assert(analysis);

    unsigned long n = netlist->n;
    unsigned long e = netlist->e;

    int *A = (int*)malloc((n-1) * e * sizeof(int));
    if (!A) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    double *v = (double*)malloc((n-1) * sizeof(double));
    if (!v) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    double *u = (double*)malloc(e * sizeof(double));
    if (!u) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    double *i = (double*)malloc(e * sizeof(double));
    if (!i) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    //ignore the transposed matrix for now
#if 0
    int *At = NULL;
#else
    int *At = (int*)malloc(size * sizeof(int));
    if (!At) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
#endif

    unsigned long el_group1_size = netlist->el_group1_size;
    unsigned long el_group2_size = netlist->el_group2_size;

    unsigned int i;
    for (i=0; i<el_group1_size; ++i) {
        struct element *_el = &netlist->el_group1_pool[i];
        //TODO: populate A
    }

    for (i=0; i<el_group2_size; ++i) {
        struct element *_el = &netlist->el_group2_pool[i];
        //TODO: populate A
    }
}

void analyse_kcl(struct netlist_info *netlist, struct analysis_info *analysis);
void analyse_mna(struct netlist_info *netlist, struct analysis_info *analysis);

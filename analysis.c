#include "analysis.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

void analyse_kvl(struct netlist_info *netlist, struct analysis_info *analysis) {
    assert(netlist);
    assert(analysis);

    unsigned long n = netlist->n;
    unsigned long e = netlist->e;

    int *A = (int*)calloc((n-1) * e, sizeof(int));
    if (!A) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    double *v = (double*)calloc((n-1), sizeof(double));
    if (!v) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    double *u = (double*)calloc(e, sizeof(double));
    if (!u) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    double *i_current = (double*)calloc(e, sizeof(double));
    if (!i_current) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    //ignore the transposed matrix for now
#if 0
    int *At = NULL;
#else
    int *At = (int*)calloc((n-1) * e, sizeof(int));
    if (!At) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
#endif

    unsigned long el_group1_size = netlist->el_group1_size;
    unsigned long el_group2_size = netlist->el_group2_size;

    unsigned long i;
    for (i=0; i<el_group1_size; ++i) {
        struct element *_el = &netlist->el_group1_pool[i];
        unsigned long y = i;
        unsigned long xplus;
        unsigned long xminus;
        switch (_el->type) {
        case 'i':
            xplus = _el->i.vplus.nuid;
            xminus = _el->i.vminus.nuid;
            if (xplus)
                A[(xplus-1) * e + y] = 1;
            if (xminus)
                A[(xminus-1) * e + y] = -1;
            break;
        case 'r':
            xplus = _el->r.vplus.nuid;
            xminus = _el->r.vminus.nuid;
            if (xplus)
                A[(xplus-1) * e + y] = 1;
            if (xminus)
                A[(xminus-1) * e + y] = -1;
            break;
        case 'c':
            xplus = _el->c.vplus.nuid;
            xminus = _el->c.vminus.nuid;
            if (xplus)
                A[(xplus-1) * e + y] = 1;
            if (xminus)
                A[(xminus-1) * e + y] = -1;
            break;
        case 'q':
            printf("warning: ignore bjt elements in analysis for now\n");
            break;
        case 'm':
            printf("warning: ignore mos elements in analysis for now\n");
            break;
        case 'd':
            printf("warning: ignore diode elements in analysis for now\n");
            break;
        default:
            assert(0);
        }

#if 0
        if (xplus)
            printf("euid (%lu) writes (+) to A[%lu,%lu]\n",_el->euid,xplus,y);
        if (xminus)
            printf("euid (%lu) writes (-) to A[%lu,%lu]\n",_el->euid,xminus,y);

        print_int_array(n-1,e,A);
#endif

    }

    for (i=0; i<el_group2_size; ++i) {
        struct element *_el = &netlist->el_group2_pool[i];
        unsigned long y = el_group1_size + i;
        unsigned long xplus;
        unsigned long xminus;
        switch (_el->type) {
        case 'v':
            xplus = _el->v.vplus.nuid;
            xminus = _el->v.vminus.nuid;
            if (xplus)
                A[(xplus-1) * e + y] = 1;
            if (xminus)
                A[(xminus-1) * e + y] = -1;
            break;
        case 'l':
            xplus = _el->l.vplus.nuid;
            xminus = _el->l.vminus.nuid;
            if (xplus)
                A[(xplus-1) * e + y] = 1;
            if (xminus)
                A[(xminus-1) * e + y] = -1;
            break;
        default:
            assert(0);
        }

#if 0
        if (xplus)
            printf("euid (%lu) writes (+) to A[%lu,%lu]\n",_el->euid,xplus,y);
        if (xminus)
            printf("euid (%lu) writes (-) to A[%lu,%lu]\n",_el->euid,xminus,y);

        print_int_array(n-1,e,A);
#endif
    }

    analysis->n = n-1;
    analysis->e = e;
    analysis->A = A;
    analysis->At = At;
    analysis->v = v;
    analysis->u = u;
    analysis->i = i_current;
}

void analyse_kcl(struct netlist_info *netlist, struct analysis_info *analysis);
void analyse_mna(struct netlist_info *netlist, struct analysis_info *analysis);

void print_int_array(unsigned long row, unsigned long col, int *p) {
    unsigned long i;
    unsigned long j;

    printf("==============================================\n");
    printf("      ");
    for (j=0; j<col; ++j) {
        printf("%2lu|",j);
    }
    printf("\n    ___________________________________________\n");

    for (i=0; i<row; ++i) {
        printf("%3lu | ",i + 1);
        for (j=0; j<col; ++j) {
            int val = p[i*col + j];
            printf("% 2d ",val);
        }
        printf("\n");
    }
}

#include "analysis.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

void analysis_init(struct netlist_info *netlist, struct analysis_info *analysis) {
    assert(netlist);
    assert(analysis);

    unsigned long n = netlist->n;
    unsigned long e = netlist->e;
    unsigned long el_group1_size = netlist->el_group1_size;
    unsigned long el_group2_size = netlist->el_group2_size;

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

    double *G = (double*)calloc(el_group1_size * el_group1_size, sizeof(double));
    if (!G) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    double *C = (double*)calloc(el_group1_size * el_group1_size, sizeof(double));
    if (!C) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    double *L = (double*)calloc(el_group2_size * el_group2_size, sizeof(double));
    if (!L) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    double *S1 = (double*)calloc(el_group1_size, sizeof(double));
    if (!S1) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    double *S2 = (double*)calloc(el_group2_size, sizeof(double));
    if (!S2) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

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

            S1[y] = _el->value;

            break;
        case 'r':
            xplus = _el->r.vplus.nuid;
            xminus = _el->r.vminus.nuid;
            if (xplus)
                A[(xplus-1) * e + y] = 1;
            if (xminus)
                A[(xminus-1) * e + y] = -1;

            G[y*el_group1_size + y] = 1 / _el->value;

            break;
        case 'c':
            xplus = _el->c.vplus.nuid;
            xminus = _el->c.vminus.nuid;
            if (xplus)
                A[(xplus-1) * e + y] = 1;
            if (xminus)
                A[(xminus-1) * e + y] = -1;

            C[y*el_group1_size + y] = _el->value;

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

            //use i directly, S2 contains only group2 elements
            S2[i] = _el->value;

            break;
        case 'l':
            xplus = _el->l.vplus.nuid;
            xminus = _el->l.vminus.nuid;
            if (xplus)
                A[(xplus-1) * e + y] = 1;
            if (xminus)
                A[(xminus-1) * e + y] = -1;

            //use i directly, L contains only group2 elements
            L[i*el_group2_size + i] = _el->value;

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

    int *At = (int*)malloc((n-1) * e * sizeof(int));
    if (!At) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    unsigned long j;
    for (i=0; i<n-1; ++i) {
        for (j=0; j<e; ++j) {
            At[j*(n-1) + i] = A[i*e + j];
        }
    }

    //create A1 and A1t

    int *A1 = (int*)malloc((n-1) * el_group1_size * sizeof(int));
    if (!A1) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    for (i=0; i<n-1; ++i) {
        for (j=0; j<el_group1_size; ++j) {
            A1[i*el_group1_size + j] = A[i*e + j];
        }
    }

    int *A1t = (int*)malloc((n-1) * el_group1_size * sizeof(int));
    if (!A1t) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    for (i=0; i<n-1; ++i) {
        for (j=0; j<el_group1_size; ++j) {
            A1t[j*(n-1) + i] = A1[i*el_group1_size + j];
        }
    }

    //create A1 and A1t

    int *A2 = (int*)malloc((n-1) * el_group2_size * sizeof(int));
    if (!A2) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    for (i=0; i<n-1; ++i) {
        for (j=0; j<el_group2_size; ++j) {
            A2[i*el_group2_size + j] = A[i*e + el_group1_size + j];
        }
    }

    int *A2t = (int*)malloc((n-1) * el_group2_size * sizeof(int));
    if (!A2t) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    for (i=0; i<n-1; ++i) {
        for (j=0; j<el_group2_size; ++j) {
            A2t[j*(n-1) + i] = A2[i*el_group2_size + j];
        }
    }

    analysis->n = n-1;
    analysis->e = e;
    analysis->el_group1_size = el_group1_size;
    analysis->el_group2_size = el_group2_size;
    analysis->A = A;
    analysis->At = At;
    analysis->A1 = A1;
    analysis->A1t = A1t;
    analysis->A2 = A2;
    analysis->A2t = A2t;
    analysis->G = G;
    analysis->C = C;
    analysis->L = L;
    analysis->S1 = S1;
    analysis->S2 = S2;
    analysis->v = v;
    analysis->u = u;
    analysis->i = i_current;
}

void analyse_kvl(struct netlist_info *netlist, struct analysis_info *analysis) {
    analysis_init(netlist,analysis);
}

void analyse_kcl(struct netlist_info *netlist, struct analysis_info *analysis) {
    analysis_init(netlist,analysis);

}

void analyse_mna(struct netlist_info *netlist, struct analysis_info *analysis) {
    analysis_init(netlist,analysis);

}

void print_int_array(unsigned long row, unsigned long col, int *p) {
    unsigned long i;
    unsigned long j;

    printf("==============================================\n");
    printf("      ");
    for (j=0; j<col; ++j) {
        printf("%2lu|",j);
    }
    printf("\n     ___________________________________________\n");

    for (i=0; i<row; ++i) {
        printf("%3lu | ",i + 1);
        for (j=0; j<col; ++j) {
            int val = p[i*col + j];
            printf("% 2d ",val);
        }
        printf("\n");
    }
}

void print_double_array(unsigned long row, unsigned long col, double *p) {
    unsigned long i;
    unsigned long j;

    printf("==============================================\n");
    printf("      ");
    for (j=0; j<col; ++j) {
        printf("%2lu|",j);
    }
    printf("\n     ___________________________________________\n");

    for (i=0; i<row; ++i) {
        printf("%3lu | ",i + 1);
        for (j=0; j<col; ++j) {
            double val = p[i*col + j];
            printf("%2g ",val);
        }
        printf("\n");
    }
}

#include "analysis.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <gsl/gsl_linalg.h>

void analysis_init(struct netlist_info *netlist, struct analysis_info *analysis) {
    assert(netlist);
    assert(analysis);

    //the last node we care
    //also the number of nodes without considering the ground
    unsigned long _n = netlist->n - 1;
    unsigned long e = netlist->e;
    unsigned long el_group1_size = netlist->el_group1_size;
    unsigned long el_group2_size = netlist->el_group2_size;

    unsigned long mna_dim_size = _n + el_group2_size;
    printf("analysis: trying to allocate %lu bytes ...\n",
           mna_dim_size * mna_dim_size * sizeof(dfloat_t));
    dfloat_t *mna_matrix =
        (dfloat_t*)calloc(mna_dim_size * mna_dim_size,sizeof(dfloat_t));
    if (!mna_matrix) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *mna_vector = (dfloat_t*)calloc(mna_dim_size,sizeof(dfloat_t));
    if (!mna_vector) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *x = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    //dfloat_t *x = (dfloat_t*)calloc(mna_dim_size,sizeof(dfloat_t));
    if (!x) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *v = (dfloat_t*)calloc((_n), sizeof(dfloat_t));
    if (!v) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *u = (dfloat_t*)calloc(e, sizeof(dfloat_t));
    if (!u) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *C = (dfloat_t*)calloc(el_group1_size, sizeof(dfloat_t));
    if (!C) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *L = (dfloat_t*)calloc(el_group2_size, sizeof(dfloat_t));
    if (!L) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    printf("analysis: populating matrices ...\n");

    unsigned long i;
    for (i=0; i<el_group1_size; ++i) {
        struct element *_el = &netlist->el_group1_pool[i];
        switch (_el->type) {
        case 'c':   C[i] =     _el->value;  break;
        default:                            break;
        }
    }

    for (i=0; i<el_group2_size; ++i) {
        struct element *_el = &netlist->el_group2_pool[i];
        switch (_el->type) {
        case 'l':  L[i] = _el->value;  break;
        default:                       break;
        }
    }

    unsigned long j;

    //populate MNA Matrix

    //ignore ground node
    for (i=1; i<=_n; ++i) {
        struct  node *_node = &netlist->node_pool[i];
        for (j=0; j<_node->refs; ++j) {
            struct element *el = _node->attached_el[j]._el;
            if (el->type == 'r') {
                //NOTE: all rows are moved up by one (we ingore the ground node)
                unsigned long vplus = el->r->vplus.nuid;
                unsigned long vminus = el->r->vminus.nuid;
                assert(_node == el->r->vplus._node || _node == el->r->vminus._node);

                dfloat_t value = 1/el->value;

                if (vplus)
                    mna_matrix[(vplus-1)*mna_dim_size + vplus - 1]
                        += vminus ? value/2 : value;

                if (vminus)
                    mna_matrix[(vminus-1)*mna_dim_size + vminus - 1]
                        += vplus ? value/2 : value;

                if (vplus && vminus) {
                    mna_matrix[(vplus-1)*mna_dim_size + vminus - 1] += -value/2;
                    mna_matrix[(vminus-1)*mna_dim_size + vplus - 1] += -value/2;
                }
            }
            else if (el->type == 'v') {
                //ignore 'l' elements
                assert(_node == el->v->vplus._node || _node == el->v->vminus._node);

                //ignore ground node
                if (_node->nuid) {
                    unsigned long row = _node->nuid - 1;
                    dfloat_t value = (_node == el->v->vplus._node) ? +1 : -1;

                    //group2 element, populate A2
                    mna_matrix[row*mna_dim_size + _n + el->idx] = value;

                    //group2 element, populate A2 transposed
                    mna_matrix[(_n + el->idx)*mna_dim_size + row] = value;
                }
            }
            switch (el->type) {
            case 'v':  mna_vector[_n + el->idx] = el->value;  break;
            case 'i': {
                assert(_node == el->i->vplus._node || _node == el->i->vminus._node);
                //we have -A1*S1, therefore we subtract from the final result
                mna_vector[i] -=
                    (_node == el->i->vminus._node) ? -el->value :  el->value;
                break;
            }
            default:  break;
            }
        }
    }

    analysis->n = _n;
    analysis->e = e;
    analysis->el_group1_size = el_group1_size;
    analysis->el_group2_size = el_group2_size;
    analysis->C = C;
    analysis->L = L;
    analysis->v = v;
    analysis->u = u;
    analysis->mna_matrix = mna_matrix;
    analysis->mna_vector = mna_vector;
    analysis->x = x;
}

void solve_LU(struct analysis_info *analysis) {
    unsigned long mna_dim_size =
        analysis->n + analysis->el_group2_size;

    //GSL magic
    gsl_matrix_view Aview =
        gsl_matrix_view_array(analysis->mna_matrix,
                              mna_dim_size,
                              mna_dim_size);

    gsl_vector_view bview =
        gsl_vector_view_array(analysis->mna_vector,
                              mna_dim_size);

    gsl_vector_view x =
        gsl_vector_view_array(analysis->x,
                              mna_dim_size);

    gsl_permutation *perm = gsl_permutation_alloc(mna_dim_size);

    int perm_sign;
    gsl_linalg_LU_decomp(&Aview.matrix,perm,&perm_sign);
    gsl_linalg_LU_solve(&Aview.matrix,perm,&bview.vector,&x.vector);
}

void solve_cholesky(struct analysis_info *analysis) {
    //TODO
}

void analyse_mna(struct netlist_info *netlist, struct analysis_info *analysis) {
    analysis_init(netlist,analysis);
    solve_LU(analysis);
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

void print_char_int_array(unsigned long row, unsigned long col, char *p) {
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

void print_dfloat_array(unsigned long row, unsigned long col, dfloat_t *p) {
    unsigned long i;
    unsigned long j;

    printf("==============================================\n");
    printf("      ");
    for (j=0; j<col; ++j) {
        printf("%5lu|",j);
    }
    printf("\n     ___________________________________________\n");

    for (i=0; i<row; ++i) {
        printf("%3lu | ",i + 1);
        for (j=0; j<col; ++j) {
            dfloat_t val = p[i*col + j];
            printf("%5.2g ",val);
        }
        printf("\n");
    }
}

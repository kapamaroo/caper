#include "analysis.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>

enum connection_type get_conn_type(unsigned long annotated_id) {
    //throw the CONN_EL_GROUP_BITS
    annotated_id >>= CONN_EL_GROUP_BITS;
    unsigned long bits = annotated_id & (unsigned long)CONN_TYPE_MASK;
    assert(bits <= CONN_LAST_TYPE);
    enum connection_type type = (enum connection_type) bits;
    return type;
}

enum connection_el_group get_conn_el_group(unsigned long annotated_id) {
    unsigned long bits = annotated_id & CONN_EL_GROUP_MASK;
    enum connection_el_group group = (enum connection_el_group) bits;
    return group;
}

unsigned long get_conn_raw_id(unsigned long annotated_id) {
    return annotated_id >> CONN_INFO_BITS;
}

unsigned long set_conn_info(unsigned long id, enum connection_type conn_type,
                            char el_type) {
    //the first CONN_INFO_BITS of id must be empty!
    assert(!(id >> (sizeof(unsigned long) * CHAR_BIT - CONN_INFO_BITS)));

    enum connection_el_group group = CONN_EL_GROUP1;
    if (el_type == 'v' || el_type == 'l')
        group = CONN_EL_GROUP2;

    unsigned long annotated_id = (id << CONN_TYPE_BITS) | (unsigned long)conn_type;
    annotated_id = (annotated_id << CONN_EL_GROUP_BITS) | (unsigned long)group;
    return annotated_id;
}

void analysis_init(struct netlist_info *netlist, struct analysis_info *analysis) {
    assert(netlist);
    assert(analysis);

    unsigned long n = netlist->n;
    unsigned long e = netlist->e;
    unsigned long el_group1_size = netlist->el_group1_size;
    unsigned long el_group2_size = netlist->el_group2_size;

    unsigned long mna_vector_size = n-1 + el_group2_size;
    printf("analysis: trying to allocate %lu bytes ...\n",mna_vector_size * mna_vector_size);
    double *mna_vector = (double*)calloc(mna_vector_size * mna_vector_size,sizeof(double));
    if (!mna_vector) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

#if 0
    char *A = (char*)calloc((n-1) * e, sizeof(char));
    if (!A) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
#endif

    printf("still alive :)\n");

    double *v = (double*)calloc((n-1), sizeof(double));
    if (!v) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    printf("still alive :)\n");

    double *u = (double*)calloc(e, sizeof(double));
    if (!u) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    printf("still alive :)\n");

    double *i_current = (double*)calloc(e, sizeof(double));
    if (!i_current) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    printf("still alive :)\n");

    double *G = (double*)calloc(el_group1_size, sizeof(double));
    if (!G) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    printf("still alive :)\n");

    double *C = (double*)calloc(el_group1_size, sizeof(double));
    if (!C) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    printf("still alive :)\n");

    double *L = (double*)calloc(el_group2_size, sizeof(double));
    if (!L) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    printf("still alive :)\n");

    double *S1 = (double*)calloc(el_group1_size, sizeof(double));
    if (!S1) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    printf("still alive :)\n");

    double *S2 = (double*)calloc(el_group2_size, sizeof(double));
    if (!S2) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    printf("analysis: populating matrices ...\n");

    unsigned long i;
    for (i=0; i<el_group1_size; ++i) {
        struct element *_el = &netlist->el_group1_pool[i];
        unsigned long y = i;
#if 0
        unsigned long xplus;
        unsigned long xminus;
#endif
        switch (_el->type) {
        case 'i':
#if 0
            xplus = _el->i->vplus.nuid;
            xminus = _el->i->vminus.nuid;
            if (xplus)
                A[(xplus-1) * e + y] = 1;
            if (xminus)
                A[(xminus-1) * e + y] = -1;
#endif

            S1[y] = _el->value;

            break;
        case 'r':
#if 0
            xplus = _el->r->vplus.nuid;
            xminus = _el->r->vminus.nuid;
            if (xplus)
                A[(xplus-1) * e + y] = 1;
            if (xminus)
                A[(xminus-1) * e + y] = -1;
#endif

            G[y] = 1 / _el->value;

            break;
        case 'c':
#if 0
            xplus = _el->c->vplus.nuid;
            xminus = _el->c->vminus.nuid;
            if (xplus)
                A[(xplus-1) * e + y] = 1;
            if (xminus)
                A[(xminus-1) * e + y] = -1;
#endif

            C[y] = _el->value;

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
    }

    for (i=0; i<el_group2_size; ++i) {
        struct element *_el = &netlist->el_group2_pool[i];
#if 0
        unsigned long y = el_group1_size + i;
        unsigned long xplus;
        unsigned long xminus;
#endif
        switch (_el->type) {
        case 'v':
#if 0
            xplus = _el->v->vplus.nuid;
            xminus = _el->v->vminus.nuid;
            if (xplus)
                A[(xplus-1) * e + y] = 1;
            if (xminus)
                A[(xminus-1) * e + y] = -1;
#endif

            //use i directly, S2 contains only group2 elements
            S2[i] = _el->value;

            break;
        case 'l':
#if 0
            xplus = _el->l->vplus.nuid;
            xminus = _el->l->vminus.nuid;
            if (xplus)
                A[(xplus-1) * e + y] = 1;
            if (xminus)
                A[(xminus-1) * e + y] = -1;
#endif

            //use i directly, L contains only group2 elements
            L[i] = _el->value;

            break;
        default:
            assert(0);
        }
    }

    //create A1 and A1t
    printf("analysis: creating A1, A2 and transposed matrices ...\n");

    unsigned long j;

#if 0
    char *At = (char*)malloc((n-1) * e * sizeof(char));
    if (!At) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    for (i=0; i<n-1; ++i) {
        for (j=0; j<e; ++j) {
            At[j*(n-1) + i] = A[i*e + j];
        }
    }

    char *A1 = (char*)malloc((n-1) * el_group1_size * sizeof(char));
    if (!A1) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    for (i=0; i<n-1; ++i) {
        for (j=0; j<el_group1_size; ++j) {
            A1[i*el_group1_size + j] = A[i*e + j];
        }
    }

    char *A1t = (char*)malloc((n-1) * el_group1_size * sizeof(char));
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

    char *A2 = (char*)malloc((n-1) * el_group2_size * sizeof(char));
    if (!A2) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    for (i=0; i<n-1; ++i) {
        for (j=0; j<el_group2_size; ++j) {
            A2[i*el_group2_size + j] = A[i*e + el_group1_size + j];
        }
    }

    char *A2t = (char*)malloc((n-1) * el_group2_size * sizeof(char));
    if (!A2t) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    for (i=0; i<n-1; ++i) {
        for (j=0; j<el_group2_size; ++j) {
            A2t[j*(n-1) + i] = A2[i*el_group2_size + j];
        }
    }
#endif

    //ignore ground node
    for (i=1; i<n-1; ++i) {
        struct  node *_node = &netlist->node_pool[i];
        for (j=0; j<_node->refs; ++j) {
            struct element *el = _node->attached_el[j]._el;
            if (el->type == 'r') {
                unsigned long vplus = el->r->vplus.nuid;
                unsigned long vminus = el->r->vminus.nuid;
                assert(_node == el->r->vplus._node || _node == el->r->vminus._node);

                mna_vector[vplus*mna_vector_size + vplus] += 1/el->value;
                mna_vector[vplus*mna_vector_size + vminus] += -1/el->value;
                mna_vector[vminus*mna_vector_size + vplus] += -1/el->value;
                mna_vector[vminus*mna_vector_size + vminus] += 1/el->value;
            }
            if (el->type == 'v' /*|| el->type == 'l'*/) {
                assert(_node == el->v->vplus._node || _node == el->v->vminus._node);

                //ignore ground node
                if (_node->nuid) {
                    unsigned long row = _node->nuid - 1;
                    unsigned long offset = n - 1;
                    double value = (_node == el->v->vplus._node) ? +1 : -1;

                    //group2 element, populate A2
                    mna_vector[row*mna_vector_size + offset + el->idx] = value;

                    //group2 element, populate A2 transposed
                    mna_vector[(row + el->idx)*mna_vector_size + row] = value;
                }
            }
        }
    }

    analysis->n = n-1;
    analysis->e = e;
    analysis->el_group1_size = el_group1_size;
    analysis->el_group2_size = el_group2_size;
#if 0
    analysis->A = A;
    analysis->At = At;
    analysis->A1 = A1;
    analysis->A1t = A1t;
    analysis->A2 = A2;
    analysis->A2t = A2t;
#endif
    analysis->G = G;
    analysis->C = C;
    analysis->L = L;
    analysis->S1 = S1;
    analysis->S2 = S2;
    analysis->v = v;
    analysis->u = u;
    analysis->i = i_current;
    analysis->mna_vector = mna_vector;
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

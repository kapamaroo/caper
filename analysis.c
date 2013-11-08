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

    unsigned long mna_dim_size = n-1 + el_group2_size;
    printf("analysis: trying to allocate %lu bytes ...\n",
           mna_dim_size * mna_dim_size * sizeof(dfloat_t));
    dfloat_t *mna_matrix = (dfloat_t*)calloc(mna_dim_size * mna_dim_size,sizeof(dfloat_t));
    if (!mna_matrix) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *mna_vector = (dfloat_t*)calloc(mna_dim_size,sizeof(dfloat_t));
    if (!mna_vector) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *v = (dfloat_t*)calloc((n-1), sizeof(dfloat_t));
    if (!v) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *u = (dfloat_t*)calloc(e, sizeof(dfloat_t));
    if (!u) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *i_current = (dfloat_t*)calloc(e, sizeof(dfloat_t));
    if (!i_current) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *G = (dfloat_t*)calloc(el_group1_size, sizeof(dfloat_t));
    if (!G) {
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

    dfloat_t *S1 = (dfloat_t*)calloc(el_group1_size, sizeof(dfloat_t));
    if (!S1) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *S2 = (dfloat_t*)calloc(el_group2_size, sizeof(dfloat_t));
    if (!S2) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    printf("analysis: populating matrices ...\n");

    unsigned long i;
    for (i=0; i<el_group1_size; ++i) {
        struct element *_el = &netlist->el_group1_pool[i];
        unsigned long y = i;
        switch (_el->type) {
        case 'i':  S1[y] =     _el->value;  break;
        case 'r':   G[y] = 1 / _el->value;  break;
        case 'c':   C[y] =     _el->value;  break;
        default:
            printf("warning: ignore type '%c' element in analysis\n",_el->type);
            break;
        }
    }

    for (i=0; i<el_group2_size; ++i) {
        struct element *_el = &netlist->el_group2_pool[i];
        switch (_el->type) {
        case 'l':  L[i] = _el->value;  break;
        default:
            break;
        }
    }

    //create A1 and A1t
    printf("analysis: creating A1, A2 and transposed matrices ...\n");

    unsigned long j;

    //populate MNA Matrix

    //ignore ground node
    for (i=1; i<n; ++i) {
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
                    unsigned long offset = n - 1;
                    dfloat_t value = (_node == el->v->vplus._node) ? +1 : -1;

                    //group2 element, populate A2
                    mna_matrix[row*mna_dim_size + offset + el->idx] = value;

                    //group2 element, populate A2 transposed
                    mna_matrix[(offset + el->idx)*mna_dim_size + row] = value;
                }
            }
            switch (el->type) {
            case 'v':  mna_vector[n - 1 + el->idx] = el->value;  break;
            case 'i': {
                assert(_node == el->i->vplus._node || _node == el->i->vminus._node);
                mna_vector[i] += (_node == el->i->vminus._node)
                    ? -S1[el->idx]
                    :  S1[el->idx];
                break;
            }
            default:  break;
            }
        }
    }

    analysis->n = n-1;
    analysis->e = e;
    analysis->el_group1_size = el_group1_size;
    analysis->el_group2_size = el_group2_size;
    analysis->G = G;
    analysis->C = C;
    analysis->L = L;
    analysis->S1 = S1;
    analysis->S2 = S2;
    analysis->v = v;
    analysis->u = u;
    analysis->i = i_current;
    analysis->mna_matrix = mna_matrix;
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

#include "analysis.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <gsl/gsl_linalg.h>
#include <math.h>

extern int debug_on;
extern int force_sparse;

#define MSG(msg) do { printf("INFO : %-24s(): %s\n",__FUNCTION__,msg); } while (0);
#ifdef NDEBUG
#define DEBUG_MSG(msg)
#else
#define DEBUG_MSG(msg) do { if (debug_on) printf("DEBUG: %-24s(): %s\n",__FUNCTION__,msg); } while (0);
#endif

#define BI_CG_EPSILON 1e-14

void fprint_dfloat_array(const char *filename,
                         unsigned long row, unsigned long col, dfloat_t *p);
static unsigned long count_nonzeros(struct netlist_info *netlist);
static void analyse_init_solver(struct analysis_info *analysis,enum solver _solver);

void analysis_init(struct netlist_info *netlist, struct analysis_info *analysis) {
    const int use_sparse = analysis->use_sparse;
    const enum solver _solver = analysis->_solver;
    const enum transient_method _transient_method = analysis->_transient_method;

    printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
    //the last node we care
    //also the number of nodes without considering the ground
    unsigned long _n = netlist->n - 1;
    unsigned long e = netlist->e;
    unsigned long el_group1_size = netlist->el_group1_size;
    unsigned long el_group2_size = netlist->el_group2_size;

    unsigned long mna_dim_size = _n + el_group2_size;

    cs *cs_mna_matrix = NULL;
    cs *cs_transient_matrix = NULL;
    dfloat_t *mna_matrix = NULL;
    dfloat_t *transient_matrix = NULL;

    if (use_sparse) {
        unsigned long nonzeros = count_nonzeros(netlist);
        //see cs_spalloc()
        printf("debug: trying to allocate %lu bytes ...\n",
               sizeof(cs) + 2*nonzeros * sizeof(int) + nonzeros*sizeof(dfloat_t));

        cs_mna_matrix = cs_spalloc(mna_dim_size,mna_dim_size,nonzeros,1,1);
        if (!cs_mna_matrix) {
            perror(__FUNCTION__);
            exit(EXIT_FAILURE);
        }

        if (_transient_method != T_NONE) {
            cs_transient_matrix = cs_spalloc(mna_dim_size,mna_dim_size,nonzeros,1,1);
            if (!cs_transient_matrix) {
                perror(__FUNCTION__);
                exit(EXIT_FAILURE);
            }
        }
    }
    else {
        printf("debug: trying to allocate %lu bytes ...\n",
               mna_dim_size * mna_dim_size * sizeof(dfloat_t));
        mna_matrix = (dfloat_t*)calloc(mna_dim_size*mna_dim_size,sizeof(dfloat_t));
        if (!mna_matrix) {
            perror(__FUNCTION__);
            exit(EXIT_FAILURE);
        }

        if (_transient_method != T_NONE) {
            transient_matrix = (dfloat_t *)calloc(mna_dim_size*mna_dim_size,sizeof(dfloat_t));
            if (!transient_matrix) {
                perror(__FUNCTION__);
                exit(EXIT_FAILURE);
            }
        }
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

#if 0
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
#endif

    MSG("populating matrices ...")

    unsigned long i;
    unsigned long j;

    //populate MNA Matrix

    //NOTE: ignore ground node
    //      all rows are moved up by one

    for (i=1; i<=_n; ++i) {
        struct node *_node = &netlist->node_pool[i];
        unsigned long row  = _node->nuid - 1;
        for (j=0; j<_node->refs; ++j) {
            struct element *el = _node->attached_el[j]._el;
            switch (el->type) {
            case 'r': {
                assert(_node == el->r->vplus._node || _node == el->r->vminus._node);
                unsigned long this_node  = row;
                unsigned long other_node = (_node != el->r->vplus._node) ? el->r->vplus.nuid : el->r->vminus.nuid;
                dfloat_t value = 1/el->value;

                if (use_sparse) {
                    //diagonal entry
                    cs_entry(cs_mna_matrix,this_node,this_node,value);
                    //off diagonal entry
                    if (other_node--)
                        cs_entry(cs_mna_matrix,this_node,other_node,-value);
                }
                else {
                    //diagonal entry
                    mna_matrix[this_node*mna_dim_size + this_node] += value;
                    //off diagonal entry
                    if (other_node--)
                        mna_matrix[this_node*mna_dim_size + other_node] += -value;
                }
                break;
            }
            case 'v': {
                assert(_node == el->v->vplus._node || _node == el->v->vminus._node);
                dfloat_t value = (_node == el->v->vplus._node) ? +1 : -1;

                unsigned long col = _n + el->idx;

                if (use_sparse) {
                    //group2 element, populate A2
                    cs_entry(cs_mna_matrix,row,col,value);
                    //group2 element, populate A2 transposed
                    cs_entry(cs_mna_matrix,col,row,value);
                }
                else {
                    //group2 element, populate A2
                    mna_matrix[row*mna_dim_size + col] = value;
                    //group2 element, populate A2 transposed
                    mna_matrix[col*mna_dim_size + row] = value;
                }

                mna_vector[col] = el->value;
                break;
            }
            case 'c': {
                if (_transient_method == T_NONE)
                    break;

                assert(_node == el->c->vplus._node || _node == el->c->vminus._node);
                unsigned long this_node  = row;
                unsigned long other_node = (_node != el->c->vplus._node) ? el->c->vplus.nuid : el->c->vminus.nuid;
                dfloat_t value = el->value;

                if (use_sparse) {
                    //diagonal entry
                    cs_entry(cs_transient_matrix,this_node,this_node,value);
                    //off diagonal entry
                    if (other_node--)
                        cs_entry(cs_transient_matrix,this_node,other_node,-value);
                }
                else {
                    //diagonal entry
                    transient_matrix[this_node*mna_dim_size + this_node] += value;
                    //off diagonal entry
                    if (other_node--)
                        transient_matrix[this_node*mna_dim_size + other_node] += -value;
                }
                break;
            }
            case 'l': {
                assert(_node == el->l->vplus._node || _node == el->l->vminus._node);
                dfloat_t value = (_node == el->l->vplus._node) ? +1 : -1;

                unsigned long col = _n + el->idx;

                if (use_sparse) {
                    //group2 element, populate A2
                    cs_entry(cs_mna_matrix,row,col,value);
                    //group2 element, populate A2 transposed
                    cs_entry(cs_mna_matrix,col,row,value);
                }
                else {
                    //group2 element, populate A2
                    mna_matrix[row*mna_dim_size + col] = value;
                    //group2 element, populate A2 transposed
                    mna_matrix[col*mna_dim_size + row] = value;
                }

                if (_transient_method == T_NONE)
                    break;

                unsigned long row = col;
                if (use_sparse)
                    cs_entry(cs_transient_matrix,row,col,-el->value);
                else
                    transient_matrix[row*mna_dim_size + col] = -el->value;

                break;
            }
            case 'i': {
                assert(_node == el->i->vplus._node || _node == el->i->vminus._node);
                //we have -A1*S1, therefore we subtract from the final result
                mna_vector[row] -=
                    (_node == el->i->vminus._node) ? -el->value : el->value;
                break;
            }
            default:  break;
            }
        }
    }

    if (use_sparse) {
        MSG("compress DC matrix ...")
        cs_mna_matrix = cs_compress(cs_mna_matrix);
        MSG("deduplicate DC entries ...")
        if (!cs_dupl(cs_mna_matrix)) {
            printf("cs_dupl() failed - exit.\n");
            exit(EXIT_FAILURE);
        }
        if (_transient_method != T_NONE) {
            MSG("compress transient matrix ...")
            cs_transient_matrix = cs_compress(cs_transient_matrix);
            MSG("deduplicate transient entries ...")
            if (!cs_dupl(cs_transient_matrix)){
                printf("cs_dupl() failed - exit.\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    analysis->n = _n;
    analysis->e = e;
    analysis->el_group1_size = el_group1_size;
    analysis->el_group2_size = el_group2_size;
#if 0
    analysis->v = v;
    analysis->u = u;
#endif
    analysis->x = x;
    analysis->mna_vector = mna_vector;

    analysis->cs_mna_matrix = cs_mna_matrix;
    analysis->cs_transient_matrix = cs_transient_matrix;
    analysis->mna_matrix = mna_matrix;
    analysis->transient_matrix = transient_matrix;

    analyse_init_solver(analysis,_solver);
}

static unsigned long count_nonzeros(struct netlist_info *netlist) {
    unsigned long i;
    unsigned long j;

    unsigned long _n = netlist->n - 1;
    unsigned long nonzeros = 0;

    //ignore ground node
    for (i=1; i<=_n; ++i) {
        struct node *_node = &netlist->node_pool[i];
        for (j=0; j<_node->refs; ++j) {
            struct element *el = _node->attached_el[j]._el;
            switch (el->type) {
            case 'r': {
                assert(_node == el->r->vplus._node || _node == el->r->vminus._node);
                unsigned long other_node = (_node != el->r->vplus._node) ? el->r->vplus.nuid : el->r->vminus.nuid;

                //NOTE: all rows are moved up by one (we ingore the ground node)
                nonzeros++;        //diagonal entry
                if (other_node--)
                    nonzeros++;    //off diagonal entry
                break;
            }
            case 'v': {
                assert(_node == el->v->vplus._node || _node == el->v->vminus._node);

                //group2 element, populate A2
                nonzeros++;

                //group2 element, populate A2 transposed
                nonzeros++;
                break;
            }
            case 'l': {
                assert(_node == el->l->vplus._node || _node == el->l->vminus._node);

                //group2 element, populate A2
                nonzeros++;

                //group2 element, populate A2 transposed
                nonzeros++;
                break;
            }
            default:  break;
            }
        }
    }
    return nonzeros;
}

void decomp_LU(struct analysis_info *analysis) {
    DEBUG_MSG("")
    unsigned long mna_dim_size =
        analysis->n + analysis->el_group2_size;

    //GSL magic
    gsl_matrix_view Aview =
        gsl_matrix_view_array(analysis->mna_matrix,
                              mna_dim_size,
                              mna_dim_size);

    analysis->LU_perm = gsl_permutation_alloc(mna_dim_size);

    int perm_sign;
    gsl_linalg_LU_decomp(&Aview.matrix,analysis->LU_perm,&perm_sign);
}

void solve_LU(struct analysis_info *analysis) {
    DEBUG_MSG("")
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

    assert(analysis->LU_perm);
    gsl_linalg_LU_solve(&Aview.matrix,analysis->LU_perm,&bview.vector,&x.vector);
}

void decomp_cholesky(struct analysis_info *analysis) {
    DEBUG_MSG("")
    unsigned long mna_dim_size =
        analysis->n + analysis->el_group2_size;

    //GSL magic
    gsl_matrix_view Aview =
        gsl_matrix_view_array(analysis->mna_matrix,
                              mna_dim_size,
                              mna_dim_size);

    gsl_linalg_cholesky_decomp(&Aview.matrix);
}

void solve_cholesky(struct analysis_info *analysis) {
    DEBUG_MSG("")
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

    gsl_linalg_cholesky_solve(&Aview.matrix,&bview.vector,&x.vector);
}

void decomp_LU_sparse(struct analysis_info *analysis) {
    DEBUG_MSG("")

    //sparse magic
    css *S = cs_sqr(2,analysis->cs_mna_matrix,0);
    if (!S) {
        printf("cs_sqr() failed - exit.\n");
        exit(EXIT_FAILURE);
    }

    csn *N = cs_lu(analysis->cs_mna_matrix,S,1);
    if (!N) {
        printf("cs_lu() failed - exit.\n");
        exit(EXIT_FAILURE);
    }

    assert(analysis->n + analysis->el_group2_size == analysis->cs_mna_matrix->n);

    analysis->cs_mna_S = S;
    analysis->cs_mna_N = N;
    //cs_free(analysis->cs_mna_matrix);
    //analysis->cs_mna_matrix = NULL;
}

void solve_LU_sparse(struct analysis_info *analysis) {
    DEBUG_MSG("")
    unsigned long mna_dim_size =
        analysis->n + analysis->el_group2_size;

    //sparse magic
    assert(analysis->cs_mna_matrix);

    cs *A = analysis->cs_mna_matrix;
    dfloat_t *b = analysis->x;

    memcpy(b,analysis->mna_vector,mna_dim_size*sizeof(dfloat_t));

    const double tol = 1e-14;
    if (!cs_lusol(2,A,b,tol)) {
        printf("cs_lusol() failed - exit.\n");
        exit(EXIT_FAILURE);
    }
}

void decomp_cholesky_sparse(struct analysis_info *analysis) {
    DEBUG_MSG("")

    //sparse magic
    css *S = cs_schol(1,analysis->cs_mna_matrix);
    if (!S) {
        printf("cs_schol() failed - exit.\n");
        exit(EXIT_FAILURE);
    }

    csn *N = cs_chol(analysis->cs_mna_matrix,S);
    if (!N) {
        printf("cs_chol() failed - exit.\n");
        exit(EXIT_FAILURE);
    }

    analysis->cs_mna_S = S;
    analysis->cs_mna_N = N;
    //cs_free(analysis->cs_mna_matrix);
    //analysis->cs_mna_matrix = NULL;
}

void solve_cholesky_sparse(struct analysis_info *analysis) {
    DEBUG_MSG("")
    unsigned long mna_dim_size =
        analysis->n + analysis->el_group2_size;

    //sparse magic
    cs *A = analysis->cs_mna_matrix;
    dfloat_t *b = analysis->x;

    memcpy(b,analysis->mna_vector,mna_dim_size*sizeof(dfloat_t));

    if (!cs_cholsol(1,A,b)) {
        printf("cs_cholsol() failed - exit.\n");
        exit(EXIT_FAILURE);
    }
}

static inline dfloat_t *init_preconditioner(dfloat_t *M, dfloat_t *z, dfloat_t *r, unsigned long mna_dim_size) {
    unsigned long i;
    for (i=0; i<mna_dim_size; ++i)
        z[i] = (M[i] != 0) ? r[i]/M[i] : r[i];
    return z;
}

static inline dfloat_t _dot(dfloat_t *x, dfloat_t *y, unsigned long size) {
    unsigned long i;
    dfloat_t result = 0;
    for (i=0; i<size; ++i)
        result += x[i]*y[i];
    return result;
}

static inline dfloat_t *_dot_add(dfloat_t *result, dfloat_t *z, dfloat_t beta, dfloat_t *p, unsigned long size) {
    unsigned long i;
    for (i=0; i<size; ++i)
        result[i] = z[i] + beta*p[i];
    return result;
}

static inline dfloat_t *_mult(dfloat_t *q, dfloat_t *A, dfloat_t *x, unsigned long size) {
    unsigned long i;
    for (i=0; i<size; ++i) {
        dfloat_t *row = &A[i*size];
        q[i] = _dot(row,x,size);
    }
    return q;
}

static inline dfloat_t _dot_transposed(dfloat_t *x, dfloat_t *y, unsigned long size) {
    unsigned long i;
    dfloat_t result = 0;
    for (i=0; i<size; ++i)
        result += x[i*size]*y[i];
    return result;
}

static inline dfloat_t *_mult_transposed(dfloat_t *q, dfloat_t *A, dfloat_t *x, unsigned long size) {
    unsigned long i;
    for (i=0; i<size; ++i) {
        dfloat_t *col = &A[i];
        q[i] = _dot_transposed(col,x,size);
    }
    return q;
}

static inline void init_M(dfloat_t *M, dfloat_t *A, unsigned long size) {
    unsigned long k;
    for (k=0; k<size; ++k)
        M[k] = A[k*size + k];
}

void solve_cg(struct analysis_info *analysis, dfloat_t tol) {
    DEBUG_MSG("")
    unsigned long _n = analysis->n;
    unsigned long el_group2_size = analysis->el_group2_size;
    unsigned long mna_dim_size = _n + el_group2_size;

    //dfloat_t *_r = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    dfloat_t *_r = (dfloat_t*)calloc(mna_dim_size,sizeof(dfloat_t));
    if (!_r) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_z = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_z) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_M = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_M) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_p = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_p) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_q = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_q) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_A = analysis->mna_matrix;
    dfloat_t *_b = analysis->mna_vector;
    dfloat_t *_x = analysis->x;

    //initial values:
    //                 _x[] = 0,...,0
    //                 _r = b
    memset(_x,0,mna_dim_size*sizeof(dfloat_t));
    memcpy(_r,_b,mna_dim_size*sizeof(dfloat_t));
    init_M(_M,_A,mna_dim_size);
    dfloat_t rho_old = _dot(_r,_r,mna_dim_size);
    dfloat_t norm_b = sqrt(_dot(_b,_b,mna_dim_size));
    if (norm_b == 0)
        norm_b = 1;

    int i = 0;
    const int max_iter = mna_dim_size;
    for (i=0; i<max_iter; ++i) {
        _z = init_preconditioner(_M,_z,_r,mna_dim_size);
        dfloat_t rho = _dot(_r,_z,mna_dim_size);
        if (i == 0)
            memcpy(_p,_z,mna_dim_size*sizeof(dfloat_t));
        else {
            dfloat_t beta = rho/rho_old;
            _p = _dot_add(_p,_z,beta,_p,mna_dim_size);
        }
        rho_old = rho;
        _q = _mult(_q,_A,_p,mna_dim_size);
        dfloat_t alpha = rho/_dot(_p,_q,mna_dim_size);
        _x = _dot_add(_x,_x,alpha,_p,mna_dim_size);
        _r = _dot_add(_r,_r,-alpha,_q,mna_dim_size);
        dfloat_t cond = sqrt(_dot(_r,_r,mna_dim_size))/norm_b;
        if (cond < tol)
            break;
    }

    free(_r);
    free(_z);
    free(_M);
    free(_p);
    free(_q);
}

void solve_cg_sparse(struct analysis_info *analysis, dfloat_t tol) {
    DEBUG_MSG("")
    unsigned long _n = analysis->n;
    unsigned long el_group2_size = analysis->el_group2_size;
    unsigned long mna_dim_size = _n + el_group2_size;

    //dfloat_t *_r = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    dfloat_t *_r = (dfloat_t*)calloc(mna_dim_size,sizeof(dfloat_t));
    if (!_r) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_z = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_z) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_M = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_M) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_p = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_p) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_q = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_q) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    cs *_A = analysis->cs_mna_matrix;
    dfloat_t *_b = analysis->mna_vector;
    dfloat_t *_x = analysis->x;

    //initial values:
    //                 _x[] = 0,...,0
    //                 _r = b
    memset(_x,0,mna_dim_size*sizeof(dfloat_t));
    memcpy(_r,_b,mna_dim_size*sizeof(dfloat_t));
    //init_M(_M,_A,mna_dim_size);
    cs_diagonal_values(_A,_M);
    dfloat_t rho_old = _dot(_r,_r,mna_dim_size);
    dfloat_t norm_b = sqrt(_dot(_b,_b,mna_dim_size));
    if (norm_b == 0)
        norm_b = 1;

    int i = 0;
    const int max_iter = mna_dim_size;
    for (i=0; i<max_iter; ++i) {
        _z = init_preconditioner(_M,_z,_r,mna_dim_size);
        dfloat_t rho = _dot(_r,_z,mna_dim_size);
        if (i == 0)
            memcpy(_p,_z,mna_dim_size*sizeof(dfloat_t));
        else {
            dfloat_t beta = rho/rho_old;
            _p = _dot_add(_p,_z,beta,_p,mna_dim_size);
        }
        rho_old = rho;
        //_q = _mult(_q,_A,_p,mna_dim_size);
        memset(_q,0,mna_dim_size*sizeof(dfloat_t));
        if (!cs_gaxpy(_A,_p,_q)) {
            printf("cs_gaxpy() failed - exit.\n");
            exit(EXIT_FAILURE);
        }
        dfloat_t alpha = rho/_dot(_p,_q,mna_dim_size);
        _x = _dot_add(_x,_x,alpha,_p,mna_dim_size);
        _r = _dot_add(_r,_r,-alpha,_q,mna_dim_size);
        dfloat_t cond = sqrt(_dot(_r,_r,mna_dim_size))/norm_b;
        if (cond < tol)
            break;
    }

    free(_r);
    free(_z);
    free(_M);
    free(_p);
    free(_q);

}
void solve_bi_cg(struct analysis_info *analysis, dfloat_t tol) {
    DEBUG_MSG("")
    unsigned long _n = analysis->n;
    unsigned long el_group2_size = analysis->el_group2_size;
    unsigned long mna_dim_size = _n + el_group2_size;

    //dfloat_t *_r = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    dfloat_t *_r = (dfloat_t*)calloc(mna_dim_size,sizeof(dfloat_t));
    if (!_r) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    //dfloat_t *_r_ = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    dfloat_t *_r_ = (dfloat_t*)calloc(mna_dim_size,sizeof(dfloat_t));
    if (!_r_) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_z = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_z) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_z_ = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_z_) {        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_M = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_M) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_p = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_p) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_q = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_q) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_p_ = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_p_) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_q_ = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_q_) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_A = analysis->mna_matrix;
    dfloat_t *_b = analysis->mna_vector;
    dfloat_t *_x = analysis->x;
    //initial values:
    //                 _x[] = 0,...,0
    //                 _r = b
    memset(_x,0,mna_dim_size*sizeof(dfloat_t));
    memcpy(_r,_b,mna_dim_size*sizeof(dfloat_t));
    memcpy(_r_,_r,mna_dim_size*sizeof(dfloat_t));
    init_M(_M,_A,mna_dim_size);
    dfloat_t rho_old = _dot(_r,_r,mna_dim_size);
    dfloat_t norm_b = sqrt(_dot(_b,_b,mna_dim_size));
    if (norm_b == 0)
        norm_b = 1;

    //M^T == M (diagonal)

    int i = 0;
    const int max_iter = (mna_dim_size > 20) ? mna_dim_size : 20;  //max()
    for (i=0; i<max_iter; ++i) {
        _z = init_preconditioner(_M,_z,_r,mna_dim_size);
        _z_ = init_preconditioner(_M,_z_,_r_,mna_dim_size);
        dfloat_t rho = _dot(_r_,_z,mna_dim_size);
#ifdef PRECISION_DOUBLE
        dfloat_t abs_rho = fabs(rho);
#else
        dfloat_t abs_rho = fabsf(rho);
#endif
        if (abs_rho < BI_CG_EPSILON) {
            printf("%s error: algorithm failure (abs(rho) < EPSILON) - exit\n",__FUNCTION__);
            exit(EXIT_FAILURE);
        }
        if (i == 0) {
            memcpy(_p,_z,mna_dim_size*sizeof(dfloat_t));
            memcpy(_p_,_z_,mna_dim_size*sizeof(dfloat_t));
        }
        else {
            dfloat_t beta = rho/rho_old;
            _p = _dot_add(_p,_z,beta,_p,mna_dim_size);
            _p_ = _dot_add(_p_,_z_,beta,_p_,mna_dim_size);
        }
        rho_old = rho;
        _q = _mult(_q,_A,_p,mna_dim_size);
        _q_ = _mult_transposed(_q_,_A,_p_,mna_dim_size);
        dfloat_t omega = _dot(_p_,_q,mna_dim_size);
#ifdef PRECISION_DOUBLE
        dfloat_t abs_omega = fabs(rho);
#else
        dfloat_t abs_omega = fabsf(rho);
#endif
        if (abs_omega < BI_CG_EPSILON) {
            printf("%s error: algorithm failure (abs(omega) < EPSILON) - exit\n",__FUNCTION__);
            exit(EXIT_FAILURE);
        }
        dfloat_t alpha = rho/omega;
        _x = _dot_add(_x,_x,alpha,_p,mna_dim_size);
        _r = _dot_add(_r,_r,-alpha,_q,mna_dim_size);
        _r_ = _dot_add(_r_,_r_,-alpha,_q_,mna_dim_size);
        dfloat_t cond = sqrt(_dot(_r,_r,mna_dim_size))/norm_b;
        if (cond < tol)
            break;
    }

    free(_r);
    free(_z);
    free(_M);
    free(_p);
    free(_q);
    free(_r_);
    free(_z_);
    free(_p_);
    free(_q_);
}

void solve_bi_cg_sparse(struct analysis_info *analysis, dfloat_t tol) {
    DEBUG_MSG("")
    unsigned long _n = analysis->n;
    unsigned long el_group2_size = analysis->el_group2_size;
    unsigned long mna_dim_size = _n + el_group2_size;

    //dfloat_t *_r = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    dfloat_t *_r = (dfloat_t*)calloc(mna_dim_size,sizeof(dfloat_t));
    if (!_r) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    //dfloat_t *_r_ = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    dfloat_t *_r_ = (dfloat_t*)calloc(mna_dim_size,sizeof(dfloat_t));
    if (!_r_) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_z = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_z) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_z_ = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_z_) {        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_M = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_M) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_p = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_p) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_q = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_q) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_p_ = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_p_) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *_q_ = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    if (!_q_) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    cs *_A = analysis->cs_mna_matrix;
    dfloat_t *_b = analysis->mna_vector;
    dfloat_t *_x = analysis->x;
    //initial values:
    //                 _x[] = 0,...,0
    //                 _r = b
    memset(_x,0,mna_dim_size*sizeof(dfloat_t));
    memcpy(_r,_b,mna_dim_size*sizeof(dfloat_t));
    memcpy(_r_,_r,mna_dim_size*sizeof(dfloat_t));
    //init_M(_M,_A,mna_dim_size);
    cs_diagonal_values(_A,_M);
    dfloat_t rho_old = _dot(_r,_r,mna_dim_size);
    dfloat_t norm_b = sqrt(_dot(_b,_b,mna_dim_size));
    if (norm_b == 0)
        norm_b = 1;

    //M^T == M (diagonal)

    int i = 0;
    const int max_iter = (mna_dim_size > 20) ? mna_dim_size : 20;  //max()
    for (i=0; i<max_iter; ++i) {
        _z = init_preconditioner(_M,_z,_r,mna_dim_size);
        _z_ = init_preconditioner(_M,_z_,_r_,mna_dim_size);
        dfloat_t rho = _dot(_r_,_z,mna_dim_size);
#ifdef PRECISION_DOUBLE
        dfloat_t abs_rho = fabs(rho);
#else
        dfloat_t abs_rho = fabsf(rho);
#endif
        if (abs_rho < BI_CG_EPSILON) {
            printf("%s error: algorithm failure (abs(rho) < EPSILON) - exit\n",__FUNCTION__);
            exit(EXIT_FAILURE);
        }
        if (i == 0) {
            memcpy(_p,_z,mna_dim_size*sizeof(dfloat_t));
            memcpy(_p_,_z_,mna_dim_size*sizeof(dfloat_t));
        }
        else {
            dfloat_t beta = rho/rho_old;
            _p = _dot_add(_p,_z,beta,_p,mna_dim_size);
            _p_ = _dot_add(_p_,_z_,beta,_p_,mna_dim_size);
        }
        rho_old = rho;
        //_q = _mult(_q,_A,_p,mna_dim_size);
        memset(_q,0,mna_dim_size*sizeof(dfloat_t));
        if (!cs_gaxpy(_A,_p,_q)) {
			printf("cs_gaxpy() failed - exit.\n");
			exit(EXIT_FAILURE);
		}
        //_q_ = _mult_transposed(_q_,_A,_p_,mna_dim_size);
        memset(_q_,0,mna_dim_size*sizeof(dfloat_t));
        if (!cs_gaxpy_T(_A,_p_,_q_)) {
			printf("cs_gaxpy_T() failed - exit.\n");
			exit(EXIT_FAILURE);
		}
        dfloat_t omega = _dot(_p_,_q,mna_dim_size);
#ifdef PRECISION_DOUBLE
        dfloat_t abs_omega = fabs(rho);
#else
        dfloat_t abs_omega = fabsf(rho);
#endif
        if (abs_omega < BI_CG_EPSILON) {
            printf("%s error: algorithm failure (abs(omega) < EPSILON) - exit\n",__FUNCTION__);
            exit(EXIT_FAILURE);
        }
        dfloat_t alpha = rho/omega;
        _x = _dot_add(_x,_x,alpha,_p,mna_dim_size);
        _r = _dot_add(_r,_r,-alpha,_q,mna_dim_size);
        _r_ = _dot_add(_r_,_r_,-alpha,_q_,mna_dim_size);
        dfloat_t cond = sqrt(_dot(_r,_r,mna_dim_size))/norm_b;
        if (cond < tol)
            break;
    }

    free(_r);
    free(_z);
    free(_M);
    free(_p);
    free(_q);
    free(_r_);
    free(_z_);
    free(_p_);
    free(_q_);
}

static void write_result(FILE *f,
                         struct cmd_print_plot_item *item,
                         struct analysis_info *analysis,
                         const dfloat_t abs_time) {
    dfloat_t value = 0;
    char *name = NULL;

    assert(item->type == 'v' || item->type == 'i');
    if (item->type == 'v') {
        name = item->cnode._node->name;
        unsigned long idx = item->cnode._node->nuid;
        //ground node is always 0
        if (idx)
            value = analysis->x[idx - 1];
    }
    else {
        name = item->cel._el->name;
        unsigned long idx = analysis->n + item->cel._el->idx;
        value = analysis->x[idx];
    }

    int status = fprintf(f,"%s : %5.3f : %+e\n",name,abs_time,value);
    if (status < 0) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
}

static void write_results(struct netlist_info *netlist,
                          struct analysis_info *analysis,
                          const dfloat_t abs_time) {
    DEBUG_MSG("")
    unsigned long i;
    unsigned long j;
    for (i=0; i<netlist->cmd_pool_size; ++i) {
        struct command *cmd = &netlist->cmd_pool[i];
        if (cmd->type == CMD_PRINT || cmd->type == CMD_PLOT)
            for (j=0; j<cmd->print_plot.item_num; ++j)
                write_result(cmd->print_plot.f,
                             &cmd->print_plot.item[j],
                             analysis,abs_time);

    }
}

static int get_sparse(struct command *pool, unsigned long size) {
    if (force_sparse)
        return 1;

    unsigned long i;
    for (i=0; i<size; ++i) {
        struct command *cmd = &pool[size - 1 - i];
        if (cmd->type == CMD_OPTION && cmd->option[CMD_OPT_SPARSE])
            return 1;
    }
    return 0;
}

static inline enum solver option_to_solver(int *option, const int use_sparse) {
    if (use_sparse) {
        if (option[CMD_OPT_SPD] && option[CMD_OPT_ITER])
            return S_SPD_ITER_SPARSE;
        else if (option[CMD_OPT_SPD])
            return S_SPD_SPARSE;
        else if (option[CMD_OPT_ITER])
            return S_ITER_SPARSE;
        return S_LU_SPARSE;
    }
    else if (option[CMD_OPT_SPD] && option[CMD_OPT_ITER])
        return S_SPD_ITER;
    else if (option[CMD_OPT_SPD])
        return S_SPD;
    else if (option[CMD_OPT_ITER])
        return S_ITER;
    return S_LU;
}

static inline enum transient_method option_to_transient(int *option) {
    if (option[CMD_OPT_METHOD_TR])
        return T_TR;
    if (option[CMD_OPT_METHOD_BE])
        return T_BE;
    return T_NONE;
}

static enum solver get_solver(struct command *pool, unsigned long size, const int use_sparse) {
    //last solver wins!

    unsigned long i;
    for (i=0; i<size; ++i) {
        struct command *cmd = &pool[size - 1 - i];
        if (cmd->type == CMD_OPTION) {
            enum solver _solver = option_to_solver(cmd->option,use_sparse);
            if (_solver != S_LU && _solver != S_LU_SPARSE)
                return _solver;
        }
    }
    return use_sparse ? S_LU_SPARSE : S_LU;
}

static enum transient_method get_transient_method(struct command *pool, unsigned long size) {
    //last transient wins!

    enum transient_method _transient = T_NONE;
    enum transient_method _transient_default = T_TR;
    unsigned long i;
    unsigned long j;

    for (j=0; j<size; ++j) {
        struct command *cmd = &pool[size - 1 - j];
        if (cmd->type == CMD_TRAN) {
            _transient = _transient_default;
            break;
        }
    }

    if (_transient == T_NONE)
        return T_NONE;

    for (i=0; i<size; ++i) {
        struct command *cmd = &pool[size - 1 - i];
        if (cmd->type == CMD_OPTION) {
            enum transient_method _transient = option_to_transient(cmd->option);
            if (_transient != T_NONE)
                return _transient;
        }
    }

    return _transient_default;
}

static dfloat_t get_tolerance(struct command *pool, unsigned long size) {
    //last tolerance wins!

    unsigned long i;
    for (i=0; i<size; ++i) {
        struct command *cmd = &pool[size - 1 - i];
        if (cmd->type == CMD_OPTION && cmd->option[CMD_OPT_ITOL])
            return cmd->value;
    }
    return DEFAULT_TOL;
}

static void analyse_init_solver(struct analysis_info *analysis,
                                enum solver _solver) {
    DEBUG_MSG("")
    switch (_solver) {
    case S_SPD:       decomp_cholesky(analysis);  break;
    case S_ITER:                                  break;
    case S_SPD_ITER:                              break;
    case S_LU:        decomp_LU(analysis);        break;

        //sparse versions
    case S_SPD_SPARSE:       decomp_cholesky_sparse(analysis);  break;
    case S_ITER_SPARSE:                                         break;
    case S_SPD_ITER_SPARSE:                                     break;
    case S_LU_SPARSE:        decomp_LU_sparse(analysis);        break;
    }
}

static void analyse_dc_one_step(struct netlist_info *netlist,
                                struct analysis_info *analysis) {
    DEBUG_MSG("")
    dfloat_t tol = analysis->tol;
    switch (analysis->_solver) {
    case S_SPD:       solve_cholesky(analysis);   break;
    case S_ITER:      solve_bi_cg(analysis,tol);  break;
    case S_SPD_ITER:  solve_cg(analysis,tol);     break;
    case S_LU:        solve_LU(analysis);         break;

        //sparse versions
    case S_SPD_SPARSE:       solve_cholesky_sparse(analysis);   break;
    case S_ITER_SPARSE:      solve_bi_cg_sparse(analysis,tol);  break;
    case S_SPD_ITER_SPARSE:  solve_cg_sparse(analysis,tol);     break;
    case S_LU_SPARSE:        solve_LU_sparse(analysis);         break;
    }
}

static void analyse_dc_init(struct cmd_dc *dc,
                            struct netlist_info *netlist,
                            struct analysis_info *analysis,
                            enum solver _solver, dfloat_t tol) {
    DEBUG_MSG("")
    unsigned long _n = analysis->n;

    //init mna_vector
    struct element *el = dc->source._el;
    switch (el->type) {
    case 'v': {
        analysis->mna_vector[_n + el->idx] = dc->begin;
        break;
    }
    case 'i': {
        //we have -A1*S1, therefore we subtract from the final result
        if (el->i->vplus._node->nuid) {
            unsigned long idx = el->i->vplus._node->nuid - 1;
            analysis->mna_vector[idx] -= dc->begin - el->value;
        }
        if (el->i->vminus._node->nuid) {
            unsigned long idx = el->i->vminus._node->nuid - 1;
            analysis->mna_vector[idx] += dc->begin - el->value;
        }
        break;
    }
    default:  assert(0);
    }
}

static void analyse_dc_update(struct cmd_dc *dc,
                              struct netlist_info *netlist,
                              struct analysis_info *analysis,
                              enum solver _solver, dfloat_t tol) {
    DEBUG_MSG("")
    unsigned long _n = analysis->n;
    struct element *el = dc->source._el;
    switch (el->type) {
    case 'v': {
        //update mna_vector
        analysis->mna_vector[_n + el->idx] += dc->step;
        break;
    }
    case 'i': {
        //we have -A1*S1, therefore we subtract from the final result
        if (el->i->vplus._node->nuid) {
            unsigned long idx = el->i->vplus._node->nuid - 1;
            analysis->mna_vector[idx] -= dc->step;
        }
        if (el->i->vminus._node->nuid) {
            unsigned long idx = el->i->vminus._node->nuid - 1;
            analysis->mna_vector[idx] += dc->step;
        }
        break;
    }
    default:  assert(0);
    }
}

static void analyse_dc(struct cmd_dc *dc,
                       struct netlist_info *netlist,
                       struct analysis_info *analysis) {
    enum solver _solver = analysis->_solver;
    dfloat_t tol = analysis->tol;

    DEBUG_MSG("")
    analyse_dc_init(dc,netlist,analysis,_solver,tol);
    unsigned long i;
    unsigned long repeat = (dc->end - dc->begin)/dc->step;
    const dfloat_t abs_time = 0.0;
    for (i=0; i<repeat; ++i) {
        analyse_dc_update(dc,netlist,analysis,_solver,tol);
        analyse_dc_one_step(netlist,analysis);
        write_results(netlist,analysis,abs_time);
    }
}

static void open_logfiles(struct netlist_info *netlist) {
    DEBUG_MSG("")
    unsigned long i;
    for (i=0; i<netlist->cmd_pool_size; ++i) {
        struct command *cmd = &netlist->cmd_pool[i];
        if (cmd->type == CMD_PRINT || cmd->type == CMD_PLOT) {
            FILE *f = fopen(cmd->print_plot.logfile,"w");
            if (!f) {
                perror(__FUNCTION__);
                exit(EXIT_FAILURE);
            }
            cmd->print_plot.f = f;
        }
    }
}

static void close_logfiles(struct netlist_info *netlist) {
    DEBUG_MSG("")
    unsigned long i;
    for (i=0; i<netlist->cmd_pool_size; ++i) {
        struct command *cmd = &netlist->cmd_pool[i];
        if (cmd->type == CMD_PRINT || cmd->type == CMD_PLOT) {
            FILE *f = cmd->print_plot.f;
            int status;
            status = fflush(f);
            if (status == EOF) {
                perror(__FUNCTION__);
                exit(EXIT_FAILURE);
            }
            status = fclose(f);
            if (status == EOF) {
                perror(__FUNCTION__);
                exit(EXIT_FAILURE);
            }
        }
    }
}

static void analyse_log(struct analysis_info *analysis) {
    if (!debug_on)
        return;

    const int use_sparse = analysis->use_sparse;

    DEBUG_MSG("writing A and b to files ...")
    unsigned long mna_dim_size = analysis->n + analysis->el_group2_size;
    if (use_sparse)
        cs_print(analysis->cs_mna_matrix,"mna_sparse_matrix.log",0);
    else
        fprint_dfloat_array("mna_dense_matrix.log",
                            mna_dim_size,mna_dim_size,analysis->mna_matrix);
    fprint_dfloat_array("mna_b_vector.log",
                        mna_dim_size,1,analysis->mna_vector);
}

static struct command *get_dc(struct command *pool, unsigned long size) {
    unsigned long i;
    for (i=0; i<size; ++i) {
        struct command *dc_cmd = &pool[i];
        if (dc_cmd->type == CMD_DC)
            return dc_cmd;
    }
    return NULL;
}

static struct command *get_tran(struct command *pool, unsigned long size) {
    unsigned long i;
    for (i=0; i<size; ++i) {
        struct command *tran_cmd = &pool[i];
        if (tran_cmd->type == CMD_TRAN)
            return tran_cmd;
    }
    return NULL;
}

static void analyse_transient_update(struct netlist_info *netlist,
                                     struct analysis_info *analysis,
                                     const dfloat_t abs_time) {
    DEBUG_MSG("")
    unsigned long i;
    for (i=0; i<netlist->el_group1_size; ++i) {
        struct element *el = &netlist->el_group1_pool[i];
        if (el->type == 'i') {
            struct _transient_ *tran = el->i->transient;
            if (!tran)
                continue;
            if (el->i->vplus._node->nuid) {
                unsigned long idx = el->i->vplus._node->nuid - 1;
                switch (tran->type) {
                case TR_EXP:
                    analysis->mna_vector[idx] = -tran->update.exp(&tran->data.exp,abs_time);
                    break;
                case TR_SIN:
                    analysis->mna_vector[idx] = -tran->update.sin(&tran->data.sin,abs_time);
                    break;
                case TR_PULSE:
                    analysis->mna_vector[idx] = -tran->update.pulse(&tran->data.pulse,abs_time);
                    break;
                case TR_PWL:
                    analysis->mna_vector[idx] = -tran->update.pwl(&tran->data.pwl,abs_time);
                    break;
                }
            }
            if (el->i->vminus._node->nuid) {
                unsigned long idx = el->i->vminus._node->nuid - 1;
                switch (tran->type) {
                case TR_EXP:
                    analysis->mna_vector[idx] = tran->update.exp(&tran->data.exp,abs_time);
                    break;
                case TR_SIN:
                    analysis->mna_vector[idx] = tran->update.sin(&tran->data.sin,abs_time);
                    break;
                case TR_PULSE:
                    analysis->mna_vector[idx] = tran->update.pulse(&tran->data.pulse,abs_time);
                    break;
                case TR_PWL:
                    analysis->mna_vector[idx] = tran->update.pwl(&tran->data.pwl,abs_time);
                    break;
                }
            }
        }
    }

    for (i=0; i<netlist->el_group2_size; ++i) {
        struct element *el = &netlist->el_group2_pool[i];
        if (el->type == 'v') {
            struct _transient_ *tran = el->v->transient;
            if (!tran)
                continue;
            unsigned long idx = analysis->n + el->idx;
            switch (tran->type) {
            case TR_EXP:
                analysis->mna_vector[idx] = tran->update.exp(&tran->data.exp,abs_time);
                break;
            case TR_SIN:
                analysis->mna_vector[idx] = tran->update.sin(&tran->data.sin,abs_time);
                break;
            case TR_PULSE:
                analysis->mna_vector[idx] = tran->update.pulse(&tran->data.pulse,abs_time);
                break;
            case TR_PWL:
                analysis->mna_vector[idx] = tran->update.pwl(&tran->data.pwl,abs_time);
                break;
            }
        }
    }
}

static void analysis_transient_euler_init(struct analysis_info *analysis,
                                          struct cmd_tran *transient) {
    DEBUG_MSG("")
    const int use_sparse = analysis->use_sparse;
    unsigned long mna_dim_size = analysis->n + analysis->el_group2_size;
    //calculate G + 1/h * C

    dfloat_t h = 1/transient->time_step;

    if (use_sparse) {
        cs *tmp = analysis->cs_mna_matrix;
        analysis->cs_mna_matrix =
            cs_add(analysis->cs_mna_matrix,analysis->cs_transient_matrix,1,h);
        cs_free(tmp);
        decomp_LU_sparse(analysis);

        //compute h*C
        tmp = analysis->cs_transient_matrix;
        analysis->cs_transient_matrix =
            cs_add(analysis->cs_transient_matrix,analysis->cs_transient_matrix,h,0);
        cs_free(tmp);
    }
    else {
        _dot_add(analysis->mna_matrix,analysis->mna_matrix,h,
                 analysis->transient_matrix,mna_dim_size*mna_dim_size);
        decomp_LU(analysis);
    }
}

static void analyse_transient_euler_one_step(struct netlist_info *netlist,
                                             struct analysis_info *analysis,
                                             dfloat_t *x_prev, const dfloat_t time_step) {
    DEBUG_MSG("")
    const int use_sparse = analysis->use_sparse;

    unsigned long mna_dim_size = analysis->n + analysis->el_group2_size;
    dfloat_t *tmp = (dfloat_t *)malloc(mna_dim_size * sizeof(dfloat_t));
    if (!tmp) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    if (use_sparse) {
        memcpy(tmp,analysis->mna_vector,mna_dim_size*sizeof(dfloat_t));
        if (!cs_gaxpy(analysis->cs_transient_matrix,x_prev,tmp)) {
            printf("cs_gaxpy() failed - exit.\n");
            exit(EXIT_FAILURE);
        }

        dfloat_t *orig_mna_vector = analysis->mna_vector;
        analysis->mna_vector = tmp;

        solve_LU_sparse(analysis);
        analysis->mna_vector = orig_mna_vector;
    }
    else {
        _mult(tmp,analysis->transient_matrix,x_prev,mna_dim_size);
        _dot_add(tmp,analysis->mna_vector,1/time_step,tmp,mna_dim_size);

        dfloat_t *orig_mna_vector = analysis->mna_vector;
        analysis->mna_vector = tmp;

        solve_LU(analysis);
        analysis->mna_vector = orig_mna_vector;
    }
    free(tmp);
}

static void analysis_transient_trapezoid_init(struct analysis_info *analysis,
                                              struct cmd_tran *transient) {
    DEBUG_MSG("")
    const int use_sparse = analysis->use_sparse;

    unsigned long mna_dim_size = analysis->n + analysis->el_group2_size;

    dfloat_t h = 2/transient->time_step;

    if (use_sparse) {
        cs *tmp = analysis->cs_mna_matrix;
        analysis->cs_mna_matrix =
            cs_add(analysis->cs_mna_matrix,analysis->cs_transient_matrix,1,h);
        decomp_LU_sparse(analysis);

        //compute -(G - h * C)
        cs *tmp2 = analysis->cs_transient_matrix;
        analysis->cs_transient_matrix =
            cs_add(tmp,analysis->cs_transient_matrix,-1,h);

        cs_free(tmp);
        cs_free(tmp2);
    }
    else {
        dfloat_t *left_array = (dfloat_t *)malloc(mna_dim_size * mna_dim_size * sizeof(dfloat_t));
        if (!left_array) {
            perror(__FUNCTION__);
            exit(EXIT_FAILURE);
        }

        dfloat_t *right_array = (dfloat_t *)malloc(mna_dim_size * mna_dim_size * sizeof(dfloat_t));
        if (!right_array) {
            perror(__FUNCTION__);
            exit(EXIT_FAILURE);
        }

        //calculate G + h * C
        _dot_add(left_array,analysis->mna_matrix,h,
                 analysis->transient_matrix,mna_dim_size*mna_dim_size);

        //calculate G - h * C
        _dot_add(right_array,analysis->mna_matrix,-h,
                 analysis->transient_matrix,mna_dim_size*mna_dim_size);

        dfloat_t *old;
        old = analysis->mna_matrix;
        analysis->mna_matrix = left_array;
        free(old);

        old = analysis->transient_matrix;
        analysis->transient_matrix = right_array;
        free(old);

        decomp_LU(analysis);
    }
}

static void analyse_transient_trapezoid_one_step(struct netlist_info *netlist,
                                                 struct analysis_info *analysis,
                                                 dfloat_t *x_prev, dfloat_t *vector_prev,
                                                 const dfloat_t time_step) {
    DEBUG_MSG("")
    const int use_sparse = analysis->use_sparse;

    unsigned long mna_dim_size = analysis->n + analysis->el_group2_size;
    dfloat_t *tmp = (dfloat_t *)malloc(mna_dim_size * sizeof(dfloat_t));
    if (!tmp) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    if (use_sparse) {
        if (!cs_gaxpy(analysis->cs_transient_matrix,x_prev,vector_prev)) {
            printf("cs_gaxpy() failed - exit.\n");
            exit(EXIT_FAILURE);
        }
        _dot_add(tmp,vector_prev,1,analysis->mna_vector,mna_dim_size);

        dfloat_t *orig_mna_vector = analysis->mna_vector;
        analysis->mna_vector = tmp;

        solve_LU_sparse(analysis);
        analysis->mna_vector = orig_mna_vector;
    }
    else {
        _mult(tmp,analysis->transient_matrix,x_prev,mna_dim_size);
        _dot_add(tmp,vector_prev,-1,tmp,mna_dim_size);
        _dot_add(tmp,analysis->mna_vector,1,tmp,mna_dim_size);

        dfloat_t *orig_mna_vector = analysis->mna_vector;
        analysis->mna_vector = tmp;

        solve_LU(analysis);
        analysis->mna_vector = orig_mna_vector;
    }
    free(tmp);
}

void analyse_transient(struct cmd_tran *transient,
                       struct netlist_info *netlist,
                       struct analysis_info *analysis) {
    enum transient_method _transient_method = analysis->_transient_method;

    //we are at dc point, see analyse_mna()
    unsigned long mna_dim_size = analysis->n + analysis->el_group2_size;
    dfloat_t *x_prev = (dfloat_t *)malloc(mna_dim_size * sizeof(dfloat_t));
    if (!x_prev) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    unsigned long i;
    unsigned long time_slots = ceil(transient->fin_time/transient->time_step);

    switch (_transient_method) {
    case T_NONE:  assert(0);  break;
    case T_TR: {
        dfloat_t *vector_prev = (dfloat_t *)malloc(mna_dim_size * sizeof(dfloat_t));
        if (!vector_prev) {
            perror(__FUNCTION__);
            exit(EXIT_FAILURE);
        }

        analysis_transient_trapezoid_init(analysis,transient);
        for (i=0; i<time_slots; ++i) {
            memcpy(x_prev,analysis->x,mna_dim_size * sizeof(dfloat_t));
            memcpy(vector_prev,analysis->mna_vector,mna_dim_size * sizeof(dfloat_t));
            dfloat_t abs_time = i * transient->time_step;
            analyse_transient_update(netlist,analysis,abs_time);
            analyse_transient_trapezoid_one_step(netlist,analysis,x_prev,
                                                 vector_prev,transient->time_step);
            write_results(netlist,analysis,abs_time);
        }
        free(vector_prev);
        break;
    }
    case T_BE:
        analysis_transient_euler_init(analysis,transient);
        for (i=0; i<time_slots; ++i) {
            memcpy(x_prev,analysis->x,mna_dim_size * sizeof(dfloat_t));
            dfloat_t abs_time = i * transient->time_step;
            analyse_transient_update(netlist,analysis,abs_time);
            analyse_transient_euler_one_step(netlist,analysis,x_prev,
                                             transient->time_step);
            write_results(netlist,analysis,abs_time);
        }
        break;
    }

    free(x_prev);
}

void analyse_mna(struct netlist_info *netlist, struct analysis_info *analysis) {
    assert(netlist);
    assert(analysis);

    memset(analysis,0,sizeof(struct analysis_info));

    analysis->use_sparse = get_sparse(netlist->cmd_pool,netlist->cmd_pool_size);
    analysis->_solver =
        get_solver(netlist->cmd_pool,netlist->cmd_pool_size,analysis->use_sparse);
    analysis->_transient_method =
        get_transient_method(netlist->cmd_pool,netlist->cmd_pool_size);
    analysis->tol = get_tolerance(netlist->cmd_pool,netlist->cmd_pool_size);

    if (analysis->use_sparse)
        DEBUG_MSG("use sparse matrices");

    analysis_init(netlist,analysis);
    analyse_log(analysis);

    open_logfiles(netlist);

    struct command *dc_cmd = get_dc(netlist->cmd_pool,netlist->cmd_pool_size);
    if (dc_cmd)
        analyse_dc(&dc_cmd->dc,netlist,analysis);
    else {
        analyse_dc_one_step(netlist,analysis);
        if (analysis->_transient_method != T_NONE) {
            struct command *tran_cmd = get_tran(netlist->cmd_pool,netlist->cmd_pool_size);
            analyse_transient(&tran_cmd->transient,netlist,analysis);
        }
    }

    close_logfiles(netlist);
}

void fprint_dfloat_array(const char *filename,
                         unsigned long row, unsigned long col, dfloat_t *p) {
    FILE *file = fopen(filename,"w");
    if (!file) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    unsigned long i;
    unsigned long j;

    fprintf(file,"==============================================\n");
    fprintf(file,"      ");
    for (j=0; j<col; ++j) {
        fprintf(file,"%9lu|",j);
    }
    fprintf(file,"\n     ___________________________________________\n");

    for (i=0; i<row; ++i) {
        fprintf(file,"%3lu | ",i + 1);
        for (j=0; j<col; ++j) {
            dfloat_t val = p[i*col + j];
            fprintf(file,"%+.2e ",val);
        }
        fprintf(file,"\n");
    }

    if (fflush(file) == EOF) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    if (fclose(file) == EOF) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
}

void print_dfloat_array(unsigned long row, unsigned long col, dfloat_t *p) {
    unsigned long i;
    unsigned long j;

    printf("==============================================\n");
    printf("      ");
    for (j=0; j<col; ++j) {
        printf("%9lu|",j);
    }
    printf("\n     ___________________________________________\n");

    for (i=0; i<row; ++i) {
        printf("%3lu | ",i + 1);
        for (j=0; j<col; ++j) {
            dfloat_t val = p[i*col + j];
            printf("%+.2e ",val);
        }
        printf("\n");
    }
}

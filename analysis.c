#include "analysis.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <gsl/gsl_linalg.h>
#include <math.h>

#define BI_CG_EPSILON 1e-14

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

    printf("analysis: populating matrices ...\n");

    unsigned long i;
    unsigned long j;

    //populate MNA Matrix

    //ignore ground node
    for (i=1; i<=_n; ++i) {
        struct node *_node = &netlist->node_pool[i];
        for (j=0; j<_node->refs; ++j) {
            struct element *el = _node->attached_el[j]._el;
            switch (el->type) {
            case 'r': {
                assert(_node == el->r->vplus._node || _node == el->r->vminus._node);
                unsigned long this_node  = (_node == el->r->vplus._node) ? el->r->vplus.nuid : el->r->vminus.nuid;
                unsigned long other_node = (_node != el->r->vplus._node) ? el->r->vplus.nuid : el->r->vminus.nuid;
                dfloat_t value = 1/el->value;

                //NOTE: all rows are moved up by one (we ingore the ground node)
                if (this_node--) {
                    mna_matrix[this_node*mna_dim_size + this_node] += value;        //diagonal entry
                    if (other_node--)
                        mna_matrix[this_node*mna_dim_size + other_node] += -value;  //off diagonal entry
                }
                break;
            }
            case 'v': {
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
                mna_vector[_n + el->idx] = el->value;
                break;
            }
            case 'l': {
                assert(_node == el->l->vplus._node || _node == el->l->vminus._node);

                //ignore ground node
                if (_node->nuid) {
                    unsigned long row = _node->nuid - 1;
                    dfloat_t value = (_node == el->l->vplus._node) ? +1 : -1;
                    //dfloat_t value = (_node == el->l->vplus._node) ? el->value : -el->value;

                    //group2 element, populate A2
                    mna_matrix[row*mna_dim_size + _n + el->idx] = value;

                    //group2 element, populate A2 transposed
                    mna_matrix[(_n + el->idx)*mna_dim_size + row] = value;
                }
                break;
            }
            case 'i': {
                assert(_node == el->i->vplus._node || _node == el->i->vminus._node);
                //we have -A1*S1, therefore we subtract from the final result
                mna_vector[_node->nuid - 1] -=
                    (_node == el->i->vminus._node) ? -el->value : el->value;
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
    analysis->v = v;
    analysis->u = u;
    analysis->mna_matrix = mna_matrix;
    analysis->mna_vector = mna_vector;
    analysis->x = x;
    analysis->LU_perm = NULL;
}

void decomp_LU(struct analysis_info *analysis) {
    printf("debug: exec %s\n",__FUNCTION__);
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
    printf("debug: exec %s\n",__FUNCTION__);
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
    printf("debug: exec %s\n",__FUNCTION__);
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
    printf("debug: exec %s\n",__FUNCTION__);
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
    printf("debug: exec %s\n",__FUNCTION__);
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

void solve_bi_cg(struct analysis_info *analysis, dfloat_t tol) {
    printf("debug: exec %s\n",__FUNCTION__);
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

static void write_result(FILE *f, struct cmd_print_plot_item *item, struct analysis_info *analysis) {
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

    int status = fprintf(f,"%s:%5.2g\n",name,value);
    if (status < 0) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
}

static void write_results(struct netlist_info *netlist, struct analysis_info *analysis) {
    unsigned long i;
    unsigned long j;
    for (i=0; i<netlist->cmd_pool_size; ++i) {
        struct command *cmd = &netlist->cmd_pool[i];
        if (cmd->type == CMD_PRINT || cmd->type == CMD_PLOT)
            for (j=0; j<cmd->print_plot.item_num; ++j)
                write_result(cmd->print_plot.f,&cmd->print_plot.item[j],analysis);

    }
}

enum solver {
    S_LU = 0,
    S_SPD,
    S_ITER,
    S_SPD_ITER
};

static inline enum solver option_to_solver(int *option) {
    if (option[CMD_OPT_SPD] && option[CMD_OPT_ITER])
        return S_SPD_ITER;
    else if (option[CMD_OPT_SPD])
        return S_SPD;
    else if (option[CMD_OPT_ITER])
        return S_ITER;
    return S_LU;
}

static enum solver get_solver(struct command *pool, unsigned long size) {
    //last solver wins!

    unsigned long i;
    for (i=0; i<size; ++i) {
        struct command *cmd = &pool[size - 1 - i];
        if (cmd->type == CMD_OPTION) {
            enum solver _solver = option_to_solver(cmd->option);
            if (_solver != S_LU)
                return _solver;
        }
    }
    return S_LU;
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

static int use_sparse(struct command *pool, unsigned long size) {
    unsigned long i;
    for (i=0; i<size; ++i) {
        struct command *cmd = &pool[size - 1 - i];
        if (cmd->type == CMD_OPTION && cmd->option[CMD_OPT_SPARSE])
            return 1;
    }
    return 0;
}

static void analyse_init_solver(struct analysis_info *analysis,
                                enum solver _solver) {
    switch (_solver) {
    case S_SPD:  decomp_cholesky(analysis);  break;
    case S_LU:   decomp_LU(analysis);        break;
    default:                                 break;
    }
}

static void analyse_one_step(struct netlist_info *netlist,
                             struct analysis_info *analysis,
                             enum solver _solver, dfloat_t tol) {
    switch (_solver) {
    case S_SPD:       solve_cholesky(analysis);   break;
    case S_ITER:      solve_bi_cg(analysis,tol);  break;
    case S_SPD_ITER:  solve_cg(analysis,tol);     break;
    case S_LU:        solve_LU(analysis);         break;
    }
    write_results(netlist,analysis);
}

static void analyse_dc_init(struct cmd_dc *dc,
                            struct netlist_info *netlist,
                            struct analysis_info *analysis,
                            enum solver _solver, dfloat_t tol) {
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
                       struct analysis_info *analysis,
                       enum solver _solver, dfloat_t tol) {
    analyse_dc_init(dc,netlist,analysis,_solver,tol);
    unsigned long i;
    unsigned long repeat = (dc->end - dc->begin)/dc->step;
    for (i=0; i<repeat; ++i) {
        analyse_dc_update(dc,netlist,analysis,_solver,tol);
        analyse_one_step(netlist,analysis,_solver,tol);
    }
}

static void open_logfiles(struct netlist_info *netlist) {
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

void analyse_mna(struct netlist_info *netlist, struct analysis_info *analysis) {
    analysis_init(netlist,analysis);

    enum solver _solver = get_solver(netlist->cmd_pool,netlist->cmd_pool_size);
    dfloat_t tol = get_tolerance(netlist->cmd_pool,netlist->cmd_pool_size);
    printf("debug: tolerance value = %g\n",tol);

    struct command *dc_cmd = NULL;
    unsigned long i;
    for (i=0; i<netlist->cmd_pool_size; ++i) {
        if (netlist->cmd_pool[i].type == CMD_DC) {
            dc_cmd = &netlist->cmd_pool[i];
            break;
        }
    }

    analyse_init_solver(analysis,_solver);
    open_logfiles(netlist);
    if (dc_cmd)
        analyse_dc(&dc_cmd->dc,netlist,analysis,_solver,tol);
    else
        analyse_one_step(netlist,analysis,_solver,tol);
    close_logfiles(netlist);
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

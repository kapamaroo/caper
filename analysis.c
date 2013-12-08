#include "analysis.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <gsl/gsl_linalg.h>
#include <math.h>

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
#if 1
            else if (el->type == 'l') {
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
            }
#endif
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

    gsl_linalg_cholesky_decomp(&Aview.matrix);
    gsl_linalg_cholesky_solve(&Aview.matrix,&bview.vector,&x.vector);
}

static inline dfloat_t *init_preconditioner(dfloat_t *M, dfloat_t *z, dfloat_t *r, unsigned long mna_dim_size) {
    unsigned long i;
    for (i=0; i<mna_dim_size; ++i)
        z[i] = (M[i] != 0) ? r[i]/M[i] : 0;
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

void solve_cg(struct analysis_info *analysis, dfloat_t tol) {
    unsigned long _n = analysis->n;
    unsigned long el_group2_size = analysis->el_group2_size;
    unsigned long mna_dim_size = _n + el_group2_size;

    //initial values:
    //                 _x[] = 0,...,0
    //                 _r = b

    //dfloat_t *_x = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    dfloat_t *_x = (dfloat_t*)calloc(mna_dim_size,sizeof(dfloat_t));
    if (!_x) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    //dfloat_t *_r = (dfloat_t*)malloc(mna_dim_size*sizeof(dfloat_t));
    dfloat_t *_r = (dfloat_t*)calloc(mna_dim_size,sizeof(dfloat_t));
    if (!_r) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    dfloat_t *_b = analysis->mna_vector;
    memcpy(_r,_b,mna_dim_size);

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

    unsigned long k;
    for (k=0; k<mna_dim_size; ++k)
        _M[k] = analysis->mna_matrix[k*mna_dim_size + k];

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
            memcpy(_p,_z,mna_dim_size);
        else {
            dfloat_t beta = rho/rho_old;
            _p = _dot_add(_p,_z,beta,_p,mna_dim_size);
        }
        rho_old = rho;
        _q = _mult(_q,analysis->mna_matrix,_p,mna_dim_size);
        dfloat_t alpha = rho/_dot(_p,_q,mna_dim_size);
        _x = _dot_add(_x,_x,alpha,_p,mna_dim_size);
        _r = _dot_add(_r,_r,-alpha,_q,mna_dim_size);
        dfloat_t cond = sqrt(_dot(_r,_r,mna_dim_size))/norm_b;
        if (cond < tol)
            break;
    }
}

static int get_cmd_opt(struct command *pool, unsigned long size,
                       enum cmd_option_type option_type) {
    unsigned long i;
    for (i=0; i<size; ++i)
        if (pool[i].type == CMD_OPTION &&
            pool[i].option_type == option_type)
            return 1;
    return 0;
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

static void analyse_dc(struct cmd_dc *dc, struct netlist_info *netlist,
                       struct analysis_info *analysis) {
    unsigned long _n = analysis->n;
    unsigned long mna_dim_size = _n + analysis->el_group2_size;
    dfloat_t *backup_mna_matrix = analysis->mna_matrix;
    dfloat_t *backup_mna_vector = analysis->mna_vector;

    dfloat_t *new_mna_matrix = (dfloat_t*)malloc(mna_dim_size * mna_dim_size * sizeof(dfloat_t));
    if (!new_mna_matrix) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    dfloat_t *new_mna_vector = (dfloat_t*)malloc(mna_dim_size * sizeof(dfloat_t));
    if (!new_mna_vector) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    int use_cholesky = get_cmd_opt(netlist->cmd_pool,
                                   netlist->cmd_pool_size,
                                   CMD_OPT_SPD);

    unsigned long i;
    dfloat_t dc_value = dc->begin;
    unsigned long repeat = (dc->end - dc->begin)/dc->step;
    for (i=0; i<repeat; ++i,dc_value+=dc->step) {
        memcpy(new_mna_matrix,backup_mna_matrix,mna_dim_size*mna_dim_size*sizeof(dfloat_t));
        memcpy(new_mna_vector,backup_mna_vector,mna_dim_size*sizeof(dfloat_t));
        analysis->mna_matrix = new_mna_matrix;
        analysis->mna_vector = new_mna_vector;

        //update mna_matrix and mna_vector
        struct element *el = dc->source._el;
        switch (el->type) {
        case 'v':
            if (el->v->vplus._node->nuid) {
                unsigned long row = el->_vi->vplus._node->nuid - 1;
                dfloat_t value = 1;

                //group2 element, populate A2
                analysis->mna_matrix[row*mna_dim_size + _n + el->idx] = value;

                //group2 element, populate A2 transposed
                analysis->mna_matrix[(_n + el->idx)*mna_dim_size + row] = value;
            }

            if (el->v->vminus._node->nuid) {
                unsigned long row = el->_vi->vminus._node->nuid - 1;
                dfloat_t value = -1;

                //group2 element, populate A2
                analysis->mna_matrix[row*mna_dim_size + _n + el->idx] = value;

                //group2 element, populate A2 transposed
                analysis->mna_matrix[(_n + el->idx)*mna_dim_size + row] = value;
            }

            analysis->mna_vector[_n + el->idx] = dc_value;
            break;
        case 'i':
            //we have -A1*S1, therefore we subtract from the final result
            if (el->i->vplus._node->nuid) {
                unsigned long idx = el->i->vplus._node->nuid;
                analysis->mna_vector[idx] -= el->value;
            }
            if (el->i->vminus._node->nuid) {
                unsigned long idx = el->i->vminus._node->nuid;
                analysis->mna_vector[idx] -= -el->value;
            }

            break;
        default:  assert(0);
        }

        if (use_cholesky)
            solve_cholesky(analysis);
        else
            solve_LU(analysis);

        write_results(netlist,analysis);

        //printf("\n***    repeat(%4lu) : Solution Vector (x)\n",i);
        //print_dfloat_array(mna_dim_size,1,analysis->x);
    }

    //restore original mna_matrix and mna_vector
    analysis->mna_matrix = backup_mna_matrix;
    analysis->mna_vector = backup_mna_vector;
    free(new_mna_matrix);
    free(new_mna_vector);
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
    unsigned long mna_dim_size = analysis->n + analysis->el_group2_size;
    printf("\n***    MNA Matrix (before)\n");
    print_dfloat_array(mna_dim_size,mna_dim_size,analysis->mna_matrix);

    int use_cholesky = get_cmd_opt(netlist->cmd_pool,
                                   netlist->cmd_pool_size,
                                   CMD_OPT_SPD);

    open_logfiles(netlist);

    unsigned long i;
    for (i=0; i<netlist->cmd_pool_size; ++i) {
        struct command *cmd = &netlist->cmd_pool[i];
        if (cmd->type == CMD_DC) {
            analyse_dc(&cmd->dc,netlist,analysis);
            close_logfiles(netlist);
            return;
        }
    }

    if (use_cholesky)
        solve_cholesky(analysis);
    else
        solve_LU(analysis);

    write_results(netlist,analysis);
    close_logfiles(netlist);
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

#ifndef __DATATYPES_H__
#define __DATATYPES_H__

#include <stdio.h>

#define PRECISION_DOUBLE

#ifdef PRECISION_DOUBLE
typedef double dfloat_t;
#else
typedef float dfloat_t;
#endif

enum connection_type {
    CONN_VPLUS = 0,
    CONN_VMINUS,
    CONN_BJT_C,
    CONN_BJT_E,
    CONN_BJT_B,
    CONN_MOS_S,
    CONN_MOS_D,
    CONN_MOS_G,
    CONN_MOS_B
};

enum cmd_type {
    CMD_OPTION = 0,
    CMD_DC,
    CMD_PLOT,
    CMD_PRINT,
    CMD_BAD_COMMAND  //must be last
};

enum cmd_option_type {
    CMD_OPT_SPD = 0,
    CMD_OPT_ITER,
    CMD_OPT_ITOL,
    CMD_OPT_SPARSE,
    CMD_OPT_BAD_OPTION  //must be last
};

#define CMD_SIZE (CMD_BAD_COMMAND)
#define CMD_OPT_SIZE (CMD_OPT_BAD_OPTION)

#define DEFAULT_TOL 1e-3

struct node;
struct element;

/* Containers act like smart pointers.
   We need them to keep track of the memory address of data in case it changes
   due to realloc().
*/

struct container_node {
    unsigned long nuid;
    struct node *_node;
};

struct container_element {
    char type;
    unsigned long idx;
    struct element *_el;
};

struct cmd_print_plot_item {
    char type;
    union {
        struct container_node cnode;
        struct container_element cel;
    };
};

#define MAX_PRINT_PLOT_ITEMS 16
#define MAX_LOG_FILENAME 16

struct cmd_print_plot {
    unsigned int item_num;
    struct cmd_print_plot_item item[MAX_PRINT_PLOT_ITEMS];
    char *logfile;
    FILE *f;
};

struct cmd_dc {
    struct container_element source;
    dfloat_t begin;
    dfloat_t end;
    dfloat_t step;
};

struct command {
    enum cmd_type type;
    dfloat_t value;
    union {
        int option[CMD_OPT_BAD_OPTION + 1];
        struct cmd_dc dc;
        struct cmd_print_plot print_plot;
    };
};

struct node {
    unsigned long nuid;
    char *name;
    dfloat_t value;
    unsigned long refs;
    unsigned long el_size;
    struct container_element *attached_el;
};

struct _passive_ {
    struct container_node vplus;
    struct container_node vminus;
};

enum transient_type {
    TR_EXP =0,
    TR_SIN,
    TR_PULSE,
    TR_PWL
};

struct transient_exp {
    dfloat_t (*call)(struct transient_exp *data);
    dfloat_t i1;
    dfloat_t i2;
    dfloat_t td1;  // int ??
    dfloat_t tc1;
    dfloat_t td2;  // int ??
    dfloat_t tc2;
};

struct transient_sin {
    dfloat_t (*call)(struct transient_sin *data);
    dfloat_t i1;
    dfloat_t ia;
    dfloat_t fr;  // int ??
    dfloat_t td;  // int ??
    dfloat_t df;  // int ??
    dfloat_t ph;  // int ??
};

struct transient_pulse {
    dfloat_t (*call)(struct transient_pulse *data);
    dfloat_t i1;
    dfloat_t i2;
    dfloat_t td;  // int ??
    dfloat_t tr;
    dfloat_t tf;
    dfloat_t pw;
    dfloat_t per;  // int ??
    unsigned long k;  // internal helper
};

struct transient_pwl_pair {
    dfloat_t time;
    dfloat_t value;
};

struct transient_pwl {
    dfloat_t (*call)(struct transient_pwl *data);
    struct transient_pwl_pair *pair;
    unsigned long size;
    unsigned long next;
};

struct _transient_ {
    enum transient_type type;
    union {
        struct transient_exp exp;
        struct transient_sin sin;
        struct transient_pulse pulse;
        struct transient_pwl pwl;
        void *raw_ptr;
    };
};

struct _source_ {
    struct container_node vplus;
    struct container_node vminus;
    struct _transient_ *transient;
};

enum nonlinear_model_type {
    MODEL_BJT_NONE,
    MODEL_MOS_NOME,
    MODEL_DIODE_NONE
};

struct nonlinear_model {
    char *name;
    enum nonlinear_model_type type;
};

struct _mos_ {
    struct container_node s;
    struct container_node d;
    struct container_node g;
    struct container_node b;
    struct nonlinear_model model;
    union {
        dfloat_t l;
        dfloat_t length;
    };
    union {
        dfloat_t w;
        dfloat_t width;
    };
};

struct _bjt_ {
    struct container_node c;
    struct container_node e;
    struct container_node b;
    struct nonlinear_model model;
    dfloat_t area;
};

struct _diode_ {
    struct container_node vplus;
    struct container_node vminus;
    struct nonlinear_model model;
    dfloat_t area;
};

struct element {
    /* common */
    unsigned long euid;
    char *name;
    dfloat_t value;

    /* pin classification */
    char type;
    unsigned long idx;  //to which pool depends on type
    union {
        struct _source_ *_vi;
        struct _source_ *v;
        struct _source_ *i;

        struct _passive_ *_rcl;
        struct _passive_ *r;
        struct _passive_ *c;
        struct _passive_ *l;

        struct _mos_ *mos;
        struct _bjt_ *bjt;
        struct _diode_ *diode;

        void *raw_ptr;
    };
};

struct netlist_info {
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

    struct node *node_pool;
    struct element *el_group1_pool;
    struct element *el_group2_pool;

    unsigned long cmd_pool_size;
    struct command *cmd_pool;
};

#endif

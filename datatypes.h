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
    CMD_BAD_COMMAND
};

enum cmd_option_type {
    CMD_OPT_SPD = 0,
    CMD_OPT_BAD_OPTION
};

#define CMD_SIZE (CMD_PRINT + 1)
#define CMD_OPT_SIZE (CMD_OPT_SPD + 1)

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
    union {
        enum cmd_option_type option_type;
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

struct _source_ {
    struct container_node vplus;
    struct container_node vminus;
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

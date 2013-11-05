#ifndef __DATATYPES_H__
#define __DATATYPES_H__

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

#define CONN_LAST_TYPE CONN_MOS_B
#define CONN_BITS 4
#define CONN_MASK ((1 << (CONN_BITS)) - 1)

struct node {
    unsigned long nuid;
    char *name;
    double value;
    unsigned long refs;
    unsigned long el_size;
    unsigned long *element;
};

struct container_node {
    unsigned long nuid;
    struct node *_node;
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
        double l;
        double length;
    };
    union {
        double w;
        double width;
    };
};

struct _bjt_ {
    struct container_node c;
    struct container_node e;
    struct container_node b;
    struct nonlinear_model model;
    double area;
};

struct _diode_ {
    struct container_node vplus;
    struct container_node vminus;
    struct nonlinear_model model;
    double area;
};

struct element {
    /* common */
    unsigned long euid;
    char *name;
    double value;

    /* pin classification */
    char type;
    union {
        struct _source_ _vi;
        struct _source_ v;
        struct _source_ i;

        struct _passive_ _rcl;
        struct _passive_ r;
        struct _passive_ c;
        struct _passive_ l;

        struct _mos_ mos;
        struct _bjt_ bjt;
        struct _diode_ diode;
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
};

enum connection_type get_conn_type(unsigned long annotated_id);
unsigned long set_conn_type(unsigned long id, enum connection_type type);

#endif

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
#define CONN_TYPE_BITS 4
#define CONN_TYPE_MASK ((1 << (CONN_TYPE_BITS)) - 1)

enum connection_el_group {
    CONN_EL_GROUP1 = 0,
    CONN_EL_GROUP2
};

#define CONN_EL_GROUP_BITS 1
#define CONN_EL_GROUP_MASK ((1 << (CONN_EL_GROUP_BITS)) - 1)

struct connection_info {
    enum connection_type type;
    enum connection_el_group group;
};

#define CONN_INFO_BITS (CONN_TYPE_BITS + CONN_EL_GROUP_BITS)
#define CONN_INFO_MASK (((CONN_TYPE_MASK) << (CONN_EL_GROUP_BITS)) \
                        | (CONN_EL_GROUP_MASK))

/*
  |            sizeof(unsigned long)             |
   ----------------------------------------------
  |  uid  | CONN_TYPE_BITS | CONN_EL_GROUP_BITS  |
   ----------------------------------------------
*/

enum connection_type get_conn_type(unsigned long annotated_id);
enum connection_el_group get_conn_el_group(unsigned long annotated_id);
unsigned long get_conn_raw_id(unsigned long annotated_id);
unsigned long set_conn_info(unsigned long id, enum connection_type conn_type,
                            char el_type);

struct container_element;

struct node {
    unsigned long nuid;
    char *name;
    double value;
    unsigned long refs;
    unsigned long el_size;
    struct container_element *attached_el;
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

struct container_element {
    char type;
    unsigned long idx;
    struct element *_el;
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

#endif

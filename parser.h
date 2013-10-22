#ifndef __CAPER_PARSER_H__
#define __CAPER_PARSER_H__

#define DEFAULT_BJT_AREA 1
#define DEFAULT_DIODE_AREA 1

#define SEMANTIC_ERRORS -2

struct fileinfo {
    char *name;
    char *raw_begin;
    char *raw_end;
    size_t size;
};

struct raw_node {
    char *name;
    double value;
    unsigned long refs;
};

struct node {
    unsigned long nuid;
#if 0
    struct raw_node _node;
#else
    char *name;
    double value;
    unsigned long refs;
#endif
};

struct _passive_ {
    struct node *vplus;
    struct node *vminus;
};

struct _source_ {
    struct node *vplus;
    struct node *vminus;
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
    struct node *s;
    struct node *d;
    struct node *g;
    struct node *b;
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
    struct node *c;
    struct node *e;
    struct node *b;
    struct nonlinear_model model;
    double area;
};

struct _diode_ {
    struct node *vplus;
    struct node *vminus;
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

void parser_init();
void parser_clean();

int parse_file(const char *filename);

void print_element(struct element *_el);
void print_elements();
void print_node(struct node *_node);
void print_nodes();

struct node *get_node_pool();
struct element *get_element_pool();

#endif

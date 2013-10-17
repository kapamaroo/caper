#ifndef __CAPER_PARSER_H__
#define __CAPER_PARSER_H__

struct fileinfo {
    char *name;
    char *raw_begin;
    char *raw_end;
    size_t size;
};

struct node {
    unsigned long nuid;
    char *name;
};

struct _passive_ {
    struct node vplus;
    struct node vminus;
};

struct _source_ {
    struct node vplus;
    struct node vminus;
};

struct _mos_ {
    struct node s;
    struct node d;
    struct node g;
    struct node b;
};

struct _bjt_ {
    struct node c;
    struct node e;
    struct node b;
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
    };
};

void parser_init();

int parse_file(const char *filename);

#endif

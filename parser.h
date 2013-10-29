#ifndef __CAPER_PARSER_H__
#define __CAPER_PARSER_H__

#include "datatypes.h"

#define DEFAULT_BJT_AREA 1
#define DEFAULT_DIODE_AREA 1

#define SEMANTIC_ERRORS -2

struct fileinfo {
    char *name;
    char *raw_begin;
    char *raw_end;
    size_t size;
};

int parse_file(const char *filename);

void print_element(struct element *_el);
void print_elements();
void print_node(struct node *_node);
void print_nodes();

struct node *get_node_pool();
struct element *get_element_pool();

#endif

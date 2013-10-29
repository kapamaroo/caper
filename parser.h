#ifndef __CAPER_PARSER_H__
#define __CAPER_PARSER_H__

#include "datatypes.h"

#define DEFAULT_BJT_AREA 1
#define DEFAULT_DIODE_AREA 1

#define SEMANTIC_ERRORS -2

void parse_file(const char *filename, struct netlist_info *netlist);

void print_element(struct element *_el);
void print_elements(unsigned long size, struct element el_pool[size]);
void print_node(struct node *_node);
void print_nodes(unsigned long size, struct node node_pool[size]);

#endif

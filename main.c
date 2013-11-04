#include <stdio.h>
#include <stdlib.h>

#include "parser.h"
#include "analysis.h"

void help() {

}

void about() {

}

void handle_file(char *filename) {
    printf("\n================    File name: '%s    ================\n",filename);
    struct netlist_info netlist;
    parse_file(filename,&netlist);
    if (netlist.error) {
        printf("\nError: input file '%s' is not well defined - exit.\n",filename);
        return;
    }

    printf("\n\n***    Circuit Elements    ***\n\n");
    print_elements(netlist.el_group1_size,netlist.el_group1_pool);
    print_elements(netlist.el_group2_size,netlist.el_group2_pool);
    printf("\n\n***    Circuit Nodes    ***\n\n");
    print_nodes(netlist.node_size,netlist.node_pool);

    struct analysis_info analysis;
    analyse_kvl(&netlist,&analysis);
    print_int_array(analysis.n, analysis.e, analysis.A);

    printf("***    Transposed A\n");
    print_int_array(analysis.e,analysis.n,analysis.At);
    printf("***    A1\n");
    print_int_array(analysis.n,analysis.el_group1_size,analysis.A1);
    printf("***    A2\n");
    print_int_array(analysis.n,analysis.el_group2_size,analysis.A2);
    printf("***    G\n");
    print_double_array(analysis.el_group1_size,analysis.el_group1_size,analysis.G);
    printf("***    C\n");
    print_double_array(analysis.el_group1_size,analysis.el_group1_size,analysis.C);
    printf("***    L\n");
    print_double_array(analysis.el_group2_size,analysis.el_group2_size,analysis.L);
    printf("***    S1\n");
    print_double_array(analysis.el_group1_size,1,analysis.S1);
    printf("***    S2\n");
    print_double_array(analysis.el_group2_size,1,analysis.S2);
}

int main(int argc, char *argv[]) {
    int i;
    for (i=1; i<argc; ++i) {
        handle_file(argv[i]);
    }

    printf("\nTerminating...\n\n");
    exit(EXIT_SUCCESS);
}

#include <stdio.h>
#include <stdlib.h>

#include "parser.h"
#include "analysis.h"

void help() {

}

void about() {

}

void debug_info() {
    printf("sizeof(struct node) = %lu\n",sizeof(struct node));
    printf("sizeof(struct element) = %lu\n",sizeof(struct element));
    printf("sizeof(struct _source_) = %lu\n",sizeof(struct _source_));
    printf("sizeof(struct _passive_) = %lu\n",sizeof(struct _passive_));
    printf("sizeof(struct _mos_) = %lu\n",sizeof(struct _mos_));
    printf("sizeof(struct _bjt_) = %lu\n",sizeof(struct _bjt_));
    printf("sizeof(struct _diode_) = %lu\n",sizeof(struct _diode_));

    printf("sizeof(float) = %lu\n",sizeof(float));
    printf("sizeof(double) = %lu\n",sizeof(double));
    printf("sizeof(dfloat_t) = %lu\n",sizeof(dfloat_t));

    printf("sizeof(void*) = %lu\n",sizeof(void*));
}

void handle_file(char *filename) {
    printf("\n================    File name: '%s    ================\n",filename);
    struct netlist_info netlist;
    parse_file(filename,&netlist);
    if (netlist.error) {
        printf("\nError: input file '%s' is not well defined - exit.\n",filename);
        return;
    }

#if 0
    printf("\n\n***    Circuit Elements    ***\n\n");
    print_elements(netlist.el_group1_size,netlist.el_group1_pool);
    print_elements(netlist.el_group2_size,netlist.el_group2_pool);
    printf("\n\n***    Circuit Nodes    ***\n\n");
    print_nodes(netlist.node_size,netlist.node_pool);
#endif

    struct analysis_info analysis;
    analyse_mna(&netlist,&analysis);

#if 0
    printf("\n***    A\n");
    print_char_int_array(analysis.n, analysis.e, analysis.A);
    printf("\n***    A1\n");
    print_char_int_array(analysis.n,analysis.el_group1_size,analysis.A1);
    printf("\n***    A2\n");
    print_char_int_array(analysis.n,analysis.el_group2_size,analysis.A2);
    printf("\n***    G\n");
    print_dfloat_array(analysis.el_group1_size,1,analysis.G);
    printf("\n***    C\n");
    print_dfloat_array(analysis.el_group1_size,1,analysis.C);
    printf("\n***    L\n");
    print_dfloat_array(analysis.el_group2_size,1,analysis.L);
    printf("\n***    S1\n");
    print_dfloat_array(analysis.el_group1_size,1,analysis.S1);
    printf("\n***    S2\n");
    print_dfloat_array(analysis.el_group2_size,1,analysis.S2);

    unsigned long mna_dim_size = analysis.n + analysis.el_group2_size;
    printf("\n***    MNA Matrix\n");
    print_dfloat_array(mna_dim_size,mna_dim_size,analysis.mna_matrix);
    printf("\n***    MNA Vector\n");
    print_dfloat_array(mna_dim_size,1,analysis.mna_vector);

#endif
    printf("\n***    Solution Vector (x)\n");
    print_dfloat_array(analysis.n + analysis.el_group2_size,
                       1,analysis.x);

    printf("\nelements = %lu\n",analysis.e);
    printf("nodes = %lu  //without ground\n",analysis.n);
}

int main(int argc, char *argv[]) {
    int i;

    if (argc == 1)
        debug_info();

    for (i=1; i<argc; ++i) {
        handle_file(argv[i]);
    }

    printf("\nTerminating...\n\n");
    exit(EXIT_SUCCESS);
}

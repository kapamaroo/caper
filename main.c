#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "analysis.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"

int debug_on = 0;
int force_sparse = 0;

void about() {
    printf("\n" ANSI_COLOR_RED "NAME" ANSI_COLOR_RESET "\n");
    printf("\tcaper - A flaming Spice flavor for circuit emulation!\n");
}

void authors() {
    printf("\n" ANSI_COLOR_RED "AUTHORS" ANSI_COLOR_RESET "\n");
    printf("\tEmmanouil Maroudas <emmmarou@uth.gr>\n");
    printf("\tKalandaridis Theodosios <thkaland@uth.gr>\n");
    printf("\tMylonas Aggelos <agmylona@gmail.com>\n");
    printf("\tKousias Konstantinos <kokousia@inf.uth.gr>\n");
}

void more() {
    printf("\n" ANSI_COLOR_RED "MORE" ANSI_COLOR_RESET "\n");
    printf("\thttps://github.com/kapamaroo/caper\n");
}

void options() {
    printf("\n" ANSI_COLOR_RED "OPTIONS" ANSI_COLOR_RESET "\n");
    printf("\t-d\tenable debug messages\n");
    printf("\t-s\tforce sparse matrix storage format\n");
}

void help(int argc, char *argv[]) {
    about();
    options();

    printf("\n" ANSI_COLOR_RED "USAGE" ANSI_COLOR_RESET "\n");
    printf("\t%s [-d] [-s] NETLIST_FILE\n",argv[0]);

    authors();
    more();
    printf("\n");
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

    printf("\nelements = %lu\n",analysis.e);
    printf("nodes = %lu  //without ground\n",analysis.n);

    printf("\n***    Solution Vector (x)\n");
    print_dfloat_array(analysis.n + analysis.el_group2_size,
                       1,analysis.x);
}

void parse_args(int argc, char *argv[]) {
    int i;
    for (i=1; i<argc; ++i) {
        if (!strcmp(argv[i],"-d")) {
            debug_on = 1;
            debug_info();
        }
        else if (!strcmp(argv[i],"-s")) {
            force_sparse = 1;
        }
    }
}

int main(int argc, char *argv[]) {
    int i;

    if (argc == 1) {
        help(argc,argv);
        exit(EXIT_FAILURE);
    }

    parse_args(argc,argv);

    for (i=1; i<argc; ++i) {
        if (argv[i][0] != '-')
            handle_file(argv[i]);
    }

    printf("\nTerminating...\n\n");
    exit(EXIT_SUCCESS);
}

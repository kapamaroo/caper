#include <stdio.h>
#include <stdlib.h>

#include "parser.h"

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

    print_elements(netlist.el_size,netlist.el_pool);
    print_nodes(netlist.node_size,netlist.node_pool);
}

int main(int argc, char *argv[]) {
    int i;
    for (i=1; i<argc; ++i) {
        handle_file(argv[i]);
    }

    printf("\nTerminating...\n\n");
    exit(EXIT_SUCCESS);
}

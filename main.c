#include <stdio.h>
#include <stdlib.h>

#include "parser.h"

void help() {

}

void about() {

}

void handle_file(char *filename) {
    printf("\n================    File name: '%s    ================\n",filename);
    int status = parse_file(filename);
    if (status == SEMANTIC_ERRORS) {
        printf("\nError: input file '%s' is not well defined - exit.\n",filename);
        //print_elements();
        //print_nodes();
        return;
    }

    print_elements();
    print_nodes();
}

int main(int argc, char *argv[]) {
    int i;
    for (i=1; i<argc; ++i) {
        handle_file(argv[i]);
    }

    printf("\nTerminating...\n\n");
    exit(EXIT_SUCCESS);
}

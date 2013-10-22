#include <stdio.h>
#include <stdlib.h>

#include "parser.h"

void help() {

}

void about() {

}

void handle_file(char *filename) {
    int status = parse_file(filename);
    print_elements();
    print_nodes();
    if (status == SEMANTIC_ERRORS) {
        printf("\nError: input file '%s' is not well defined - exit.\n\n",filename);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    int i;

    parser_init();
    for (i=1; i<argc; ++i) {
        handle_file(argv[i]);
    }

    printf("\nTerminating...\n\n");
    exit(EXIT_SUCCESS);
}

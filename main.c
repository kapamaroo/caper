#include <stdio.h>
#include <stdlib.h>

#include "parser.h"

void help() {

}

void about() {

}

int main(int argc, char *argv[]) {
    int i;

    parser_init();
    for (i=1; i<argc; ++i) {
        parse_file(argv[i]);
    }

    printf("\nTerminating... OK.\n\n");
    exit(EXIT_SUCCESS);
}

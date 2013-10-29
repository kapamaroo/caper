#include "analysis.h"

#include <stdlib.h>
#include <assert.h>
#include <errno.h>

void analyse(struct netlist_info *netlist, struct analysis_info *analysis,
             enum analysis_type type) {
    assert(netlist);
    assert(analysis);

    unsigned int size = (netlist->n - 1) * netlist->e;
    int *A = (int*)malloc(size * sizeof(int));
    if (!A) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    //ignore the transposed matrix for now
#if 0

#else
    int *At = (int*)malloc(size * sizeof(int));
    if (!At) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
#endif

    //unsigned int i;

    //TODO: populate A
}

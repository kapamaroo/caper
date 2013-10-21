#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include "parser.h"

char *parse_line(char *buf);
char *parse_node(char *buf, char **node);
char *parse_element(char *buf);
char *parse_comment(char *buf);
char *parse_command(char *buf);

struct fileinfo *current_input;

unsigned long el_pool_size = 128;
unsigned long el_pool_next = 0;
struct element *el_pool;

unsigned long node_pool_size = 128;
unsigned long node_pool_next = 0;
struct node **node_pool;

static unsigned long __nuid__ = 1;  //zero goes to ground node
static inline unsigned long set_nuid(struct node *n) {
    int i;
    for (i = 0; i<node_pool_next; ++i) {
        if (strcmp(node_pool[i]->name,n->name) == 0) {
            n->nuid = node_pool[i]->nuid;
            return n->nuid;
        }
    }

    unsigned long nuid = __nuid__++;
    n->nuid = nuid;
    return nuid;
}

static unsigned long __euid__ = 0;
static inline unsigned long set_euid() {
    return __euid__++;
}

void parser_init() {
    current_input = NULL;
    el_pool = (struct element *)malloc(el_pool_size * sizeof(struct element));
    if (!el_pool) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    node_pool = (struct node **)malloc(node_pool_size * sizeof(struct node *));
    if (!node_pool) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
}

struct fileinfo *open_file(const char *filename) {
    struct fileinfo *finfo;
    finfo = (struct fileinfo *)malloc(sizeof(struct fileinfo));
    if (!finfo) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    struct stat fstats;
    int error = stat(filename,&fstats);
    if (error) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    int fd = open(filename,O_RDONLY);
    if (fd == -1) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    char *raw_data =
        mmap(NULL,fstats.st_size,PROT_READ,MAP_POPULATE | MAP_PRIVATE,fd,0);
    if (raw_data == MAP_FAILED) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    error = close(fd);
    if (error) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    finfo->name = (char *)malloc((strlen(filename) + 1) * sizeof(char));
    finfo->name = strcpy(finfo->name,filename);
    finfo->size = fstats.st_size;
    finfo->raw_begin = raw_data;
    finfo->raw_end = &raw_data[fstats.st_size];

    return finfo;
}

int close_file(struct fileinfo *finfo) {
    int error = munmap(finfo->raw_begin,finfo->size);
    if (error) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    free(finfo);
    finfo = NULL;
    return error;
}

int parse_file(const char *filename) {
    current_input = open_file(filename);

    char *ptr;
    for (ptr = current_input->raw_begin; ptr; )
        ptr = parse_line(ptr);

    int error = close_file(current_input);
    return error;
}

inline static int isdelimiter(char c) {
    return isspace(c) && c != '\n';
}

char *parse_eat_whitechars(char *buf) {
    /* return NULL on end of buffer */

    char *end = current_input->raw_end;
    if (buf == end)
        return NULL;
    while (buf != end) {
        if (isdelimiter(*buf)) {
            buf++;
            //printf("debug: found whitespace\n");
        }
        else
            break;
    }
    if (buf == end)
        return NULL;
    return buf;
}

char *parse_eat_newline(char *buf) {
    /* return NULL on end of buffer */
    /* skip all LF */

    char *end = current_input->raw_end;
    while (buf != end) {
        if (*buf != '\n') {
            break;
        }
        buf++;
    }
    if (buf == end)
        return NULL;
    return buf;
}

char *parse_line(char *buf) {
    /* return NULL on end of buffer */
    buf = parse_eat_whitechars(buf);
    if (!buf)
        return NULL;
    buf = parse_eat_newline(buf);
    if (!buf)
        return NULL;
    switch (*buf) {
    case '*':  buf = parse_comment(buf);  break;
    case '.':  buf = parse_command(buf);  break;
    default:   buf = parse_element(buf);  break;
    }
    return buf;
}

char *parse_string(char *buf, char **name) {
    //printf("in function: %s\n",__FUNCTION__);

    char *start = buf;
    int size = 0;
    char *end = current_input->raw_end;
    while (buf != end) {
        if (isdelimiter(*buf)) {
            break;
        }
        buf++;
        size++;
    }

    *name = (char*)malloc((size+1) * sizeof(char));
    if (!*name) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    *name = memcpy(*name,start,size);
    (*name)[size] = '\0';

    buf = parse_eat_whitechars(buf);

    //printf("debug: new string \"%s\"\tsize=%d\n",*name,size);

    //printf("out of function: %s\n",__FUNCTION__);
    return buf;
}

char *parse_value(char *buf, double *value) {
    //printf("in function: %s\n",__FUNCTION__);

    char *endptr = buf;
    errno = 0;
    *value = strtod(buf,&endptr);
    if (errno) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    if (buf == endptr) {
        printf("Error: bad value\n");
        exit(EXIT_FAILURE);
    }

    //printf("debug: value=%f\n",*value);
    buf = parse_eat_whitechars(endptr);
    return buf;
}

void grow(void **pool,unsigned long size, unsigned long element_size,
          unsigned long next) {
#if 1
    printf("error: grow does not work - exit.\n");
    exit(EXIT_FAILURE);
#endif

    printf("in function: %s\n",__FUNCTION__);

    if (next == size) {
        if (size == ULONG_MAX) {
            printf("Out of memory: reached pool limit - exit.\n");
            exit(EXIT_FAILURE);
        }
        else if (size > ULONG_MAX >> 1)
            size = ULONG_MAX;
        else
            size << 1;
        void *new_pool = (void *)realloc(*pool,size * element_size);
        if (!new_pool) {
            printf("Out of memory: realloc failed - exit.\n");
            exit(EXIT_FAILURE);
        }
        *pool = new_pool;
    }
}

static inline int isinvalidnodename(char *s) {
    if (s[0] != '-')
        return 0;
    if (!isdigit(s[1]))
        return 0;

    printf("Error: bad node name \"%s\"\n",s);
    return 1;
}

char *parse_element(char *buf) {
    //printf("in function: %s\n",__FUNCTION__);

    //printf("\n***  new element  ***\n\n");

    /* return NULL on end of buffer */

    /* eat the node specifier */
    struct element *s_el = &el_pool[el_pool_next++];

    char type = *buf;
    s_el->type = type;

    s_el->euid = set_euid();

    /* check if we need to resize the pool */
    //grow((void**)&el_pool,el_pool_size,sizeof(struct element),el_pool_next);

    //printf("element type: '%c'\n",type);
    switch (type) {
    case 'v':
    case 'i': {
        node_pool[node_pool_next++] = &s_el->_vi.vminus;
        //grow((void**)&node_pool,node_pool_size,sizeof(struct node*),node_pool_next);
        node_pool[node_pool_next++] = &s_el->_vi.vplus;
        //grow((void **)&node_pool,node_pool_size,sizeof(struct node*),node_pool_next);

        buf = parse_string(buf,&s_el->name);
        if (!buf || *buf == '\n') {
            printf("error: expected string - exit.\n");
            exit(EXIT_FAILURE);
        }

        buf = parse_string(buf,&s_el->_vi.vplus.name);
        if (!buf || *buf == '\n') {
            printf("error: expected string - exit.\n");
            exit(EXIT_FAILURE);
        }
        if (isinvalidnodename(s_el->_vi.vplus.name)) {
            exit(EXIT_FAILURE);
        }

        buf = parse_string(buf,&s_el->_vi.vminus.name);
        if (!buf || *buf == '\n') {
            printf("error: expected string - exit.\n");
            exit(EXIT_FAILURE);
        }
        if (isinvalidnodename(s_el->_vi.vminus.name)) {
            exit(EXIT_FAILURE);
        }

        buf = parse_value(buf,&s_el->value);
        if (!buf)
            return NULL;
        if (*buf != '\n') {
            printf("error: invalid character '%c' after value - exit.\n",*buf);
            exit(EXIT_FAILURE);
        }

        set_nuid(&s_el->_vi.vminus);
        set_nuid(&s_el->_vi.vplus);

        printf("%s %s %s %+8.4f\n",s_el->name, s_el->_vi.vplus.name,s_el->_vi.vminus.name,s_el->value);

        break;
    }
    case 'r':
    case 'c':
    case 'l': {
        node_pool[node_pool_next++] = &s_el->_rcl.vminus;
        //grow((void**)&node_pool,node_pool_size,sizeof(struct node),node_pool_next);
        node_pool[node_pool_next++] = &s_el->_rcl.vplus;
        //grow((void**)&node_pool,node_pool_size,sizeof(struct node),node_pool_next);

        buf = parse_string(buf,&s_el->name);
        if (!buf || *buf == '\n') {
            printf("error: expected string - exit.\n");
            exit(EXIT_FAILURE);
        }

        buf = parse_string(buf,&s_el->_rcl.vplus.name);
        if (!buf || *buf == '\n') {
            printf("error: expected string - exit.\n");
            exit(EXIT_FAILURE);
        }

        buf = parse_string(buf,&s_el->_rcl.vminus.name);
        if (!buf || *buf == '\n') {
            printf("error: expected string - exit.\n");
            exit(EXIT_FAILURE);
        }

        buf = parse_value(buf,&s_el->value);
        if (!buf)
            return NULL;
        if (*buf != '\n') {
            printf("error: invalid character '%c' after value - exit.\n",*buf);
            exit(EXIT_FAILURE);
        }

        set_nuid(&s_el->_rcl.vminus);
        set_nuid(&s_el->_rcl.vplus);

        printf("%s %s %s %+8.4f\n",s_el->name, s_el->_vi.vplus.name,s_el->_vi.vminus.name,s_el->value);

        break;
    }
    default:
        printf("Unknown element type '%c' - exit.\n",type);
        exit(EXIT_FAILURE);
    }

    return buf;
}

char *parse_comment(char *buf) {
    //printf("in function: %s\n",__FUNCTION__);

    /* return NULL on end of buffer */

    char *end = current_input->raw_end;
    while (buf != end) {
        if (*buf++ == '\n')
            break;
    }
    if (buf == end)
        return NULL;

    return buf;
}

char *parse_command(char *buf) {
    //printf("in function: %s\n",__FUNCTION__);

    /* return NULL on end of buffer */

    /* eat '.' */
    buf++;

#if 0
    char *name;
    buf = parse_string(buf,&name);

    if (strcmp(name,""))

#else
    printf("***  WARNING  ***    command parsing is not implemented yet! - skip command\n");
    char *end = current_input->raw_end;
    while (buf != end) {
        if (*buf == '\n')
            break;
        buf++;
    }
#endif

    buf = parse_eat_whitechars(buf);

    return buf;
}

struct node **get_node_pool() {
    assert(node_pool);
    return node_pool;
}

struct element *get_element_pool() {
    assert(el_pool);
    return el_pool;
}

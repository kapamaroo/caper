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
#include <ctype.h>

#include "parser.h"

void parse_line(char **buf);
void parse_element(char **buf);
void parse_comment(char **buf);
void parse_command(char **buf);

char *parse_string(char **buf, char *info);

struct fileinfo *current_input;

unsigned long el_pool_size = 128;
unsigned long el_pool_next = 0;
struct element *el_pool;

unsigned long node_pool_size = 128;
unsigned long node_pool_next = 1;  //reserve the first node to be the ground node
struct node *node_pool;

static inline int is_invalid_node_name(char *s) {
    if (s[0] != '-')
        return 0;
    if (!isdigit(s[1]))
        return 0;

    printf("Error: bad node name \"%s\"\n",s);
    return 1;
}

static unsigned long __nuid__ = 1;  //zero goes to ground node
static inline struct node *parse_node(char **buf, char *info) {
    char *name = parse_string(buf,info);
    if (is_invalid_node_name(name))
        exit(EXIT_FAILURE);

    int i;
    for (i=0; i<node_pool_next; ++i) {
        struct node *_n = &node_pool[i];
        if (strcmp(_n->name,name) == 0) {
            free(name);
            _n->refs++;
            return _n;
        }
    }

    struct node *_n = &node_pool[node_pool_next++];
    _n->name = name;
    _n->nuid = __nuid__++;
    _n->value = 0;
    _n->refs = 1;

    /* check if we need to resize the pool */
    //grow((void**)&node_pool,node_pool_size,sizeof(struct node*),node_pool_next);

    return _n;
}

static unsigned long __euid__ = 0;
static inline struct element *get_new_element(char type) {
    struct element *el = &el_pool[el_pool_next++];
    el->type = type;
    el->euid = __euid__++;
    //we get the name later

    /* check if we need to resize the pool */
    //grow((void**)&el_pool,el_pool_size,sizeof(struct element),el_pool_next);

    return el;
}

void print_element(struct element *_el) {
    assert(_el);
    switch (_el->type) {
    case 'v':
    case 'i':
        printf("%s %s %s %+8.4f\n",
               _el->name,
               _el->_vi.vplus->name,
               _el->_vi.vminus->name,
               _el->value);
        break;
    case 'r':
    case 'l':
    case 'c':
        printf("%s %s %s %+8.4f\n",
               _el->name,
               _el->_rcl.vplus->name,
               _el->_rcl.vminus->name,
               _el->value);
        break;
    case 'q':
        printf("%s %s %s %s %s %f\n",
               _el->name,
               _el->bjt.c->name,
               _el->bjt.b->name,
               _el->bjt.e->name,
               _el->bjt.model.name,
               _el->bjt.area);
        break;
    case 'm':
        printf("%s %s %s %s %s %s %f %f",
               _el->name,
               _el->mos.d->name,
               _el->mos.g->name,
               _el->mos.s->name,
               _el->mos.b->name,
               _el->mos.model.name,
               _el->mos.length,
               _el->mos.width);
        break;
    case 'd':
        printf("%s %s %s %s %+8.4f\n",
               _el->name,
               _el->diode.vplus->name,
               _el->diode.vminus->name,
               _el->diode.model.name,
               _el->value);
        break;
    default:
        printf("Unknown element type '%c' - exit.\n",_el->type);
        exit(EXIT_FAILURE);
    }
}

void print_elements() {
    printf("\n\n***    Circuit Elements    ***\n\n");

    int i;
    for (i=0; i<el_pool_next; ++i)
        print_element(&el_pool[i]);
}

void print_node(struct node *n) {
    assert(n);
    printf("nuid (%lu): node %s connects %lu elements\n",n->nuid,n->name,n->refs);
}

void print_nodes() {
    printf("\n\n***    Circuit Nodes    ***\n\n");

    int i;
    for (i=0; i<node_pool_next; ++i)
        print_node(&node_pool[i]);
}

void parser_init() {
    current_input = NULL;
    el_pool = (struct element *)malloc(el_pool_size * sizeof(struct element));
    if (!el_pool) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    node_pool = (struct node *)malloc(node_pool_size * sizeof(struct node));
    if (!node_pool) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    //init ground node
    struct node *ground = &node_pool[0];
    ground->name = "0";
    ground->nuid = 0;
    ground->value = 0;
    ground->refs = 0;
}

int is_semantically_correct() {
    struct node *ground = &node_pool[0];
    if (ground->refs == 0) {
        printf("error: missing ground node\n");
        return 0;
    }
    return 1;
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

void close_file(struct fileinfo *finfo) {
    int error = munmap(finfo->raw_begin,finfo->size);
    if (error) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    free(finfo);
    finfo = NULL;
}

int parse_file(const char *filename) {
    current_input = open_file(filename);

    char *ptr;
    for (ptr = current_input->raw_begin; ptr; )
        parse_line(&ptr);

    close_file(current_input);

    if (is_semantically_correct())
        return 0;
    return SEMANTIC_ERRORS;
}

void parse_eat_whitechars(char **buf) {
    if (!*buf)
        return;
    char *end = current_input->raw_end;
    if (*buf == end) {
        *buf = NULL;
        return;
    }
    while (*buf != end) {
        if (!isspace(**buf) || **buf == '\n')
            break;
        (*buf)++;
    }
    if (*buf == end)
        *buf = NULL;
}

void parse_eat_newline(char **buf) {
    /* skip all LF */

    char *end = current_input->raw_end;
    while (*buf != end) {
        if (**buf != '\n') {
            break;
        }
        (*buf)++;
    }
    if (*buf == end)
        *buf = NULL;
}

void parse_line(char **buf) {
    parse_eat_whitechars(buf);
    if (!*buf)
        return;
    parse_eat_newline(buf);
    if (!*buf)
        return;
    switch (**buf) {
    case '*':  parse_comment(buf);  break;
    case '.':  parse_command(buf);  break;
    default:   parse_element(buf);  break;
    }
}

inline static int isdelimiter(char c) {
    return isspace(c); // && c != '\n';
}

char *parse_string(char **buf, char *info) {
    //printf("in function: %s\n",__FUNCTION__);

    if (!*buf || isspace(**buf)) {
        printf("error: expected %s - exit.\n",info);
        exit(EXIT_FAILURE);
    }

    char *start = *buf;
    int size = 0;
    char *end = current_input->raw_end;
    while (*buf != end) {
        if (isdelimiter(**buf)) {
            break;
        }
        (*buf)++;
        size++;
    }

    char *name = (char*)malloc((size+1) * sizeof(char));
    if (!name) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    name = memcpy(name,start,size);
    name[size] = '\0';

    //convert to lowercase
    int i;
    for (i=0; i<size; ++i)
        name[i] = tolower(name[i]);

    parse_eat_whitechars(buf);

    //printf("debug: new string \"%s\"\tsize=%d\n",*name,size);

    //printf("out of function: %s\n",__FUNCTION__);
    return name;
}

double parse_value(char **buf, char *info) {
    //printf("in function: %s\n",__FUNCTION__);

    if (!*buf || isspace(**buf)) {
        printf("error: expected %s - exit\n",info);
        exit(EXIT_FAILURE);
    }

    char *end = current_input->raw_end;
    errno = 0;
    char *endptr = *buf;
    double value = strtod(*buf,&endptr);
    if (errno) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    if (*buf == endptr) {
        printf("Error: bad value\n");
        exit(EXIT_FAILURE);
    }
    *buf = endptr;
    if (*buf == end)
        *buf = NULL;
    if (*buf && !isspace(**buf)) {
        printf("error: invalid character '%c' after value - exit.\n",**buf);
        exit(EXIT_FAILURE);
    }

    parse_eat_whitechars(buf);

    //printf("debug: value=%f\n",*value);

    return value;
}

double parse_value_optional(char **buf, double default_value) {
    //printf("in function: %s\n",__FUNCTION__);

    if (!*buf || isspace(**buf))
        return default_value;
    else
        return parse_value(buf,"value");
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
            size <<= 1;
        void *new_pool = (void *)realloc(*pool,size * element_size);
        if (!new_pool) {
            printf("Out of memory: realloc failed - exit.\n");
            exit(EXIT_FAILURE);
        }
        *pool = new_pool;
    }
}

void parse_element(char **buf) {
    //printf("in function: %s\n",__FUNCTION__);

    char type = tolower(**buf);
    struct element *s_el = get_new_element(type);

    switch (type) {
    case 'v':
    case 'i': {
        s_el->name = parse_string(buf,"voltage/current source name");
        s_el->_vi.vplus = parse_node(buf,"voltage/current '+' node");
        s_el->_vi.vminus = parse_node(buf,"voltage/current '-' node");
        s_el->value = parse_value(buf,"voltage/current value");
        break;
    }
    case 'r':
    case 'c':
    case 'l': {
        s_el->name = parse_string(buf,"rlc element name");
        s_el->_rcl.vplus = parse_node(buf,"rcl '+' node");
        s_el->_rcl.vminus = parse_node(buf,"rcl '-' node");
        s_el->value = parse_value(buf,"rcl value");
        break;
    }
    case 'q': {
        s_el->name = parse_string(buf,"bjt name");
        s_el->bjt.c = parse_node(buf,"bjt c node");
        s_el->bjt.b = parse_node(buf,"bjt b node");
        s_el->bjt.e = parse_node(buf,"bjt e node");
        s_el->bjt.model.name = parse_string(buf,"bjt model name");
        s_el->bjt.area = parse_value_optional(buf,DEFAULT_BJT_AREA);
        break;
    }
    case 'm': {
        s_el->name = parse_string(buf,"mos name");
        s_el->mos.d = parse_node(buf,"mos d node");
        s_el->mos.g = parse_node(buf,"mos g node");
        s_el->mos.s = parse_node(buf,"mos s node");
        s_el->mos.b = parse_node(buf,"mos b node");
        s_el->mos.model.name = parse_string(buf,"mos model name");
        s_el->mos.l = parse_value(buf,"mos length value");
        s_el->mos.w = parse_value(buf,"mos width value");
        break;
    }
    case 'd': {
        s_el->name = parse_string(buf,"diode name");
        s_el->diode.vplus = parse_node(buf,"diode '+' node");
        s_el->diode.vminus = parse_node(buf,"diode '-' node");
        s_el->diode.model.name = parse_string(buf,"diode model name");
        s_el->diode.area = parse_value_optional(buf,DEFAULT_DIODE_AREA);
        break;
    }
    default:
        printf("Unknown element type '%c' - exit.\n",type);
        exit(EXIT_FAILURE);
    }
}

void parse_comment(char **buf) {
    //printf("in function: %s\n",__FUNCTION__);

    char *end = current_input->raw_end;
    while (*buf != end) {
        if (*(*buf)++ == '\n')
            break;
    }
    if (*buf == end)
        *buf = NULL;
}

static inline int command_is(char *command, char *type) {
    return (strcmp(command,type) == 0);
}

void parse_command(char **buf) {
    //printf("in function: %s\n",__FUNCTION__);

    /* eat '.' */
    (*buf)++;

#if 0
    char *name;
    parse_string(buf,&name);

    if (command_is(name,"")) {

    }

#else
    printf("***  WARNING  ***    command parsing is not implemented yet! - skip command\n");
    char *end = current_input->raw_end;
    while (*buf != end) {
        if (**buf == '\n')
            break;
        (*buf)++;
    }
#endif

    parse_eat_whitechars(buf);
}

struct node *get_node_pool() {
    assert(node_pool);
    return node_pool;
}

struct element *get_element_pool() {
    assert(el_pool);
    return el_pool;
}

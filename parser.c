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
#include "hash.h"
#include "transient_support.h"

#define SHOULD_REBUILD 1
#define NO_REBUILD 0

void parse_line(char **buf);
void parse_element(char **buf);
void parse_comment(char **buf);
void parse_command(char **buf);
struct _transient_ *parse_transient(char **buf);

char *parse_string(char **buf, char *info);

struct file_info {
    char *name;
    char *raw_begin;
    char *raw_end;
    size_t size;
};

static struct file_info *current_input = NULL;

static unsigned long line_num = 1;

#define INIT_EL_POOL_SIZE 1
#define INIT_NODE_POOL_SIZE 1
#define INIT_CMD_POOL_SIZE 16

#if INIT_NODE_POOL_SIZE < 1
#error node pool size must be at least 1
#endif

static unsigned long __euid__ = 0;
static unsigned long __nuid__ = 1;  //zero goes to ground node

static unsigned long el_group1_pool_size = INIT_EL_POOL_SIZE;
static unsigned long el_group1_pool_next = 0;
struct element *el_group1_pool = NULL;

static unsigned long el_group2_pool_size = INIT_EL_POOL_SIZE;
static unsigned long el_group2_pool_next = 0;
struct element *el_group2_pool = NULL;

static unsigned long node_pool_size = INIT_NODE_POOL_SIZE;
//reserve the first node to be the ground node
static unsigned long node_pool_next = 1;
struct node *node_pool = NULL;

static unsigned int node_hash_size = 1013;
struct hash_table *node_hash_table = NULL;

static unsigned int cmd_pool_size = INIT_CMD_POOL_SIZE;
static unsigned int cmd_pool_next = 0;
static struct command *cmd_pool = NULL;

static int counter_cmd_plot = 0;
static int counter_cmd_print = 0;

static inline void rebuild() {
    unsigned long i;
    for (i=0; i<el_group1_pool_next; ++i) {
        struct element *_el = &el_group1_pool[i];
        switch (_el->type) {
        case 'i':
            _el->_vi->vplus._node = &node_pool[_el->_vi->vplus.nuid];
            _el->_vi->vminus._node = &node_pool[_el->_vi->vminus.nuid];
            break;
        case 'r':
        case 'c':
            _el->_rcl->vplus._node = &node_pool[_el->_rcl->vplus.nuid];
            _el->_rcl->vminus._node = &node_pool[_el->_rcl->vminus.nuid];
            break;
        case 'q':
            _el->bjt->c._node = &node_pool[_el->bjt->c.nuid];
            _el->bjt->e._node = &node_pool[_el->bjt->e.nuid];
            _el->bjt->b._node = &node_pool[_el->bjt->b.nuid];
            break;
        case 'm':
            _el->mos->s._node = &node_pool[_el->mos->s.nuid];
            _el->mos->d._node = &node_pool[_el->mos->d.nuid];
            _el->mos->g._node = &node_pool[_el->mos->g.nuid];
            _el->mos->b._node = &node_pool[_el->mos->b.nuid];
            break;
        case 'd':
            _el->diode->vplus._node = &node_pool[_el->diode->vplus.nuid];
            _el->diode->vminus._node = &node_pool[_el->diode->vminus.nuid];
            break;
        default:
            printf("Unknown element type '%c' in group1 pool - exit.\n",_el->type);
            exit(EXIT_FAILURE);
        }
    }

    for (i=0; i<el_group2_pool_next; ++i) {
        struct element *_el = &el_group2_pool[i];
        switch (_el->type) {
        case 'v':
            _el->_vi->vplus._node = &node_pool[_el->_vi->vplus.nuid];
            _el->_vi->vminus._node = &node_pool[_el->_vi->vminus.nuid];
            break;
        case 'l':
            _el->_rcl->vplus._node = &node_pool[_el->_rcl->vplus.nuid];
            _el->_rcl->vminus._node = &node_pool[_el->_rcl->vminus.nuid];
            break;
        default:
            printf("Unknown element type '%c' in group2 pool - exit.\n",_el->type);
            exit(EXIT_FAILURE);
        }
    }

    //ignore ground node
    for (i=1; i<node_pool_next; ++i) {
        struct node *_node = &node_pool[i];
        unsigned long j;
        for (j=0; j<_node->refs; ++j) {
            struct element *pool = el_group1_pool;
            if (_node->attached_el[j].type == 'v' || _node->attached_el[j].type == 'l')
                pool = el_group2_pool;
            _node->attached_el[j]._el = &pool[_node->attached_el[j].idx];
        }
    }

    for (i=0; i<node_hash_table->size; ++i) {
        struct hash_element *root = node_hash_table->pool[i];
        while (root) {
            struct container_node *_cnode = root->data;
            struct node **_node_ptr = &_cnode->_node;
            *_node_ptr = &node_pool[_cnode->nuid];
            root = root->next;
        }
    }
}

static inline
unsigned long grow(void **pool,unsigned long size, unsigned long element_size,
          unsigned long next, const unsigned int should_rebuild) {
    //returns the new size

    assert(element_size);

    if (size == ULONG_MAX) {
        printf("Out of memory: reached pool limit - exit.\n");
        exit(EXIT_FAILURE);
    }

    assert(next <= size);
    if (next < size)
        return size;

    //if we use more than half bits of unsigned long, increase size
    //with a constant, else duplicate it
    unsigned long bits = sizeof(unsigned long) * CHAR_BIT;
    unsigned long inc = 1 << 8;
    unsigned long new_size;

    if (size == 0)
        new_size = 4;
    else if (ULONG_MAX - size < inc)
        new_size = ULONG_MAX;
    else if (size >> (bits/2))
        new_size = size + inc;
    else
        new_size = size << 1;

    assert(new_size);

    //NOTE: *pool may be NULL
    void *new_pool = (void *)realloc(*pool,new_size * element_size);
    if (!new_pool) {
        printf("Out of memory: realloc failed - exit.\n");
        exit(EXIT_FAILURE);
    }

    void *old_pool = *pool;
    *pool = new_pool;
    if (old_pool != new_pool && should_rebuild)
        rebuild();

    return new_size;
}

static inline int is_invalid_node_name(char *s) {
    if (s[0] != '-')
        return 0;
    if (!isdigit(s[1]))
        return 0;

    printf("Error: bad node name \"%s\"\n",s);
    return 1;
}

static inline struct container_node parse_node(char **buf, struct element *el,
                                               enum connection_type conn_type) {
    assert(el);

    char *name = parse_string(buf,"node identifier");
    if (is_invalid_node_name(name))
        exit(EXIT_FAILURE);

    //do not track ground node
    struct node *ground = &node_pool[0];
    if (strcmp(ground->name,name) == 0) {
        free(name);
        ground->refs++;
        struct container_node container = { .nuid=ground->nuid, ._node=ground };
        return container;
    }

    struct node *_node = NULL;
    struct container_node *_cnode = NULL;

    _cnode = hash_get(node_hash_table,name);
    if (_cnode) {
        _node = _cnode->_node;
        assert(_node->nuid != 0);
        free(name);
        _node->el_size = grow((void**)&_node->attached_el,_node->el_size,
                              sizeof(struct container_element),_node->refs,
                              NO_REBUILD);
        _node->attached_el[_node->refs].type = el->type;
        _node->attached_el[_node->refs].idx = el->idx;
        _node->attached_el[_node->refs]._el = el;
        _node->refs++;

        struct container_node container = { .nuid=_node->nuid, ._node=_node };
        return container;
    }

    /* check if we need to resize the pool */
    node_pool_size = grow((void**)&node_pool,node_pool_size,
                          sizeof(struct node),node_pool_next,SHOULD_REBUILD);

    unsigned long _nuid = __nuid__++;
    _node = &node_pool[node_pool_next++];

    _cnode = (struct container_node*)malloc(sizeof(struct container_node));
    if (!_cnode) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    _cnode->nuid = _nuid;
    _cnode->_node = _node;

    hash_insert(node_hash_table,name,_cnode);

    _node->nuid = _nuid;
    _node->name = name;
    _node->value = 0;
    _node->refs = 0;
    _node->el_size = 0;
    _node->attached_el = NULL;

    _node->el_size = grow((void**)&_node->attached_el,_node->el_size,
                          sizeof(struct container_element),_node->refs,
                          NO_REBUILD);
    _node->attached_el[_node->refs].type = el->type;
    _node->attached_el[_node->refs].idx = el->idx;
    _node->attached_el[_node->refs]._el = el;
    _node->refs++;

    struct container_node container = { .nuid=_nuid, ._node=_node };
    return container;
}

static inline struct element *get_new_element(char type) {
    /* check if we need to resize the pool */

    struct element *el = NULL;
    unsigned long idx;

    switch (type) {
    case 'v':
    case 'l':
        el_group2_pool_size = grow((void**)&el_group2_pool,el_group2_pool_size,
                                   sizeof(struct element),el_group2_pool_next,
                                   SHOULD_REBUILD);
        el = &el_group2_pool[el_group2_pool_next];
        idx = el_group2_pool_next++;
        break;
    default:
        el_group1_pool_size = grow((void**)&el_group1_pool,el_group1_pool_size,
                                   sizeof(struct element),el_group1_pool_next,
                                   SHOULD_REBUILD);
        el = &el_group1_pool[el_group1_pool_next];
        idx = el_group1_pool_next++;
    }

    assert(el);
    el->type = type;
    el->idx = idx;
    el->euid = __euid__++;
    el->name = NULL;  //we get the name later

    unsigned long size = 0;
    switch (type) {
    case 'v':
    case 'i':  size = sizeof(struct _source_);   break;
    case 'r':
    case 'c':
    case 'l':  size = sizeof(struct _passive_);  break;
    case 'm':  size = sizeof(struct _mos_);      break;
    case 'q':  size = sizeof(struct _bjt_);      break;
    case 'd':  size = sizeof(struct _diode_);    break;
    default:
        printf("debug: type=0x%2x (hex ascii)\n",type);
        assert(0);
    }

    assert(size);
    el->raw_ptr = malloc(size);
    if (!el->raw_ptr) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    return el;
}

void print_element(struct element *_el) {
    assert(_el);
    printf("euid (%3lu) : ",_el->euid);
    switch (_el->type) {
    case 'v':
    case 'i':
        printf("%s %s %s %+e\n",
               _el->name,
               _el->_vi->vplus._node->name,
               _el->_vi->vminus._node->name,
               _el->value);
        break;
    case 'r':
    case 'l':
    case 'c':
        printf("%s %s %s %e\n",
               _el->name,
               _el->_rcl->vplus._node->name,
               _el->_rcl->vminus._node->name,
               _el->value);
        break;
    case 'q':
        printf("%s %s %s %s %s %e\n",
               _el->name,
               _el->bjt->c._node->name,
               _el->bjt->b._node->name,
               _el->bjt->e._node->name,
               _el->bjt->model.name,
               _el->bjt->area);
        break;
    case 'm':
        printf("%s %s %s %s %s %s L=%e W=%e",
               _el->name,
               _el->mos->d._node->name,
               _el->mos->g._node->name,
               _el->mos->s._node->name,
               _el->mos->b._node->name,
               _el->mos->model.name,
               _el->mos->length,
               _el->mos->width);
        break;
    case 'd':
        printf("%s %s %s %s %+e\n",
               _el->name,
               _el->diode->vplus._node->name,
               _el->diode->vminus._node->name,
               _el->diode->model.name,
               _el->value);
        break;
    default:
        printf("Unknown element type '%c' - exit.\n",_el->type);
        exit(EXIT_FAILURE);
    }
}

void print_elements(unsigned long size, struct element pool[size]) {
    unsigned long i;
    for (i=0; i<size; ++i)
        print_element(&pool[i]);
}

void print_node(struct node *n) {
    assert(n);
    printf("nuid (%lu): node %s connects %lu elements\n",n->nuid,n->name,n->refs);
}

void print_nodes(unsigned long size, struct node node_pool[size]) {
    unsigned long i;
    for (i=0; i<size; ++i)
        print_node(&node_pool[i]);
}

void parser_init() {
    line_num = 1;

    if (el_group1_pool) {
        unsigned long i;
        for (i=0; i<el_group1_pool_next; ++i) {
            struct element *_el = &el_group1_pool[i];
            free(_el->name);
        }
        free(el_group1_pool);
        el_group1_pool = NULL;
    }

    if (el_group2_pool) {
        unsigned long i;
        for (i=0; i<el_group2_pool_next; ++i) {
            struct element *_el = &el_group2_pool[i];
            free(_el->name);
        }
        free(el_group2_pool);
        el_group2_pool = NULL;
    }

    if (node_pool) {
        unsigned long i;
        for (i=1; i<node_pool_next; ++i) {
            struct node *_n = &node_pool[i];
            free(_n->name);
        }
        free(node_pool);
        node_pool = NULL;
    }

    if (node_hash_table)
        hash_clean_table(node_hash_table);
    node_hash_table = hash_create_table(node_hash_size);

    __euid__ = 0;

    el_group1_pool_size = INIT_EL_POOL_SIZE;
    el_group1_pool_next = 0;
    el_group1_pool =
        (struct element *)malloc(el_group1_pool_size * sizeof(struct element));
    if (!el_group1_pool) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    el_group2_pool_size = INIT_EL_POOL_SIZE;
    el_group2_pool_next = 0;
    el_group2_pool =
        (struct element *)malloc(el_group2_pool_size * sizeof(struct element));
    if (!el_group2_pool) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    __nuid__ = 1;
    node_pool_size = INIT_NODE_POOL_SIZE;
    node_pool_next = 1;  //reserve the first node to be the ground node
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
    ground->el_size = 0;
    ground->attached_el = NULL;

    struct container_node *_cground =
        (struct container_node*)malloc(sizeof(struct container_node));
    if (!_cground) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    _cground->nuid = ground->nuid;
    _cground->_node = ground;
    hash_insert(node_hash_table,ground->name,_cground);

    cmd_pool_size = INIT_CMD_POOL_SIZE;
    cmd_pool_next = 0;

#if 0
    //do NOT free cmd_pool, it is going to be used by analysis later
    if (cmd_pool)
        free(cmd_pool);
#endif

    cmd_pool = (struct command*)malloc(cmd_pool_size * sizeof(struct command));
    if (!cmd_pool) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    counter_cmd_plot = 0;
    counter_cmd_print = 0;
}

int is_semantically_correct() {
    struct node *ground = &node_pool[0];
    if (ground->refs == 0) {
        printf("error: missing ground node\n");
        return SEMANTIC_ERRORS;
    }
    return 0;
}

struct file_info *open_file(const char *filename) {
    struct file_info *finfo;
    finfo = (struct file_info *)malloc(sizeof(struct file_info));
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

void close_file(struct file_info **finfo) {
    int error = munmap((*finfo)->raw_begin,(*finfo)->size);
    if (error) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    free(*finfo);
    *finfo = NULL;
}

void parse_file(const char *filename, struct netlist_info *netlist) {
    parser_init();
    assert(!current_input);
    current_input = open_file(filename);

    char *ptr;
    for (ptr = current_input->raw_begin; ptr; )
        parse_line(&ptr);

    close_file(&current_input);

    assert(netlist);
    netlist->error = is_semantically_correct();
    netlist->node_size = node_pool_next;
    netlist->el_size = el_group1_pool_next + el_group2_pool_next;
    netlist->el_group1_size = el_group1_pool_next;
    netlist->el_group2_size = el_group2_pool_next;
    netlist->node_pool = node_pool;
    netlist->el_group1_pool = el_group1_pool;
    netlist->el_group2_pool = el_group2_pool;
    netlist->cmd_pool_size = cmd_pool_next;
    netlist->cmd_pool = cmd_pool;
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
    /* skip all CR and LF */

    char *end = current_input->raw_end;
    while (*buf != end) {
        if (**buf == '\r')
            ;
        else if (**buf != '\n')
            break;
        else
            line_num++;
        (*buf)++;
    }
    if (*buf == end)
        *buf = NULL;
}

static int discard_line(char **buf) {
    int bad_chars = 0;
    char *end = current_input->raw_end;
    if (!(*buf))
        return 0;
    while (*buf != end) {
        if (**buf == '\n')
            break;
        else if (**buf != '\r')
            bad_chars++;
        (*buf)++;
    }
    if (*buf == end)
        *buf = NULL;
    return bad_chars;
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
    int garbage = discard_line(buf);
    if (garbage) {
        printf("warning: %d garbage characters at end of line %lu\n", garbage, line_num);
        exit(EXIT_FAILURE);
    }
}

inline static int isdelimiter(char c) {
    if (isspace(c))
        return 1;
    if (isalnum(c))
        return 0;

    //custom ispunct()
    switch (c) {
    case '!':
    case '"':
    case '#':
        return 1;
    case '$':
        return 0;
    case '%':
    case '&':
    case 0x27:
    case '(':
    case ')':
    case '*':
    case '+':
    case ',':
        return 1;
    case '-':
        return 0;
    case '.':
    case '/':
    case ':':
    case ';':
    case '<':
    case '=':
    case '>':
    case '?':
    case '@':
    case '[':
    case '\\':
    case ']':
    case '^':
          return 1;
    case '_':
        return 0;
    case '`':
    case '{':
    case '|':
    case '}':
    case '~':
        return 1;
    }

    return 1;
}

char *parse_string(char **buf, char *info) {
    //printf("in function: %s\n",__FUNCTION__);

    if (!*buf || isdelimiter(**buf)) {
        printf("error:%lu: expected %s - exit.\n",line_num,info);
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
    name = strncpy(name,start,size);
    name[size] = '\0';

    //convert to lowercase
    int i;
    for (i=0; i<size; ++i)
        name[i] = tolower(name[i]);

    //printf("debug: new string : %s\n",name);

    parse_eat_whitechars(buf);

    //printf("debug: new string \"%s\"\tsize=%d\n",*name,size);

    //printf("out of function: %s\n",__FUNCTION__);
    return name;
}

char parse_char(char **buf, char *patterns, char *info) {
    assert(patterns && strlen(patterns));

    char *end = current_input->raw_end;
    if (*buf == end)
        *buf = NULL;
    if (!*buf) {
        printf("error:%lu: expected %s - exit.\n",line_num,info);
        exit(EXIT_FAILURE);
    }

    char c = tolower(**buf);
    (*buf)++;
    if (*buf == end)
        *buf = NULL;

    char *p = patterns;
    while (*p != '\0' && c != tolower(*p))
        p++;

    if (*p == '\0') {
        printf("error:%lu: parsing %s, expected '%s' got '%c' 0x%02x (hex ascii) - exit\n",
               line_num,info,patterns,c,c);
        exit(EXIT_FAILURE);
    }

    return c;
}

dfloat_t parse_value(char **buf, char *prefix, char *info) {
    //printf("in function: %s\n",__FUNCTION__);

    if (!*buf || isspace(**buf)) {
        printf("error:%lu: expected %s - exit\n",line_num,info);
        exit(EXIT_FAILURE);
    }

    if (prefix) {
        int i;
        int size = strlen(prefix);
        for (i=0; i<size; ++i) {
            char c = tolower(**buf);
            char p = tolower(prefix[i]);
            if (c != p) {
                printf("error:%lu: expected prefix '%s' before value - exit\n",
                       line_num,prefix);
                exit(EXIT_FAILURE);
            }
            (*buf)++;
        }
    }

    char *end = current_input->raw_end;
    errno = 0;

    dfloat_t value;
    int offset = 0;
#ifdef PRECISION_DOUBLE
    int status = sscanf(*buf,"%lf%n",&value,&offset);
#else
    int status = sscanf(*buf,"%f%n",&value,&offset);
#endif
    if (errno) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    if (status == EOF) {
        printf("error: sscanf() early fail\n");
        exit(EXIT_FAILURE);
    }
    if (status == 0) {
        printf("error: sscanf() bad input '%10s'\n",*buf);
        exit(EXIT_FAILURE);
    }

    assert(offset);
    *buf += offset;

    if (*buf == end)
        *buf = NULL;
    if (*buf && !isdelimiter(**buf)) {
        printf("error:%lu: invalid character '%c' after value %lf - exit.\n",
               line_num,**buf,value);
        exit(EXIT_FAILURE);
    }

    parse_eat_whitechars(buf);

    //printf("debug: value=%lf\n",*value);

#if 0
    if (value == 0) {
        printf("error: %s cannot be zero - exit\n",info);
        exit(EXIT_FAILURE);
    }
#endif

    return value;
}

dfloat_t parse_value_optional(char **buf, char *prefix, dfloat_t default_value) {
    //printf("in function: %s\n",__FUNCTION__);

    if (!*buf || isspace(**buf))
        return default_value;
    else
        return parse_value(buf,prefix,"value");
}

static dfloat_t get_init_transient_value(struct element *el) {
    assert(el->type == 'v' || el->type == 'i');
    struct _source_ *s = el->_vi;
    assert(s->transient);

    switch (s->transient->type) {
    case TR_EXP:
        return s->transient->data.exp.i1;
    case TR_SIN:
        return s->transient->data.sin.i1;
    case TR_PULSE:
        return s->transient->data.pulse.i1;
    case TR_PWL:
        if (s->transient->data.pwl.next && s->transient->data.pwl.pair[0].time == 0)
            return s->transient->data.pwl.pair[0].value;
        return el->value;
    }
    return el->value;
}

static void check_transient(struct element *el) {
    assert(el);
    dfloat_t initial_transient_value = get_init_transient_value(el);
    if (initial_transient_value != el->value) {
        printf("***  WARNING  ***    transient analysis may produce inaccurate results\n");
        printf("                     in source '%s':\n",el->name);
        printf("                     reason: (dc value) %lf != %f (initial transient value)\n",
               el->value,initial_transient_value);
    }
}

void parse_element(char **buf) {
    //printf("in function: %s\n",__FUNCTION__);

    assert(*buf);
    char type = tolower(**buf);
    struct element *s_el = get_new_element(type);

    switch (type) {
    case 'v':
    case 'i': {
        s_el->name = parse_string(buf,"voltage/current source name");
        s_el->_vi->vplus = parse_node(buf,s_el,CONN_VPLUS);
        s_el->_vi->vminus = parse_node(buf,s_el,CONN_VMINUS);
        s_el->value = parse_value(buf,NULL,"voltage/current value");
        s_el->_vi->transient = parse_transient(buf);
        if (s_el->_vi->transient)
            check_transient(s_el);
        break;
    }
    case 'r':
    case 'c':
    case 'l': {
        s_el->name = parse_string(buf,"rlc element name");
        s_el->_rcl->vplus = parse_node(buf,s_el,CONN_VPLUS);
        s_el->_rcl->vminus = parse_node(buf,s_el,CONN_VMINUS);
        s_el->value = parse_value(buf,NULL,"rcl value");
        break;
    }
    case 'q': {
        s_el->name = parse_string(buf,"bjt name");
        s_el->bjt->c = parse_node(buf,s_el,CONN_BJT_C);
        s_el->bjt->b = parse_node(buf,s_el,CONN_BJT_B);
        s_el->bjt->e = parse_node(buf,s_el,CONN_BJT_E);
        s_el->bjt->model.name = parse_string(buf,"bjt model name");
        s_el->bjt->area = parse_value_optional(buf,NULL,DEFAULT_BJT_AREA);
        break;
    }
    case 'm': {
        s_el->name = parse_string(buf,"mos name");
        s_el->mos->d = parse_node(buf,s_el,CONN_MOS_D);
        s_el->mos->g = parse_node(buf,s_el,CONN_MOS_G);
        s_el->mos->s = parse_node(buf,s_el,CONN_MOS_S);
        s_el->mos->b = parse_node(buf,s_el,CONN_MOS_B);
        s_el->mos->model.name = parse_string(buf,"mos model name");
        s_el->mos->l = parse_value(buf,"l=","mos length value");
        s_el->mos->w = parse_value(buf,"w=","mos width value");
        break;
    }
    case 'd': {
        s_el->name = parse_string(buf,"diode name");
        s_el->diode->vplus = parse_node(buf,s_el,CONN_VPLUS);
        s_el->diode->vminus = parse_node(buf,s_el,CONN_VMINUS);
        s_el->diode->model.name = parse_string(buf,"diode model name");
        s_el->diode->area = parse_value_optional(buf,NULL,DEFAULT_DIODE_AREA);
        break;
    }
    default:
        printf("Unknown element type '%c' - exit.\n",type);
        exit(EXIT_FAILURE);
    }
}

//these must be in the same order as in the enum transient_type in datatypes.h
static const char *str_trans_pool[] = { "exp", "sin", "pulse", "pwl" };

static inline enum transient_type get_transient_type(const char *s) {
    assert(s);
    int i;
    const int size = sizeof(str_trans_pool)/sizeof(*str_trans_pool);
    for (i=0; i<size; ++i)
        if (strcmp(s,str_trans_pool[i]) == 0)
            return (enum transient_type)i;
    printf("Unknown transient type analysis '%s' - exit\n",s);
    exit(EXIT_FAILURE);
}

struct _transient_ *parse_transient(char **buf) {
    if (!(*buf) || **buf == '\r' || **buf == '\n')
        //no transient, new line found
        return NULL;

    //yes transient :)
    struct _transient_ *trans = (struct _transient_ *)calloc(1,sizeof(struct _transient_));
    if (!trans) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    char *str_trans = parse_string(buf,"transient type");
    trans->type = get_transient_type(str_trans);
    free(str_trans);

    switch (trans->type) {
    case TR_EXP:
        parse_char(buf,"(","'(' lparen");
        parse_eat_whitechars(buf);
        trans->update.exp = analysis_transient_call_exp;
        trans->data.exp.i1 = parse_value(buf,NULL,"exp.i1");
        trans->data.exp.i2 = parse_value(buf,NULL,"exp.i2");
        trans->data.exp.td1 = parse_value(buf,NULL,"exp.td1");
        trans->data.exp.tc1 = parse_value(buf,NULL,"exp.tc1");
        trans->data.exp.td2 = parse_value(buf,NULL,"exp.td2");
        trans->data.exp.tc2 = parse_value(buf,NULL,"exp.tc2");
        parse_char(buf,")","')' rparen");
        parse_eat_whitechars(buf);
        break;
    case TR_SIN:
        parse_char(buf,"(","'(' lparen");
        parse_eat_whitechars(buf);
        trans->update.sin = analysis_transient_call_sin;
        trans->data.sin.i1 = parse_value(buf,NULL,"sin.i1");
        trans->data.sin.ia = parse_value(buf,NULL,"sin.ia");
        trans->data.sin.fr = parse_value(buf,NULL,"sin.fr");
        trans->data.sin.td = parse_value(buf,NULL,"sin.td");
        trans->data.sin.df = parse_value(buf,NULL,"sin.df");
        trans->data.sin.ph = parse_value(buf,NULL,"sin.ph");
        parse_char(buf,")","')' rparen");
        parse_eat_whitechars(buf);
        break;
    case TR_PULSE:
        parse_char(buf,"(","'(' lparen");
        parse_eat_whitechars(buf);
        trans->update.pulse = analysis_transient_call_pulse;
        trans->data.pulse.i1 = parse_value(buf,NULL,"pulse.i1");
        trans->data.pulse.i2 = parse_value(buf,NULL,"pulse.i2");
        trans->data.pulse.td = parse_value(buf,NULL,"pulse.td");
        trans->data.pulse.tr = parse_value(buf,NULL,"pulse.tr");
        trans->data.pulse.tf = parse_value(buf,NULL,"pulse.tf");
        trans->data.pulse.pw = parse_value(buf,NULL,"pulse.pw");
        trans->data.pulse.per = parse_value(buf,NULL,"pulse.per");
        parse_char(buf,")","')' rparen");
        parse_eat_whitechars(buf);
        trans->data.pulse.k = 0;
        break;
    case TR_PWL: {
        const int default_pair_size = 8;

        trans->update.pwl = analysis_transient_call_pwl;
        trans->data.pwl.size = default_pair_size;
        trans->data.pwl.next = 0;
        trans->data.pwl.pair =
            (struct transient_pwl_pair *)calloc(default_pair_size,
                                                sizeof(struct transient_pwl_pair));
        if (!trans->data.pwl.pair) {
            perror(__FUNCTION__);
            exit(EXIT_FAILURE);
        }

        while (*buf && **buf != '\r' && **buf != '\n') {
            parse_char(buf,"(","'(' lparen");
            parse_eat_whitechars(buf);
            trans->data.pwl.pair[trans->data.pwl.next].time =
                parse_value(buf,NULL,"pwl pair (time field)");
            trans->data.pwl.pair[trans->data.pwl.next].value =
                parse_value(buf,NULL,"pwl pair (value field)");
            trans->data.pwl.next++;
            grow((void **)&trans->data.pwl.pair,trans->data.pwl.size,
                 sizeof(struct transient_pwl_pair),trans->data.pwl.next,
                 NO_REBUILD);
            parse_char(buf,")","')' rparen");
            parse_eat_whitechars(buf);
        }
        break;
    }
    }

    return trans;
}

void parse_comment(char **buf) {
    //printf("in function: %s\n",__FUNCTION__);

    char *end = current_input->raw_end;
    while (*buf != end) {
        if (**buf == '\n')
            break;
        (*buf)++;
    }
    if (*buf == end)
        *buf = NULL;
}

//these must be in the same order as in the enum cmd_type in datatypes.h
static const char *cmd_base[] = { "option", "dc", "plot", "print", "tran" };

//these must be in the same order as in the enum cmd_opt_type in datatypes.h
static const char *cmd_opt_base[] = { "spd", "iter", "itol", "sparse", "tr", "be" };

static inline enum cmd_type get_cmd_type(char *cmd) {
    assert(cmd);
    assert(sizeof(cmd_base)/sizeof(char *) == CMD_BAD_COMMAND
           && "unbalanced cmd_base and enum cmd_type");
    int i;
    for (i=0; i<CMD_SIZE; ++i)
        if (strcmp(cmd, cmd_base[i]) == 0)
            return (enum cmd_type)i;
    return CMD_BAD_COMMAND;
}

static inline enum cmd_option_type get_cmd_opt_type(char *opt) {
    assert(opt);
    assert(sizeof(cmd_opt_base)/sizeof(char *) == CMD_OPT_BAD_OPTION
           && "unbalanced cmd_opt_base and enum cmd_option_type");
    int i;
    for (i=0; i<CMD_OPT_SIZE; ++i)
        if (strcmp(opt, cmd_opt_base[i]) == 0)
            return (enum cmd_option_type)i;
    return CMD_OPT_BAD_OPTION;
}

static char *create_log_filename(char *prefix, int num) {
    char *log_filename = (char*)malloc(MAX_LOG_FILENAME * sizeof(char));
    if (!log_filename) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    int size = snprintf(log_filename,MAX_LOG_FILENAME,"%s_%05d.log",prefix,num);
    assert(size < MAX_LOG_FILENAME);
    if (size < 0) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    return log_filename;
}

void parse_print_plot_item(char **buf, struct command *cmd) {
    assert(cmd);

    int idx = cmd->print_plot.item_num;
    if (idx == MAX_PRINT_PLOT_ITEMS) {
        printf("Reached limit for print/plot items (%d) - exit\n",
               MAX_PRINT_PLOT_ITEMS);
        exit(EXIT_FAILURE);
    }

    char el_type = parse_char(buf,"vi","print/plot type");
    parse_eat_whitechars(buf);
    assert(el_type == 'v' || el_type == 'i');

    parse_eat_whitechars(buf);
    parse_char(buf,"(","lparen");
    parse_eat_whitechars(buf);

    char *node_name = parse_string(buf,"node name");
    if (el_type == 'v') {
        struct container_node *_cnode = hash_get(node_hash_table,node_name);
        if (!_cnode) {
            printf("error: node '%s' not found - exit\n",node_name);
            free(node_name);
            exit(EXIT_FAILURE);
        }

        cmd->print_plot.item[idx].type = el_type;
        cmd->print_plot.item[idx].cnode =*_cnode;
    }
    else {
        //TODO: we need hash tables for elements for faster searches
        unsigned int i;
        struct element *_el = NULL;
        for (i=0; i<el_group1_pool_next; ++i) {
            struct element *el = &el_group1_pool[i];
            if (el->type == 'i' && strcmp(el->name,node_name) == 0) {
                _el = el;
                break;
            }
        }
        if (!_el) {
            printf("error: element '%s' not found - exit\n",node_name);
            free(node_name);
            exit(EXIT_FAILURE);
        }

        cmd->print_plot.item[idx].type = el_type;
        struct container_element _cel = { .type=_el->type,
                                          .idx=_el->idx,
                                          ._el=_el };
        cmd->print_plot.item[idx].cel =_cel;
    }

    cmd->print_plot.item_num++;

    parse_eat_whitechars(buf);
    parse_char(buf,")","rparen");
    parse_eat_whitechars(buf);
}

static enum cmd_option_type parse_option(char **buf) {
    parse_eat_whitechars(buf);
    char *backup_pos = *buf;
    char *option = parse_string(buf,"option");

    if (!strcmp(option,"method=")) {
        free(option);
        option = parse_string(buf,"transient method");
    }
    else if (!strcmp(option,"method")) {
        parse_char(buf,"=","'=' asignment");
        parse_eat_whitechars(buf);
        free(option);
        option = parse_string(buf,"transient method");
    }

    enum cmd_option_type type = get_cmd_opt_type(option);
    if (type == CMD_OPT_BAD_OPTION) {
        printf("***  WARNING  ***    Unknown .option argument '%s' - error\n",option);
        *buf = backup_pos;
    }
    free(option);
    return type;
}

void parse_command(char **buf) {
    //printf("in function: %s\n",__FUNCTION__);

    /* eat '.' */
    (*buf)++;

    char *cmd = parse_string(buf,"command name");
    enum cmd_type type = get_cmd_type(cmd);

    if (type == CMD_BAD_COMMAND) {
        printf("***  WARNING  ***    Unknown command '%s' - error\n",cmd);
        free(cmd);
        return;
    }

    struct command new_cmd;
    memset(&new_cmd,0,sizeof(new_cmd));
    new_cmd.type = type;

    switch (type) {
    case CMD_OPTION: {
        do {
            enum cmd_option_type type = parse_option(buf);
            new_cmd.option[type] = 1;
            if (new_cmd.option[CMD_OPT_BAD_OPTION])
                return;
        } while (*buf && !isdelimiter(**buf));
        break;
    }
    case CMD_DC: {
        char *name = parse_string(buf,"dc source");
        unsigned long i;

        char dc_type = name[0];
        assert(dc_type == 'v' || dc_type == 'i');

        struct element *dc_source = NULL;
        if (dc_type == 'v')
            for (i=0; i<el_group2_pool_next; ++i) {
                struct element *el = &el_group2_pool[i];
                if (el->type == 'v' && strcmp(el->name,name) == 0) {
                    dc_source = el;
                    break;
                }
            }
        else
            for (i=0; i<el_group1_pool_next; ++i) {
                struct element *el = &el_group1_pool[i];
                if (el->type == 'i' && strcmp(el->name,name) == 0) {
                    dc_source = el;
                    break;
                }
            }

        if (!dc_source) {
            printf("***  WARNING  ***    Unknown dc source '%s' - error\n",name);
            free(name);
            return;
        }

        dfloat_t begin = parse_value(buf,NULL,"dc start value");
        dfloat_t end = parse_value(buf,NULL,"dc stop value");
        dfloat_t step = parse_value(buf,NULL,"dc step value");

        //perform some sanity checks

        int error = 0;

        if (step == 0) {
            error = 1;
            printf("error:%lu: dc step cannot be zero\n",line_num);
        }
        if (begin < end && step < 0) {
            error = 1;
            printf("error:%lu: expected positive dc step\n",line_num);
        }
        if (begin > end && step > 0) {
            error = 1;
            printf("error:%lu: expected negative dc step\n",line_num);
        }

        if (error) {
            free(name);
            return;
        }

        struct container_element _cel = { .type=dc_source->type,
                                          .idx=dc_source->idx,
                                          ._el=dc_source };
        new_cmd.dc.source = _cel;
        new_cmd.dc.begin = begin;
        new_cmd.dc.end = end;
        new_cmd.dc.step = step;

        break;
    }
    case CMD_PLOT:
    case CMD_PRINT: {
        new_cmd.print_plot.item_num = 0;

        char *end = current_input->raw_end;
        while (*buf != end) {
            parse_print_plot_item(buf,&new_cmd);
            if (**buf == '\n')
                break;
        }

        if (*buf == end)
            *buf = NULL;

        if (type == CMD_PLOT)
            new_cmd.print_plot.logfile =
                create_log_filename("plot",counter_cmd_plot++);
        else
            new_cmd.print_plot.logfile =
                create_log_filename("print",counter_cmd_print++);
        break;
    }
    case CMD_TRAN:
        new_cmd.transient.time_step = parse_value(buf,NULL,"transsient time_step");
        if (new_cmd.transient.time_step <= 0.0) {
            printf("error:%lu: time_step must be positive - exit.\n",line_num);
            exit(EXIT_FAILURE);
        }
        new_cmd.transient.fin_time = parse_value(buf,NULL,"transsient fin_time");
        if (new_cmd.transient.fin_time <= 0.0 ||
            new_cmd.transient.fin_time < new_cmd.transient.fin_time) {
            printf("error:%lu: fin_time must be positive and >= to time_step - exit.\n",line_num);
            exit(EXIT_FAILURE);
        }

        break;
    default:  assert(0);
    }

    cmd_pool_size = grow((void**)&cmd_pool,cmd_pool_size,
                         sizeof(struct command),cmd_pool_next,NO_REBUILD);
    cmd_pool[cmd_pool_next] = new_cmd;
    cmd_pool_next++;

    parse_eat_whitechars(buf);
}

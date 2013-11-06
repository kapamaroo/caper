#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>

#include "hash.h"

struct hash_table *hash_create_table(unsigned int size) {
    struct hash_table *ht = (struct hash_table*)malloc(sizeof(struct hash_table));
    if (!ht) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    struct hash_element **pool;
    pool = (struct hash_element**)calloc(size, sizeof(struct hash_element*));
    if (!pool) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    ht->size = size;
    ht->pool = pool;

    return ht;
}

void hash_clean_table(struct hash_table *ht) {
    unsigned int i;
    for (i=0; i<ht->size; ++i) {
        struct hash_element *root = ht->pool[i];
        while (root) {
            struct hash_element *next = root->next;
            free(root->data);
            free(root);
            root = next;
        }
    }
    free(ht->pool);
    free(ht);
}

static inline
unsigned long hash_idx(struct hash_table *ht, char *key) {
    unsigned long idx = 0;
    int key_size = strlen(key);
    int capacity = sizeof(unsigned long)/sizeof(char);
    //ignore the first chars to avoid overflow
    int i = (key_size > capacity) ? key_size - capacity : 0;
    while (i < key_size)
        idx = (idx << CHAR_BIT) + key[i++];
    idx %= ht->size;
    return idx;
}

void hash_insert(struct hash_table *ht, char *key, void *data) {
    assert(ht);
    assert(key);
    assert(data);

    unsigned long idx = hash_idx(ht,key);

    struct hash_element *el =
        (struct hash_element*)malloc(sizeof(struct hash_element));
    if (!el) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    el->key = key;
    el->data = data;

    //first element
    if (!ht->pool[idx]) {
        el->next = NULL;
        ht->pool[idx] = el;
        return;
    }

    struct hash_element *root = ht->pool[idx];
    struct hash_element *last = root;
    int last_strcmp;
    do {
        last_strcmp = strcmp(key,root->key);
        if (last_strcmp <= 0)
            break;
        last = root;
        root = root->next;
    } while (root);

    if (!root) {
        //not found, insert to hash table at the end of the list
        el->next = NULL;
        last->next = el;
    }
    else if (last_strcmp != 0) {
        //new element, insert to hash table before root
        el->next = last->next;
        last->next = el;
    }
    else {
        //collision, key already exists
        printf("error: key '%s' already exists! - exit\n",key);
        exit(EXIT_FAILURE);
    }
}

void *hash_get(struct hash_table *ht, char *key) {
    unsigned int idx = hash_idx(ht,key);
    struct hash_element *root = ht->pool[idx];
    while (root) {
        int status = strcmp(key,root->key);
        if (status == 0)
            return root->data;
        //else if (status < 0)
        //    return NULL;
        root = root->next;
    }
    return NULL;
}

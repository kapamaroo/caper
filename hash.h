#ifndef __HASH_H__
#define __HASH_H__

struct hash_element {
    char *key;
    void *data;
    struct hash_element *next;
};

struct hash_table {
    unsigned int size;
    struct hash_element **pool;
};

struct hash_table *hash_create_table(unsigned int size);
void hash_insert(struct hash_table *ht, char *key, void *data);
void *hash_get(struct hash_table *ht, char *key);
void hash_clean_table(struct hash_table *ht);

#endif

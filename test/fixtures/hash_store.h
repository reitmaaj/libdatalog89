#ifndef HASH_STORE_H
#define HASH_STORE_H

#include <stddef.h>

#include <dl89.h>

typedef struct hash_store hash_store;

hash_store *hash_store_new(void);
void hash_store_free(hash_store *store);
dl89_store hash_store_dl89(hash_store *store);

int hash_store_has(const hash_store *store, dl89_rel relation, size_t arity,
                   const dl89_const *tuple);
size_t hash_store_count(const hash_store *store, dl89_rel relation,
                        size_t arity);
size_t hash_store_total(const hash_store *store);
int hash_store_equals(const hash_store *a, const hash_store *b);

unsigned long hash_store_scan_opens(const hash_store *store);
unsigned long hash_store_scan_closes(const hash_store *store);

#endif

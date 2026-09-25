#ifndef REF_STORE_H
#define REF_STORE_H

#include <stddef.h>
#include <stdio.h>

#include <dl89.h>

typedef struct ref_store ref_store;

enum
{
    REF_ORDER_FORWARD = 0,
    REF_ORDER_REVERSE = 1,
    REF_ORDER_RANDOM = 2
};

ref_store *ref_store_new(void);
void ref_store_free(ref_store *store);
void ref_store_set_order(ref_store *store, int order);
void ref_store_set_seed(ref_store *store, unsigned long seed);
dl89_store ref_store_dl89(ref_store *store);

int ref_store_has(const ref_store *store, dl89_rel relation, size_t arity,
                  const dl89_const *tuple);
size_t ref_store_count(const ref_store *store, dl89_rel relation,
                       size_t arity);
size_t ref_store_total(const ref_store *store);
int ref_store_equals(const ref_store *a, const ref_store *b);
void ref_store_dump(const ref_store *store, FILE *out);

unsigned long ref_store_scan_opens(const ref_store *store);
unsigned long ref_store_scan_closes(const ref_store *store);
unsigned long ref_store_insert_calls(const ref_store *store);
unsigned long ref_store_new_inserts(const ref_store *store);

#endif

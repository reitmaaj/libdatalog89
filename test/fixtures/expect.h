#ifndef DL89_EXPECT_H
#define DL89_EXPECT_H

/* Exact-relation assertions over the reference store. */

#include <dl89.h>

#include "ref_store.h"

void expect_tuple(const ref_store *store, dl89_rel relation, size_t arity,
                  const dl89_const *tuple);
void expect_absent(const ref_store *store, dl89_rel relation, size_t arity,
                   const dl89_const *tuple);
void expect_count(const ref_store *store, dl89_rel relation, size_t arity,
                  size_t count);

#endif

#ifndef DATALOG89_EXPECT_H
#define DATALOG89_EXPECT_H

/* Exact-relation assertions over the reference store. */

#include <datalog89.h>

#include "ref_store.h"

void expect_tuple(const ref_store *store, datalog89_rel relation, size_t arity,
                  const datalog89_const *tuple);
void expect_absent(const ref_store *store, datalog89_rel relation, size_t arity,
                   const datalog89_const *tuple);
void expect_count(const ref_store *store, datalog89_rel relation, size_t arity,
                  size_t count);

#endif

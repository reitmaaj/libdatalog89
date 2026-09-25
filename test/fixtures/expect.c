/* expect.c - exact-relation assertions over the reference store. */

#include "expect.h"

#include <test.h>

void expect_tuple(const ref_store *store, dl89_rel relation, size_t arity,
                  const dl89_const *tuple)
{
    T_ASSERT(ref_store_has(store, relation, arity, tuple) == 1);
}

void expect_absent(const ref_store *store, dl89_rel relation, size_t arity,
                   const dl89_const *tuple)
{
    T_ASSERT(ref_store_has(store, relation, arity, tuple) == 0);
}

void expect_count(const ref_store *store, dl89_rel relation, size_t arity,
                  size_t count)
{
    T_EQ_SIZE(ref_store_count(store, relation, arity), count);
}

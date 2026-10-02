#ifndef DATALOG89_PRIV_PLAN_H
#define DATALOG89_PRIV_PLAN_H

#include "datalog89_priv.h"

#define DATALOG89_PRIV_NO_INDEX ((size_t)-1)

/* Immutable execution plan IR. One plan describes the whole frozen program
 * with contiguous arrays: variants point into the step array, steps point
 * into the binding-pattern array. Nothing mutates a built plan. */

enum datalog89_priv_src
{
    DATALOG89_PRIV_SRC_FULL = 0,
    DATALOG89_PRIV_SRC_DELTA = 1
};

enum datalog89_priv_bind_kind
{
    DATALOG89_PRIV_BIND_FREE = 0,
    DATALOG89_PRIV_BIND_SLOT = 1,
    DATALOG89_PRIV_BIND_CONST = 2,
    /* Bound by an earlier column of the same atom: free at scan time,
     * equality-checked during unification. */
    DATALOG89_PRIV_BIND_EQ = 3
};

/* Static per-column binding pattern of one step. */
struct datalog89_priv_bind
{
    unsigned char kind;
    size_t slot;
    datalog89_const constant;
};

struct datalog89_priv_step
{
    datalog89_rel relation;
    size_t arity;
    int source;
    size_t col_off;
};

struct datalog89_priv_variant
{
    datalog89_rel head;
    size_t head_arity;
    size_t rule_index;
    size_t first_step;
    size_t nsteps;
    size_t delta_pos;
};

/* One strongly connected component of the predicate dependency graph, in
 * scheduling (topological) order. EDB relations belong to no SCC. */
struct datalog89_priv_scc
{
    size_t first_variant;
    size_t nvariants;
    unsigned recursive;
};

struct datalog89_priv_plan
{
    struct datalog89_priv_variant *variants;
    size_t nvariants;
    struct datalog89_priv_step *steps;
    size_t nsteps;
    struct datalog89_priv_bind *cols;
    size_t ncols;
    struct datalog89_priv_scc *sccs;
    size_t nsccs;
};

#endif

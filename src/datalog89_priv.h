#ifndef DATALOG89_PRIV_INTERNAL_H
#define DATALOG89_PRIV_INTERNAL_H

#include <stddef.h>

#include <datalog89.h>

/* The empty GREEN_PURE annotation marks a function as pure for green. */
#define GREEN_PURE

#define DATALOG89_PRIV_NO_SLOT ((size_t)-1)

/* Memory seam: production datalog89_priv_mem.c wraps malloc; the
 * allocation-fault fixture implements these three symbols instead. */
void *datalog89_priv_mem_alloc(size_t size);
void *datalog89_priv_mem_realloc(void *ptr, size_t size);
void datalog89_priv_mem_free(void *ptr);

/* Compiled term: a constant or a variable-slot reference. */
typedef struct
{
    int kind;
    datalog89_const constant;
    size_t slot;
} datalog89_priv_cterm;

typedef struct
{
    datalog89_rel relation;
    size_t arity;
    datalog89_priv_cterm *terms;
} datalog89_priv_catom;

/* One level of the explicit join stack: an open store scan or a delta
 * cursor. bound_count records the slots bound by this frame's current
 * tuple so they can be undone before advancing. */
struct datalog89_priv_frame
{
    datalog89_scan *scan;
    size_t next_delta;
    size_t bound_count;
    unsigned char in_delta;
    unsigned char has_tuple;
};

typedef struct
{
    datalog89_priv_catom head;
    size_t body_count;
    datalog89_priv_catom *body;
    size_t var_count;
    datalog89_const *env;
    unsigned char *bound;
    datalog89_const *values;
    unsigned char *bvalues;
    datalog89_const *tuple;
    size_t tuple_cap;
    size_t *bound_log;
    struct datalog89_priv_frame *frames;
} datalog89_priv_crule;

typedef struct
{
    datalog89_rel relation;
    size_t arity;
} datalog89_priv_arity;

typedef struct
{
    datalog89_rel relation;
    size_t arity;
    datalog89_const *tuples;
    size_t count;
    size_t cap;
} datalog89_priv_delta;

typedef struct
{
    datalog89_priv_delta *entries;
    size_t count;
    size_t cap;
    size_t total;
} datalog89_priv_delta_table;

/* Immutable execution plan; src/datalog89_priv_plan.h defines the shape. */
struct datalog89_priv_plan;
struct datalog89_priv_variant;
struct datalog89_priv_bind;
struct datalog89_priv_step;

struct datalog89_eval
{
    datalog89_store store;
    datalog89_priv_crule *rules;
    size_t rule_count;
    size_t rule_cap;
    datalog89_priv_arity *arities;
    size_t arity_count;
    size_t arity_cap;
    int running;
    struct datalog89_priv_plan *plan;
    size_t plan_builds;
};

int datalog89_priv_store_valid(const datalog89_store *store);

int datalog89_priv_rule_check(const datalog89_eval *eval,
                              const datalog89_rule *rule);
int datalog89_priv_rule_install(datalog89_eval *eval,
                                const datalog89_rule *rule);

int datalog89_priv_crule_compile(const datalog89_rule *rule,
                                 datalog89_priv_crule *out);
void datalog89_priv_crule_release(datalog89_priv_crule *rule);
const datalog89_priv_arity *
datalog89_priv_registry_find(const datalog89_eval *eval,
                             datalog89_rel relation);
int datalog89_priv_registry_add(datalog89_eval *eval, datalog89_rel relation,
                                size_t arity);
void datalog89_priv_registry_truncate(datalog89_eval *eval, size_t count);

void datalog89_priv_delta_table_clear(datalog89_priv_delta_table *table);
void datalog89_priv_delta_table_free(datalog89_priv_delta_table *table);
int datalog89_priv_delta_record(datalog89_priv_delta_table *table,
                                datalog89_rel relation, size_t arity,
                                const datalog89_const *tuple);
const datalog89_priv_delta *
datalog89_priv_delta_find(const datalog89_priv_delta_table *table,
                          datalog89_rel relation, size_t arity);

int datalog89_priv_plan_build(datalog89_eval *eval);
void datalog89_priv_plan_free(struct datalog89_priv_plan *plan);
void datalog89_priv_plan_invalidate(datalog89_eval *eval);

int datalog89_priv_join_variant(datalog89_eval *eval,
                                datalog89_priv_crule *rule,
                                const struct datalog89_priv_plan *plan,
                                const struct datalog89_priv_variant *variant,
                                datalog89_priv_delta_table *sink,
                                const datalog89_priv_delta *delta);
int datalog89_priv_fixpoint_run(datalog89_eval *eval);

#endif

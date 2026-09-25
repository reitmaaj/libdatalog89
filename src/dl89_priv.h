#ifndef DL89_PRIV_INTERNAL_H
#define DL89_PRIV_INTERNAL_H

#include <stddef.h>

#include <dl89.h>

/* The empty GREEN_PURE annotation marks a function as pure for green. */
#define GREEN_PURE

#define DL89_PRIV_NO_SLOT ((size_t)-1)

/* Memory seam: production dl89_priv_mem.c wraps malloc; the allocation-fault
 * fixture implements these three symbols instead. */
void *dl89_priv_mem_alloc(size_t size);
void *dl89_priv_mem_realloc(void *ptr, size_t size);
void dl89_priv_mem_free(void *ptr);

/* Compiled term: a constant or a variable-slot reference. */
typedef struct
{
    int kind;
    dl89_const constant;
    size_t slot;
} dl89_priv_cterm;

typedef struct
{
    dl89_rel relation;
    size_t arity;
    dl89_priv_cterm *terms;
} dl89_priv_catom;

typedef struct
{
    dl89_priv_catom head;
    size_t body_count;
    dl89_priv_catom *body;
    size_t var_count;
    dl89_const *env;
    unsigned char *bound;
    dl89_const *values;
    unsigned char *bvalues;
    dl89_const *tuple;
    size_t tuple_cap;
    size_t *bound_log;
} dl89_priv_crule;

typedef struct
{
    dl89_rel relation;
    size_t arity;
} dl89_priv_arity;

typedef struct
{
    dl89_rel relation;
    size_t arity;
    dl89_const *tuples;
    size_t count;
    size_t cap;
} dl89_priv_delta;

typedef struct
{
    dl89_priv_delta *entries;
    size_t count;
    size_t cap;
    size_t total;
} dl89_priv_delta_table;

struct dl89_eval
{
    dl89_store store;
    dl89_priv_crule *rules;
    size_t rule_count;
    size_t rule_cap;
    dl89_priv_arity *arities;
    size_t arity_count;
    size_t arity_cap;
    int running;
};

int dl89_priv_store_valid(const dl89_store *store);

int dl89_priv_rule_check(const dl89_eval *eval, const dl89_rule *rule);
int dl89_priv_rule_install(dl89_eval *eval, const dl89_rule *rule);

int dl89_priv_crule_compile(const dl89_rule *rule, dl89_priv_crule *out);
void dl89_priv_crule_release(dl89_priv_crule *rule);
const dl89_priv_arity *dl89_priv_registry_find(const dl89_eval *eval,
                                               dl89_rel relation);
int dl89_priv_registry_add(dl89_eval *eval, dl89_rel relation, size_t arity);
void dl89_priv_registry_truncate(dl89_eval *eval, size_t count);

void dl89_priv_delta_table_clear(dl89_priv_delta_table *table);
void dl89_priv_delta_table_free(dl89_priv_delta_table *table);
int dl89_priv_delta_record(dl89_priv_delta_table *table, dl89_rel relation,
                           size_t arity, const dl89_const *tuple);
const dl89_priv_delta *dl89_priv_delta_find(const dl89_priv_delta_table *table,
                                            dl89_rel relation, size_t arity);

int dl89_priv_join_rule(dl89_eval *eval, dl89_priv_crule *rule,
                        dl89_priv_delta_table *sink,
                        const dl89_priv_delta *delta, size_t delta_pos);
int dl89_priv_fixpoint_run(dl89_eval *eval);

#endif

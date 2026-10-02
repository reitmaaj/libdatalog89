/* datalog89_fixpoint.c - least-fixed-point scheduling over the plan.
 *
 * Round 0 evaluates each rule's no-delta seed variant over the full store
 * and records newly inserted tuples in the delta table. Each later round
 * evaluates, for every variant whose delta position references a derivable
 * relation and whose delta is nonempty, that variant: the delta step
 * iterates the previous round's delta while other steps scan the store.
 * The run stops when a round derives nothing new. Variant structure is
 * static; the scheduler only selects which precompiled variants fire. */

#include <string.h>

#include "datalog89_priv.h"
#include "datalog89_priv_plan.h"

GREEN_PURE
static size_t cap_double(size_t cap)
{
    if (cap == 0)
    {
        return 8;
    }
    if (cap > ((size_t)-1) / 2)
    {
        return 0;
    }
    return cap * 2;
}

static void table_init(datalog89_priv_delta_table *table)
{
    table->entries = NULL;
    table->count = 0;
    table->cap = 0;
    table->total = 0;
}

void datalog89_priv_delta_table_clear(datalog89_priv_delta_table *table)
{
    size_t i;

    for (i = 0; i < table->count; ++i)
    {
        table->entries[i].count = 0;
    }
    table->total = 0;
}

void datalog89_priv_delta_table_free(datalog89_priv_delta_table *table)
{
    size_t i;

    for (i = 0; i < table->count; ++i)
    {
        datalog89_priv_mem_free(table->entries[i].tuples);
    }
    datalog89_priv_mem_free(table->entries);
    table_init(table);
}

const datalog89_priv_delta *
datalog89_priv_delta_find(const datalog89_priv_delta_table *table,
                          datalog89_rel relation, size_t arity)
{
    size_t i;

    for (i = 0; i < table->count; ++i)
    {
        if (table->entries[i].relation == relation)
        {
            if (table->entries[i].arity == arity)
            {
                return &table->entries[i];
            }
        }
    }
    return NULL;
}

static int table_grow(datalog89_priv_delta_table *table)
{
    datalog89_priv_delta *grown;
    size_t cap;

    cap = table->cap;
    while (cap <= table->count)
    {
        cap = cap_double(cap);
        if (cap == 0)
        {
            return DATALOG89_ENOMEM;
        }
    }
    if (cap > ((size_t)-1) / sizeof(*grown))
    {
        return DATALOG89_ENOMEM;
    }
    grown = datalog89_priv_mem_realloc(table->entries, cap * sizeof(*grown));
    if (grown == NULL)
    {
        return DATALOG89_ENOMEM;
    }
    table->entries = grown;
    table->cap = cap;
    return DATALOG89_OK;
}

static int table_add(datalog89_priv_delta_table *table, datalog89_rel relation,
                     size_t arity, size_t *out_index)
{
    int st;

    if (table->count == table->cap)
    {
        st = table_grow(table);
        if (st != DATALOG89_OK)
        {
            return st;
        }
    }
    table->entries[table->count].relation = relation;
    table->entries[table->count].arity = arity;
    table->entries[table->count].tuples = NULL;
    table->entries[table->count].count = 0;
    table->entries[table->count].cap = 0;
    *out_index = table->count;
    table->count = table->count + 1;
    return DATALOG89_OK;
}

static int delta_index(datalog89_priv_delta_table *table,
                       datalog89_rel relation, size_t arity, size_t *out_index)
{
    size_t i;
    int st;

    for (i = 0; i < table->count; ++i)
    {
        if (table->entries[i].relation == relation)
        {
            if (table->entries[i].arity == arity)
            {
                *out_index = i;
                return DATALOG89_OK;
            }
        }
    }
    st = table_add(table, relation, arity, out_index);
    return st;
}

static int delta_reserve(datalog89_priv_delta *delta, size_t need)
{
    datalog89_const *grown;
    size_t cap;

    cap = delta->cap;
    while (cap < need)
    {
        cap = cap_double(cap);
        if (cap == 0)
        {
            return DATALOG89_ENOMEM;
        }
    }
    if (cap > ((size_t)-1) / sizeof(*grown))
    {
        return DATALOG89_ENOMEM;
    }
    grown = datalog89_priv_mem_realloc(delta->tuples, cap * sizeof(*grown));
    if (grown == NULL)
    {
        return DATALOG89_ENOMEM;
    }
    delta->tuples = grown;
    delta->cap = cap;
    return DATALOG89_OK;
}

int datalog89_priv_delta_record(datalog89_priv_delta_table *table,
                                datalog89_rel relation, size_t arity,
                                const datalog89_const *tuple)
{
    datalog89_priv_delta *delta;
    size_t index;
    size_t need;
    int st;

    st = delta_index(table, relation, arity, &index);
    if (st != DATALOG89_OK)
    {
        return st;
    }
    delta = &table->entries[index];
    if (arity != 0)
    {
        if (delta->count > ((size_t)-1) / arity - 1)
        {
            return DATALOG89_ENOMEM;
        }
    }
    need = (delta->count + 1) * arity;
    if (need > delta->cap)
    {
        st = delta_reserve(delta, need);
        if (st != DATALOG89_OK)
        {
            return st;
        }
    }
    if (arity > 0)
    {
        memcpy(delta->tuples + delta->count * arity, tuple,
               arity * sizeof(datalog89_const));
    }
    delta->count = delta->count + 1;
    table->total = table->total + 1;
    return DATALOG89_OK;
}

/* --- variant scheduling ------------------------------------------------- */

static int run_variant(datalog89_eval *eval,
                       const struct datalog89_priv_plan *plan,
                       size_t variant_index,
                       const datalog89_priv_delta_table *delta,
                       datalog89_priv_delta_table *next)
{
    const struct datalog89_priv_variant *variant;
    const struct datalog89_priv_step *step;
    const datalog89_priv_delta *facts;
    datalog89_priv_crule *rule;
    int st;

    variant = &plan->variants[variant_index];
    if (variant->idb_delta == 0)
    {
        return DATALOG89_OK;
    }
    step = &plan->steps[variant->first_step + variant->delta_pos];
    facts = datalog89_priv_delta_find(delta, step->relation, step->arity);
    if (facts == NULL)
    {
        return DATALOG89_OK;
    }
    rule = &eval->rules[variant->rule_index];
    st = datalog89_priv_join_variant(eval, rule, plan, variant, next, facts);
    return st;
}

static int run_round(datalog89_eval *eval,
                     const struct datalog89_priv_plan *plan,
                     const datalog89_priv_delta_table *delta,
                     datalog89_priv_delta_table *next)
{
    size_t v;
    int st;

    for (v = 0; v < plan->nvariants; ++v)
    {
        if (plan->variants[v].delta_pos == plan->variants[v].nsteps)
        {
            continue;
        }
        st = run_variant(eval, plan, v, delta, next);
        if (st != DATALOG89_OK)
        {
            return st;
        }
    }
    return DATALOG89_OK;
}

static int seed_one(datalog89_eval *eval,
                    const struct datalog89_priv_plan *plan, size_t v,
                    datalog89_priv_delta_table *delta)
{
    datalog89_priv_crule *rule;
    int st;

    rule = &eval->rules[plan->variants[v].rule_index];
    st = datalog89_priv_join_variant(eval, rule, plan, &plan->variants[v],
                                     delta, NULL);
    return st;
}

static int seed_round(datalog89_eval *eval,
                      const struct datalog89_priv_plan *plan,
                      datalog89_priv_delta_table *delta)
{
    size_t v;
    int st;

    for (v = 0; v < plan->nvariants; ++v)
    {
        if (plan->variants[v].delta_pos != plan->variants[v].nsteps)
        {
            continue;
        }
        st = seed_one(eval, plan, v, delta);
        if (st != DATALOG89_OK)
        {
            return st;
        }
    }
    return DATALOG89_OK;
}

static void table_swap(datalog89_priv_delta_table *a,
                       datalog89_priv_delta_table *b)
{
    datalog89_priv_delta_table tmp;

    tmp = *a;
    *a = *b;
    *b = tmp;
}

static void fixpoint_fail(datalog89_priv_delta_table *delta,
                          datalog89_priv_delta_table *next)
{
    datalog89_priv_delta_table_free(delta);
    datalog89_priv_delta_table_free(next);
}

static int fixpoint_step(datalog89_eval *eval,
                         const struct datalog89_priv_plan *plan,
                         datalog89_priv_delta_table *delta,
                         datalog89_priv_delta_table *next)
{
    int st;

    st = run_round(eval, plan, delta, next);
    if (st != DATALOG89_OK)
    {
        return st;
    }
    datalog89_priv_delta_table_clear(delta);
    table_swap(delta, next);
    return DATALOG89_OK;
}

int datalog89_priv_fixpoint_run(datalog89_eval *eval)
{
    datalog89_priv_delta_table delta;
    datalog89_priv_delta_table next;
    int st;

    table_init(&delta);
    table_init(&next);
    st = seed_round(eval, eval->plan, &delta);
    if (st != DATALOG89_OK)
    {
        fixpoint_fail(&delta, &next);
        return st;
    }
    while (delta.total > 0)
    {
        st = fixpoint_step(eval, eval->plan, &delta, &next);
        if (st != DATALOG89_OK)
        {
            fixpoint_fail(&delta, &next);
            return st;
        }
    }
    datalog89_priv_delta_table_free(&delta);
    datalog89_priv_delta_table_free(&next);
    return DATALOG89_OK;
}

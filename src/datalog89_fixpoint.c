/* datalog89_fixpoint.c - semi-naive least-fixed-point scheduling.
 *
 * Round 0 evaluates every rule over the full store and records newly
 * inserted tuples in the delta table. Each later round evaluates, for every
 * rule and every derivable body position, a variant whose scan at that
 * position iterates the previous round's delta while other positions scan
 * the full store. The run stops when a round derives nothing new. */

#include <string.h>

#include "datalog89_priv.h"

GREEN_PURE
static int relation_is_idb(const datalog89_eval *eval, datalog89_rel relation)
{
    size_t r;

    for (r = 0; r < eval->rule_count; ++r)
    {
        if (eval->rules[r].head.relation == relation)
        {
            return 1;
        }
    }
    return 0;
}

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

static int run_variant(datalog89_eval *eval, size_t rule_index, size_t pos,
                       const datalog89_priv_delta_table *delta,
                       datalog89_priv_delta_table *next)
{
    const datalog89_priv_catom *atom;
    const datalog89_priv_delta *facts;
    int st;

    atom = &eval->rules[rule_index].body[pos];
    if (relation_is_idb(eval, atom->relation) == 0)
    {
        return DATALOG89_OK;
    }
    facts = datalog89_priv_delta_find(delta, atom->relation, atom->arity);
    if (facts == NULL)
    {
        return DATALOG89_OK;
    }
    st = datalog89_priv_join_rule(eval, &eval->rules[rule_index], next, facts,
                                  pos);
    return st;
}

static void table_swap(datalog89_priv_delta_table *a,
                       datalog89_priv_delta_table *b)
{
    datalog89_priv_delta_table tmp;

    tmp = *a;
    *a = *b;
    *b = tmp;
}

static int run_round(datalog89_eval *eval,
                     const datalog89_priv_delta_table *delta,
                     datalog89_priv_delta_table *next)
{
    size_t r;
    size_t b;
    int st;

    for (r = 0; r < eval->rule_count; ++r)
    {
        for (b = 0; b < eval->rules[r].body_count; ++b)
        {
            st = run_variant(eval, r, b, delta, next);
            if (st != DATALOG89_OK)
            {
                return st;
            }
        }
    }
    return DATALOG89_OK;
}

static void fixpoint_fail(datalog89_priv_delta_table *delta,
                          datalog89_priv_delta_table *next)
{
    datalog89_priv_delta_table_free(delta);
    datalog89_priv_delta_table_free(next);
}

static int fixpoint_step(datalog89_eval *eval,
                         datalog89_priv_delta_table *delta,
                         datalog89_priv_delta_table *next)
{
    int st;

    st = run_round(eval, delta, next);
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
    size_t r;
    int st;

    table_init(&delta);
    table_init(&next);
    for (r = 0; r < eval->rule_count; ++r)
    {
        st = datalog89_priv_join_rule(eval, &eval->rules[r], &delta, NULL, 0);
        if (st != DATALOG89_OK)
        {
            fixpoint_fail(&delta, &next);
            return st;
        }
    }
    while (delta.total > 0)
    {
        st = fixpoint_step(eval, &delta, &next);
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

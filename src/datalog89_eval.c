/* datalog89_eval.c - evaluator lifetime, arity registry, facts, and run entry.
 */

#include "datalog89_priv.h"

GREEN_PURE
static int ops_complete(const datalog89_store_ops *ops)
{
    if (ops == NULL)
    {
        return 0;
    }
    if (ops->insert == NULL)
    {
        return 0;
    }
    if (ops->scan_open == NULL)
    {
        return 0;
    }
    if (ops->scan_next == NULL)
    {
        return 0;
    }
    if (ops->scan_close == NULL)
    {
        return 0;
    }
    return 1;
}

GREEN_PURE
int datalog89_priv_store_valid(const datalog89_store *store)
{
    if (store == NULL)
    {
        return 0;
    }
    return ops_complete(store->ops);
}

datalog89_status datalog89_eval_create(const datalog89_eval_config *config,
                                       datalog89_eval **out)
{
    datalog89_eval *eval;

    if (out == NULL)
    {
        return DATALOG89_EINVAL;
    }
    *out = NULL;
    if (config == NULL)
    {
        return DATALOG89_EINVAL;
    }
    if (datalog89_priv_store_valid(&config->store) == 0)
    {
        return DATALOG89_EINVAL;
    }
    eval = datalog89_priv_mem_alloc(sizeof(*eval));
    if (eval == NULL)
    {
        return DATALOG89_ENOMEM;
    }
    eval->store = config->store;
    eval->rules = NULL;
    eval->rule_count = 0;
    eval->rule_cap = 0;
    eval->arities = NULL;
    eval->arity_count = 0;
    eval->arity_cap = 0;
    eval->running = 0;
    eval->plan = NULL;
    eval->plan_builds = 0;
    *out = eval;
    return DATALOG89_OK;
}

static void plan_release(datalog89_eval *eval)
{
    datalog89_priv_plan_free(eval->plan);
    datalog89_priv_mem_free(eval->plan);
}

void datalog89_eval_destroy(datalog89_eval *eval)
{
    size_t i;

    if (eval == NULL)
    {
        return;
    }
    if (eval->plan != NULL)
    {
        plan_release(eval);
    }
    for (i = 0; i < eval->rule_count; ++i)
    {
        datalog89_priv_crule_release(&eval->rules[i]);
    }
    datalog89_priv_mem_free(eval->rules);
    datalog89_priv_mem_free(eval->arities);
    datalog89_priv_mem_free(eval);
}

GREEN_PURE
const datalog89_priv_arity *
datalog89_priv_registry_find(const datalog89_eval *eval, datalog89_rel relation)
{
    size_t i;

    for (i = 0; i < eval->arity_count; ++i)
    {
        if (eval->arities[i].relation == relation)
        {
            return &eval->arities[i];
        }
    }
    return NULL;
}

GREEN_PURE
static size_t arity_next_cap(size_t cap)
{
    size_t next;

    if (cap == 0)
    {
        return 8;
    }
    if (cap > ((size_t)-1) / 2)
    {
        return 0;
    }
    next = cap * 2;
    if (next > ((size_t)-1) / sizeof(datalog89_priv_arity))
    {
        return 0;
    }
    return next;
}

static int arity_grow(datalog89_eval *eval)
{
    datalog89_priv_arity *grown;
    size_t cap;

    if (eval->arity_count < eval->arity_cap)
    {
        return DATALOG89_OK;
    }
    cap = arity_next_cap(eval->arity_cap);
    if (cap == 0)
    {
        return DATALOG89_ENOMEM;
    }
    grown = datalog89_priv_mem_realloc(eval->arities, cap * sizeof(*grown));
    if (grown == NULL)
    {
        return DATALOG89_ENOMEM;
    }
    eval->arities = grown;
    eval->arity_cap = cap;
    return DATALOG89_OK;
}

int datalog89_priv_registry_add(datalog89_eval *eval, datalog89_rel relation,
                                size_t arity)
{
    int st;

    st = arity_grow(eval);
    if (st != DATALOG89_OK)
    {
        return st;
    }
    eval->arities[eval->arity_count].relation = relation;
    eval->arities[eval->arity_count].arity = arity;
    eval->arity_count = eval->arity_count + 1;
    return DATALOG89_OK;
}

void datalog89_priv_registry_truncate(datalog89_eval *eval, size_t count)
{
    if (count < eval->arity_count)
    {
        eval->arity_count = count;
    }
}

static void registry_drop_last(datalog89_eval *eval)
{
    datalog89_priv_registry_truncate(eval, eval->arity_count - 1);
}

datalog89_status datalog89_eval_add_rule(datalog89_eval *eval,
                                         const datalog89_rule *rule)
{
    int st;

    if (eval == NULL)
    {
        return DATALOG89_EINVAL;
    }
    if (eval->running != 0)
    {
        return DATALOG89_EBUSY;
    }
    st = datalog89_priv_rule_install(eval, rule);
    return (datalog89_status)st;
}

datalog89_status datalog89_eval_add_fact(datalog89_eval *eval,
                                         datalog89_rel relation, size_t arity,
                                         const datalog89_const *tuple)
{
    const datalog89_priv_arity *entry;
    int added;
    int inserted;
    int st;

    if (eval == NULL)
    {
        return DATALOG89_EINVAL;
    }
    if (arity > 0)
    {
        if (tuple == NULL)
        {
            return DATALOG89_EINVAL;
        }
    }
    if (eval->running != 0)
    {
        return DATALOG89_EBUSY;
    }
    entry = datalog89_priv_registry_find(eval, relation);
    added = 0;
    if (entry == NULL)
    {
        st = datalog89_priv_registry_add(eval, relation, arity);
        if (st != DATALOG89_OK)
        {
            return (datalog89_status)st;
        }
        added = 1;
    }
    else
    {
        if (entry->arity != arity)
        {
            return DATALOG89_EPROGRAM;
        }
    }
    inserted = 0;
    st = eval->store.ops->insert(eval->store.ctx, relation, arity, tuple,
                                 &inserted);
    if (st != 0)
    {
        if (added != 0)
        {
            registry_drop_last(eval);
        }
        return DATALOG89_ESTORE;
    }
    return DATALOG89_OK;
}

datalog89_status datalog89_eval_run(datalog89_eval *eval)
{
    int st;

    if (eval == NULL)
    {
        return DATALOG89_EINVAL;
    }
    if (eval->running != 0)
    {
        return DATALOG89_EBUSY;
    }
    eval->running = 1;
    st = DATALOG89_OK;
    if (eval->plan == NULL)
    {
        st = datalog89_priv_plan_build(eval);
    }
    if (st == DATALOG89_OK)
    {
        st = datalog89_priv_fixpoint_run(eval);
    }
    eval->running = 0;
    return (datalog89_status)st;
}

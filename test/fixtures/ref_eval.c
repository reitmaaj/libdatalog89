/* ref_eval.c - independent worklist Datalog evaluator (test oracle).
 *
 * Deliberately simple and obviously correct: seed a worklist with every
 * initial fact (and every zero-body derivation), then for each fact try it
 * at every rule/body-position whose relation and arity match, scanning the
 * remaining atoms from the store. Newly inserted facts enter the worklist.
 */

#include <stdlib.h>
#include <string.h>

#include "ref_eval.h"

typedef struct
{
    dl89_rel relation;
    size_t arity;
    dl89_const *tuple;
} re_item;

typedef struct
{
    re_item *items;
    size_t count;
    size_t cap;
    size_t pos;
    int failed;
} re_queue;

typedef struct
{
    dl89_var var;
    dl89_const value;
    int bound;
} re_binding;

typedef struct
{
    re_binding *bindings;
    size_t count;
    size_t cap;
} re_env;

typedef struct
{
    dl89_store store;
    dl89_const *values;
    unsigned char *bound;
    size_t max_arity;
} re_ctx;

static void env_init(re_env *env, re_binding *storage, size_t cap)
{
    env->bindings = storage;
    env->count = 0;
    env->cap = cap;
}

static int env_find(const re_env *env, dl89_var var, dl89_const *value)
{
    size_t i;

    for (i = 0; i < env->count; ++i)
    {
        if (env->bindings[i].var == var)
        {
            *value = env->bindings[i].value;
            return 1;
        }
    }
    return 0;
}

static int env_bind(re_env *env, dl89_var var, dl89_const value)
{
    if (env->count == env->cap)
    {
        return 0;
    }
    env->bindings[env->count].var = var;
    env->bindings[env->count].value = value;
    env->bindings[env->count].bound = 1;
    env->count = env->count + 1;
    return 1;
}

static int term_value(const dl89_term *term, const re_env *env,
                      dl89_const *value)
{
    if (term->kind == DL89_TERM_CONST)
    {
        *value = term->u.constant;
        return 1;
    }
    return env_find(env, term->u.variable, value);
}

static int unify_atom(const dl89_atom *atom, const dl89_const *tuple,
                      re_env *env, size_t *added)
{
    size_t p;
    dl89_const value;

    *added = 0;
    for (p = 0; p < atom->arity; ++p)
    {
        if (term_value(&atom->terms[p], env, &value) != 0)
        {
            if (value != tuple[p])
            {
                return 0;
            }
        }
        else
        {
            if (env_bind(env, atom->terms[p].u.variable, tuple[p]) == 0)
            {
                return 0;
            }
            *added = *added + 1;
        }
    }
    return 1;
}

static void env_rollback(re_env *env, size_t added)
{
    if (added <= env->count)
    {
        env->count = env->count - added;
    }
    else
    {
        env->count = 0;
    }
}

static int queue_push(re_queue *queue, dl89_rel relation, size_t arity,
                      const dl89_const *tuple)
{
    re_item *grown;
    dl89_const *copy;
    size_t cap;

    if (queue->count == queue->cap)
    {
        cap = queue->cap;
        if (cap == 0)
        {
            cap = 16;
        }
        else
        {
            cap = cap * 2;
        }
        grown = realloc(queue->items, cap * sizeof(*grown));
        if (grown == NULL)
        {
            queue->failed = 1;
            return 0;
        }
        queue->items = grown;
        queue->cap = cap;
    }
    copy = NULL;
    if (arity > 0)
    {
        copy = malloc(arity * sizeof(*copy));
        if (copy == NULL)
        {
            queue->failed = 1;
            return 0;
        }
        memcpy(copy, tuple, arity * sizeof(*copy));
    }
    queue->items[queue->count].relation = relation;
    queue->items[queue->count].arity = arity;
    queue->items[queue->count].tuple = copy;
    queue->count = queue->count + 1;
    return 1;
}

static void queue_pop(re_queue *queue, re_item *out)
{
    *out = queue->items[queue->pos];
    queue->pos = queue->pos + 1;
}

static void queue_clear(re_queue *queue)
{
    size_t i;

    for (i = 0; i < queue->count; ++i)
    {
        free(queue->items[i].tuple);
    }
    free(queue->items);
    queue->items = NULL;
    queue->count = 0;
    queue->cap = 0;
    queue->pos = 0;
}

static int emit_head(re_ctx *ctx, const dl89_rule *rule, re_env *env,
                     re_queue *queue)
{
    size_t p;
    dl89_const value;
    int inserted;
    int rc;

    for (p = 0; p < rule->head.arity; ++p)
    {
        if (term_value(&rule->head.terms[p], env, &value) == 0)
        {
            return 0;
        }
        ctx->values[p] = value;
    }
    inserted = 0;
    rc = ctx->store.ops->insert(ctx->store.ctx, rule->head.relation,
                                rule->head.arity, ctx->values, &inserted);
    if (rc != 0)
    {
        return 0;
    }
    if (inserted != 0)
    {
        if (queue_push(queue, rule->head.relation, rule->head.arity,
                       ctx->values) == 0)
        {
            return 0;
        }
    }
    return 1;
}

static int eval_body(re_ctx *ctx, const dl89_rule *rule, size_t depth,
                     size_t skip, const re_item *fact, re_env *env,
                     re_queue *queue)
{
    const dl89_atom *atom;
    dl89_scan *scan;
    dl89_const tuple[8];
    size_t added;
    size_t p;
    int found;
    int rc;
    int ok;

    if (depth == rule->body_count)
    {
        return emit_head(ctx, rule, env, queue);
    }
    atom = &rule->body[depth];
    if (depth == skip)
    {
        if (unify_atom(atom, fact->tuple, env, &added) == 0)
        {
            env_rollback(env, added);
            return 1;
        }
        ok = eval_body(ctx, rule, depth + 1, skip, fact, env, queue);
        env_rollback(env, added);
        return ok;
    }
    for (p = 0; p < atom->arity; ++p)
    {
        if (term_value(&atom->terms[p], env, &ctx->values[p]) != 0)
        {
            ctx->bound[p] = 1;
        }
        else
        {
            ctx->values[p] = 0;
            ctx->bound[p] = 0;
        }
    }
    scan = NULL;
    rc = ctx->store.ops->scan_open(ctx->store.ctx, atom->relation, atom->arity,
                                   ctx->values, ctx->bound, &scan);
    if (rc != 0)
    {
        return 0;
    }
    for (;;)
    {
        found = 0;
        rc = ctx->store.ops->scan_next(ctx->store.ctx, scan, tuple, &found);
        if (rc != 0)
        {
            ctx->store.ops->scan_close(ctx->store.ctx, scan);
            return 0;
        }
        if (found == 0)
        {
            break;
        }
        if (unify_atom(atom, tuple, env, &added) != 0)
        {
            if (eval_body(ctx, rule, depth + 1, skip, fact, env, queue) == 0)
            {
                env_rollback(env, added);
                ctx->store.ops->scan_close(ctx->store.ctx, scan);
                return 0;
            }
        }
        env_rollback(env, added);
    }
    ctx->store.ops->scan_close(ctx->store.ctx, scan);
    return 1;
}

static int try_fact(re_ctx *ctx, const dl89_rule *rules, size_t rule_count,
                    const re_item *fact, re_binding *storage,
                    size_t storage_cap, re_queue *queue)
{
    size_t r;
    size_t b;
    re_env env;
    int ok;

    for (r = 0; r < rule_count; ++r)
    {
        if (rules[r].body_count == 0)
        {
            continue;
        }
        for (b = 0; b < rules[r].body_count; ++b)
        {
            if (rules[r].body[b].relation != fact->relation)
            {
                continue;
            }
            if (rules[r].body[b].arity != fact->arity)
            {
                continue;
            }
            env_init(&env, storage, storage_cap);
            ok = eval_body(ctx, &rules[r], 0, b, fact, &env, queue);
            if (ok == 0)
            {
                return 0;
            }
        }
    }
    return 1;
}

static size_t total_terms(const dl89_rule *rule)
{
    size_t total;
    size_t b;

    total = rule->head.arity;
    for (b = 0; b < rule->body_count; ++b)
    {
        total = total + rule->body[b].arity;
    }
    return total;
}

static size_t max_arity_of(const dl89_rule *rule)
{
    size_t max;
    size_t b;

    max = rule->head.arity;
    for (b = 0; b < rule->body_count; ++b)
    {
        if (rule->body[b].arity > max)
        {
            max = rule->body[b].arity;
        }
    }
    return max;
}

static int seed_zero_body(re_ctx *ctx, const dl89_rule *rules,
                          size_t rule_count, re_queue *queue)
{
    size_t r;
    re_env env;

    for (r = 0; r < rule_count; ++r)
    {
        if (rules[r].body_count != 0)
        {
            continue;
        }
        env_init(&env, NULL, 0);
        if (emit_head(ctx, &rules[r], &env, queue) == 0)
        {
            return 0;
        }
    }
    return 1;
}

static int seed_relation(re_ctx *ctx, dl89_rel relation, size_t arity,
                         re_queue *queue)
{
    dl89_scan *scan;
    dl89_const tuple[8];
    size_t p;
    int found;
    int rc;

    for (p = 0; p < arity; ++p)
    {
        ctx->values[p] = 0;
        ctx->bound[p] = 0;
    }
    scan = NULL;
    rc = ctx->store.ops->scan_open(ctx->store.ctx, relation, arity, ctx->values,
                                   ctx->bound, &scan);
    if (rc != 0)
    {
        return 0;
    }
    for (;;)
    {
        found = 0;
        rc = ctx->store.ops->scan_next(ctx->store.ctx, scan, tuple, &found);
        if (rc != 0)
        {
            ctx->store.ops->scan_close(ctx->store.ctx, scan);
            return 0;
        }
        if (found == 0)
        {
            break;
        }
        if (queue_push(queue, relation, arity, tuple) == 0)
        {
            ctx->store.ops->scan_close(ctx->store.ctx, scan);
            return 0;
        }
    }
    ctx->store.ops->scan_close(ctx->store.ctx, scan);
    return 1;
}

static int seen_contains(const dl89_rel *rels, const size_t *arities,
                         size_t count, dl89_rel relation, size_t arity)
{
    size_t i;

    for (i = 0; i < count; ++i)
    {
        if (rels[i] == relation)
        {
            if (arities[i] == arity)
            {
                return 1;
            }
        }
    }
    return 0;
}

static int seed_initial(re_ctx *ctx, const dl89_rule *rules, size_t rule_count,
                        re_queue *queue)
{
    dl89_rel rels[64];
    size_t arities[64];
    size_t seen;
    size_t r;
    size_t b;

    seen = 0;
    for (r = 0; r < rule_count; ++r)
    {
        for (b = 0; b <= rules[r].body_count; ++b)
        {
            const dl89_atom *atom;

            if (b == 0)
            {
                atom = &rules[r].head;
            }
            else
            {
                atom = &rules[r].body[b - 1];
            }
            if (seen_contains(rels, arities, seen, atom->relation,
                              atom->arity) != 0)
            {
                continue;
            }
            if (seen < 64)
            {
                rels[seen] = atom->relation;
                arities[seen] = atom->arity;
                seen = seen + 1;
            }
            if (seed_relation(ctx, atom->relation, atom->arity, queue) == 0)
            {
                return 0;
            }
        }
    }
    return 1;
}

int ref_eval_run(dl89_store store, const dl89_rule *rules, size_t rule_count)
{
    re_ctx ctx;
    re_queue queue;
    re_binding *storage;
    size_t storage_cap;
    size_t max_arity;
    size_t r;
    re_item fact;
    int ok;

    max_arity = 0;
    storage_cap = 0;
    for (r = 0; r < rule_count; ++r)
    {
        if (total_terms(&rules[r]) > storage_cap)
        {
            storage_cap = total_terms(&rules[r]);
        }
        if (max_arity_of(&rules[r]) > max_arity)
        {
            max_arity = max_arity_of(&rules[r]);
        }
    }
    if (max_arity == 0)
    {
        max_arity = 1;
    }
    if (storage_cap == 0)
    {
        storage_cap = 1;
    }
    ctx.store = store;
    ctx.max_arity = max_arity;
    ctx.values = malloc(max_arity * sizeof(*ctx.values));
    ctx.bound = malloc(max_arity * sizeof(*ctx.bound));
    storage = malloc(storage_cap * sizeof(*storage));
    queue.items = NULL;
    queue.count = 0;
    queue.cap = 0;
    queue.pos = 0;
    queue.failed = 0;
    if (ctx.values == NULL)
    {
        free(ctx.bound);
        free(storage);
        return 1;
    }
    if (ctx.bound == NULL)
    {
        free(ctx.values);
        free(storage);
        return 1;
    }
    if (storage == NULL)
    {
        free(ctx.values);
        free(ctx.bound);
        return 1;
    }
    ok = seed_initial(&ctx, rules, rule_count, &queue);
    if (ok != 0)
    {
        ok = seed_zero_body(&ctx, rules, rule_count, &queue);
    }
    while (ok != 0)
    {
        if (queue.pos >= queue.count)
        {
            break;
        }
        queue_pop(&queue, &fact);
        ok = try_fact(&ctx, rules, rule_count, &fact, storage, storage_cap,
                      &queue);
        if (queue.failed != 0)
        {
            ok = 0;
        }
    }
    queue_clear(&queue);
    free(ctx.values);
    free(ctx.bound);
    free(storage);
    if (ok == 0)
    {
        return 1;
    }
    return 0;
}

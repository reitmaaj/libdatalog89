/* datalog89_join.c - plan-driven body joins, unification, head emission.
 *
 * A variant is a fixed sequence of steps. Each step either scans the store
 * or iterates the previous round's delta, unifies candidate tuples against
 * the step's static binding pattern, and recurses into the next step. The
 * binding pattern is precomputed at plan build time, so the evaluator never
 * re-derives which positions are bound. Written to the green
 * worker/controller discipline. */

#include "datalog89_priv.h"
#include "datalog89_priv_plan.h"

static int eval_step(datalog89_eval *eval, datalog89_priv_crule *rule,
                     const struct datalog89_priv_plan *plan,
                     const struct datalog89_priv_variant *variant, size_t index,
                     datalog89_priv_delta_table *sink,
                     const datalog89_priv_delta *delta);

/* --- pure value workers ------------------------------------------------- */

GREEN_PURE
static datalog89_const head_value(const datalog89_priv_crule *rule,
                                  const datalog89_priv_cterm *term)
{
    if (term->kind == DATALOG89_TERM_CONST)
    {
        return term->constant;
    }
    return rule->env[term->slot];
}

GREEN_PURE
static const datalog89_const *delta_tuple_at(const datalog89_priv_delta *delta,
                                             size_t arity, size_t index)
{
    return delta->tuples + index * arity;
}

/* --- scan pattern filling ----------------------------------------------- */

static void set_scan_value(datalog89_priv_crule *rule, size_t p,
                           datalog89_const value)
{
    rule->values[p] = value;
    rule->bvalues[p] = 1;
}

static void set_scan_free(datalog89_priv_crule *rule, size_t p)
{
    rule->values[p] = 0;
    rule->bvalues[p] = 0;
}

static void fill_one(datalog89_priv_crule *rule,
                     const struct datalog89_priv_bind *col, size_t p)
{
    unsigned char kind;

    kind = col->kind;
    if (kind == (unsigned char)DATALOG89_PRIV_BIND_CONST)
    {
        set_scan_value(rule, p, col->constant);
        return;
    }
    if (kind == (unsigned char)DATALOG89_PRIV_BIND_SLOT)
    {
        set_scan_value(rule, p, rule->env[col->slot]);
        return;
    }
    set_scan_free(rule, p);
}

static void fill_values(datalog89_priv_crule *rule,
                        const struct datalog89_priv_bind *cols, size_t arity)
{
    size_t p;

    for (p = 0; p < arity; ++p)
    {
        fill_one(rule, &cols[p], p);
    }
}

/* --- environment rollback ---------------------------------------------- */

static void unbind_one(datalog89_priv_crule *rule, size_t slot)
{
    rule->bound[slot] = 0;
}

static void unbind(datalog89_priv_crule *rule, size_t *log, size_t count)
{
    size_t i;

    for (i = 0; i < count; ++i)
    {
        unbind_one(rule, log[i]);
    }
}

static void clear_bound(datalog89_priv_crule *rule)
{
    size_t i;

    for (i = 0; i < rule->var_count; ++i)
    {
        unbind_one(rule, i);
    }
}

/* --- pattern unification ------------------------------------------------ */

GREEN_PURE
static int kind_is_compare(unsigned char kind)
{
    if (kind == (unsigned char)DATALOG89_PRIV_BIND_SLOT)
    {
        return 1;
    }
    if (kind == (unsigned char)DATALOG89_PRIV_BIND_EQ)
    {
        return 1;
    }
    return 0;
}

GREEN_PURE
static int env_equals(const datalog89_priv_crule *rule, size_t slot,
                      datalog89_const value)
{
    if (rule->env[slot] == value)
    {
        return 1;
    }
    return 0;
}

static void bind_slot(datalog89_priv_crule *rule, size_t slot,
                      datalog89_const value, size_t *log, size_t *n)
{
    rule->env[slot] = value;
    rule->bound[slot] = 1;
    log[*n] = slot;
    *n = *n + 1;
}

static int unify_col(datalog89_priv_crule *rule,
                     const struct datalog89_priv_bind *col,
                     datalog89_const value, size_t *log, size_t *n)
{
    unsigned char kind;

    kind = col->kind;
    if (kind == (unsigned char)DATALOG89_PRIV_BIND_CONST)
    {
        if (value == col->constant)
        {
            return 1;
        }
        return 0;
    }
    if (kind_is_compare(kind) != 0)
    {
        if (env_equals(rule, col->slot, value) != 0)
        {
            return 1;
        }
        return 0;
    }
    bind_slot(rule, col->slot, value, log, n);
    return 1;
}

static void unify_cols(datalog89_priv_crule *rule,
                       const struct datalog89_priv_bind *cols, size_t arity,
                       const datalog89_const *tuple, size_t *log,
                       size_t *bound_count, int *matched)
{
    size_t p;
    size_t n;
    int ok;

    n = 0;
    for (p = 0; p < arity; ++p)
    {
        ok = unify_col(rule, &cols[p], tuple[p], log, &n);
        if (ok == 0)
        {
            break;
        }
    }
    *bound_count = n;
    *matched = 0;
    if (p == arity)
    {
        *matched = 1;
    }
}

/* --- head emission ------------------------------------------------------ */

static int emit_head(datalog89_eval *eval, datalog89_priv_crule *rule,
                     datalog89_priv_delta_table *sink)
{
    size_t p;
    int inserted;
    int rc;
    int st;

    for (p = 0; p < rule->head.arity; ++p)
    {
        rule->tuple[p] = head_value(rule, &rule->head.terms[p]);
    }
    inserted = 0;
    rc = eval->store.ops->insert(eval->store.ctx, rule->head.relation,
                                 rule->head.arity, rule->tuple, &inserted);
    if (rc != 0)
    {
        return DATALOG89_ESTORE;
    }
    if (inserted == 0)
    {
        return DATALOG89_OK;
    }
    if (sink == NULL)
    {
        return DATALOG89_OK;
    }
    st = datalog89_priv_delta_record(sink, rule->head.relation,
                                     rule->head.arity, rule->tuple);
    return st;
}

/* --- store scan --------------------------------------------------------- */

static int open_scan(datalog89_eval *eval,
                     const struct datalog89_priv_step *step,
                     datalog89_priv_crule *rule, datalog89_scan **scan)
{
    int rc;

    rc =
        eval->store.ops->scan_open(eval->store.ctx, step->relation, step->arity,
                                   rule->values, rule->bvalues, scan);
    if (rc != 0)
    {
        return DATALOG89_ESTORE;
    }
    if (*scan == NULL)
    {
        return DATALOG89_ESTORE;
    }
    return DATALOG89_OK;
}

static int scan_one(datalog89_eval *eval, datalog89_priv_crule *rule,
                    const struct datalog89_priv_plan *plan,
                    const struct datalog89_priv_variant *variant, size_t index,
                    const struct datalog89_priv_bind *cols, size_t arity,
                    datalog89_scan *scan, datalog89_priv_delta_table *sink,
                    const datalog89_priv_delta *delta, int *done)
{
    size_t *log;
    size_t bound_count;
    int matched;
    int found;
    int rc;
    int st;

    *done = 0;
    found = 0;
    rc = eval->store.ops->scan_next(eval->store.ctx, scan, rule->tuple, &found);
    if (rc != 0)
    {
        return DATALOG89_ESTORE;
    }
    if (found == 0)
    {
        *done = 1;
        return DATALOG89_OK;
    }
    bound_count = 0;
    matched = 0;
    log = rule->bound_log + index * rule->tuple_cap;
    unify_cols(rule, cols, arity, rule->tuple, log, &bound_count, &matched);
    st = DATALOG89_OK;
    if (matched != 0)
    {
        st = eval_step(eval, rule, plan, variant, index + 1, sink, delta);
    }
    unbind(rule, log, bound_count);
    return st;
}

static int scan_step(datalog89_eval *eval, datalog89_priv_crule *rule,
                     const struct datalog89_priv_plan *plan,
                     const struct datalog89_priv_variant *variant, size_t index,
                     const struct datalog89_priv_step *step,
                     const struct datalog89_priv_bind *cols,
                     datalog89_priv_delta_table *sink,
                     const datalog89_priv_delta *delta)
{
    datalog89_scan *scan;
    int done;
    int st;

    fill_values(rule, cols, step->arity);
    scan = NULL;
    st = open_scan(eval, step, rule, &scan);
    if (st != DATALOG89_OK)
    {
        return st;
    }
    done = 0;
    while (done == 0)
    {
        st = scan_one(eval, rule, plan, variant, index, cols, step->arity, scan,
                      sink, delta, &done);
        if (st != DATALOG89_OK)
        {
            break;
        }
    }
    eval->store.ops->scan_close(eval->store.ctx, scan);
    return st;
}

/* --- delta iteration ---------------------------------------------------- */

static int delta_one(datalog89_eval *eval, datalog89_priv_crule *rule,
                     const struct datalog89_priv_plan *plan,
                     const struct datalog89_priv_variant *variant, size_t index,
                     const struct datalog89_priv_bind *cols, size_t arity,
                     datalog89_priv_delta_table *sink,
                     const datalog89_priv_delta *delta,
                     const datalog89_const *tuple)
{
    size_t *log;
    size_t bound_count;
    int matched;
    int st;

    bound_count = 0;
    matched = 0;
    log = rule->bound_log + index * rule->tuple_cap;
    unify_cols(rule, cols, arity, tuple, log, &bound_count, &matched);
    st = DATALOG89_OK;
    if (matched != 0)
    {
        st = eval_step(eval, rule, plan, variant, index + 1, sink, delta);
    }
    unbind(rule, log, bound_count);
    return st;
}

static int delta_step(datalog89_eval *eval, datalog89_priv_crule *rule,
                      const struct datalog89_priv_plan *plan,
                      const struct datalog89_priv_variant *variant,
                      size_t index, const struct datalog89_priv_bind *cols,
                      size_t arity, datalog89_priv_delta_table *sink,
                      const datalog89_priv_delta *delta)
{
    size_t i;
    int st;

    for (i = 0; i < delta->count; ++i)
    {
        st = delta_one(eval, rule, plan, variant, index, cols, arity, sink,
                       delta, delta_tuple_at(delta, arity, i));
        if (st != DATALOG89_OK)
        {
            return st;
        }
    }
    return DATALOG89_OK;
}

/* --- step recursion ----------------------------------------------------- */

static int eval_step(datalog89_eval *eval, datalog89_priv_crule *rule,
                     const struct datalog89_priv_plan *plan,
                     const struct datalog89_priv_variant *variant, size_t index,
                     datalog89_priv_delta_table *sink,
                     const datalog89_priv_delta *delta)
{
    const struct datalog89_priv_step *step;
    const struct datalog89_priv_bind *cols;
    int st;

    if (index == variant->nsteps)
    {
        st = emit_head(eval, rule, sink);
        return st;
    }
    step = &plan->steps[variant->first_step + index];
    cols = &plan->cols[step->col_off];
    if (step->source == DATALOG89_PRIV_SRC_DELTA)
    {
        st = delta_step(eval, rule, plan, variant, index, cols, step->arity,
                        sink, delta);
        return st;
    }
    st = scan_step(eval, rule, plan, variant, index, step, cols, sink, delta);
    return st;
}

int datalog89_priv_join_variant(datalog89_eval *eval,
                                datalog89_priv_crule *rule,
                                const struct datalog89_priv_plan *plan,
                                const struct datalog89_priv_variant *variant,
                                datalog89_priv_delta_table *sink,
                                const datalog89_priv_delta *delta)
{
    int st;

    clear_bound(rule);
    st = eval_step(eval, rule, plan, variant, 0, sink, delta);
    return st;
}

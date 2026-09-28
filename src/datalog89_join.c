/* datalog89_join.c - body joins, unification, head instantiation, insertion.
 *
 * Body atoms are evaluated left to right with recursive backtracking. One
 * body position may instead iterate a delta (semi-naive rounds); the join is
 * otherwise identical. Written to the green worker/controller discipline. */

#include "datalog89_priv.h"

static int eval_body(datalog89_eval *eval, datalog89_priv_crule *rule,
                     size_t depth, datalog89_priv_delta_table *sink,
                     const datalog89_priv_delta *delta, size_t delta_pos);

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

/* --- binding construction ---------------------------------------------- */

static void bind_constant(datalog89_priv_crule *rule, datalog89_const constant,
                          size_t p)
{
    rule->values[p] = constant;
    rule->bvalues[p] = 1;
}

static void bind_bound(datalog89_priv_crule *rule, size_t slot, size_t p)
{
    rule->values[p] = rule->env[slot];
    rule->bvalues[p] = 1;
}

static void bind_free(datalog89_priv_crule *rule, size_t p)
{
    rule->values[p] = 0;
    rule->bvalues[p] = 0;
}

static void bind_position(datalog89_priv_crule *rule,
                          const datalog89_priv_catom *atom, size_t p)
{
    const datalog89_priv_cterm *term;
    size_t slot;

    term = &atom->terms[p];
    if (term->kind == DATALOG89_TERM_CONST)
    {
        bind_constant(rule, term->constant, p);
        return;
    }
    slot = term->slot;
    if (rule->bound[slot] != 0)
    {
        bind_bound(rule, slot, p);
        return;
    }
    bind_free(rule, p);
}

static void build_bindings(datalog89_priv_crule *rule,
                           const datalog89_priv_catom *atom)
{
    size_t p;

    for (p = 0; p < atom->arity; ++p)
    {
        bind_position(rule, atom, p);
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

/* --- unification -------------------------------------------------------- */

static int unify_position(datalog89_priv_crule *rule,
                          const datalog89_priv_cterm *term,
                          datalog89_const value, size_t *log, size_t *n)
{
    size_t slot;

    if (term->kind == DATALOG89_TERM_CONST)
    {
        if (value != term->constant)
        {
            return 0;
        }
        return 1;
    }
    slot = term->slot;
    if (rule->bound[slot] != 0)
    {
        if (rule->env[slot] != value)
        {
            return 0;
        }
        return 1;
    }
    rule->env[slot] = value;
    rule->bound[slot] = 1;
    log[*n] = slot;
    *n = *n + 1;
    return 1;
}

static void unify_tuple(datalog89_priv_crule *rule,
                        const datalog89_priv_catom *atom,
                        const datalog89_const *tuple, size_t *log,
                        size_t *bound_count, int *matched)
{
    size_t p;
    size_t n;
    int ok;

    n = 0;
    for (p = 0; p < atom->arity; ++p)
    {
        ok = unify_position(rule, &atom->terms[p], tuple[p], log, &n);
        if (ok == 0)
        {
            break;
        }
    }
    *bound_count = n;
    *matched = 0;
    if (p == atom->arity)
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

static int open_scan(datalog89_eval *eval, const datalog89_priv_crule *rule,
                     const datalog89_priv_catom *atom, datalog89_scan **scan)
{
    int rc;

    rc =
        eval->store.ops->scan_open(eval->store.ctx, atom->relation, atom->arity,
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
                    const datalog89_priv_catom *atom, size_t depth,
                    datalog89_scan *scan, datalog89_priv_delta_table *sink,
                    const datalog89_priv_delta *delta, size_t delta_pos,
                    int *done)
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
    log = rule->bound_log + depth * rule->tuple_cap;
    unify_tuple(rule, atom, rule->tuple, log, &bound_count, &matched);
    st = DATALOG89_OK;
    if (matched != 0)
    {
        st = eval_body(eval, rule, depth + 1, sink, delta, delta_pos);
    }
    unbind(rule, log, bound_count);
    return st;
}

static int scan_atom(datalog89_eval *eval, datalog89_priv_crule *rule,
                     const datalog89_priv_catom *atom, size_t depth,
                     datalog89_priv_delta_table *sink,
                     const datalog89_priv_delta *delta, size_t delta_pos)
{
    datalog89_scan *scan;
    int done;
    int st;

    build_bindings(rule, atom);
    scan = NULL;
    st = open_scan(eval, rule, atom, &scan);
    if (st != DATALOG89_OK)
    {
        return st;
    }
    done = 0;
    while (done == 0)
    {
        st = scan_one(eval, rule, atom, depth, scan, sink, delta, delta_pos,
                      &done);
        if (st != DATALOG89_OK)
        {
            break;
        }
    }
    eval->store.ops->scan_close(eval->store.ctx, scan);
    return st;
}

/* --- delta iteration ---------------------------------------------------- */

static int delta_step(datalog89_eval *eval, datalog89_priv_crule *rule,
                      const datalog89_priv_catom *atom, size_t depth,
                      datalog89_priv_delta_table *sink,
                      const datalog89_priv_delta *delta, size_t delta_pos,
                      const datalog89_const *tuple)
{
    size_t *log;
    size_t bound_count;
    int matched;
    int st;

    bound_count = 0;
    matched = 0;
    log = rule->bound_log + depth * rule->tuple_cap;
    unify_tuple(rule, atom, tuple, log, &bound_count, &matched);
    st = DATALOG89_OK;
    if (matched != 0)
    {
        st = eval_body(eval, rule, depth + 1, sink, delta, delta_pos);
    }
    unbind(rule, log, bound_count);
    return st;
}

static int delta_atom(datalog89_eval *eval, datalog89_priv_crule *rule,
                      const datalog89_priv_catom *atom, size_t depth,
                      datalog89_priv_delta_table *sink,
                      const datalog89_priv_delta *delta, size_t delta_pos)
{
    size_t i;
    int st;

    for (i = 0; i < delta->count; ++i)
    {
        st = delta_step(eval, rule, atom, depth, sink, delta, delta_pos,
                        delta_tuple_at(delta, atom->arity, i));
        if (st != DATALOG89_OK)
        {
            return st;
        }
    }
    return DATALOG89_OK;
}

/* --- body recursion ----------------------------------------------------- */

static int eval_body(datalog89_eval *eval, datalog89_priv_crule *rule,
                     size_t depth, datalog89_priv_delta_table *sink,
                     const datalog89_priv_delta *delta, size_t delta_pos)
{
    const datalog89_priv_catom *atom;
    int st;

    if (depth == rule->body_count)
    {
        st = emit_head(eval, rule, sink);
        return st;
    }
    atom = &rule->body[depth];
    if (delta != NULL)
    {
        if (depth == delta_pos)
        {
            st = delta_atom(eval, rule, atom, depth, sink, delta, delta_pos);
            return st;
        }
    }
    st = scan_atom(eval, rule, atom, depth, sink, delta, delta_pos);
    return st;
}
int datalog89_priv_join_rule(datalog89_eval *eval, datalog89_priv_crule *rule,
                             datalog89_priv_delta_table *sink,
                             const datalog89_priv_delta *delta,
                             size_t delta_pos)
{
    int st;

    clear_bound(rule);
    st = eval_body(eval, rule, 0, sink, delta, delta_pos);
    return st;
}

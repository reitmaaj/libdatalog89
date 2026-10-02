/* datalog89_join.c - plan-driven body joins, unification, head emission.
 *
 * A variant is walked with an explicit frame stack: one frame per step
 * level, each holding an open store scan or a delta cursor. A frame fetches
 * one candidate tuple at a time, unifies it against the step's static
 * binding pattern, and descends on success. The machine never recurses in
 * C, so body length is independent of the call stack. Written to the green
 * worker/controller discipline. */

#include "datalog89_priv.h"
#include "datalog89_priv_plan.h"

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

/* --- frame stack -------------------------------------------------------- */

static int frame_start(datalog89_eval *eval, datalog89_priv_crule *rule,
                       const struct datalog89_priv_plan *plan,
                       const struct datalog89_priv_variant *variant,
                       size_t level, struct datalog89_priv_frame *frame)
{
    const struct datalog89_priv_step *step;
    int st;

    frame->scan = NULL;
    frame->next_delta = 0;
    frame->bound_count = 0;
    frame->in_delta = 0;
    frame->has_tuple = 0;
    step = &plan->steps[variant->first_step + level];
    if (step->source == DATALOG89_PRIV_SRC_DELTA)
    {
        frame->in_delta = 1;
        return DATALOG89_OK;
    }
    fill_values(rule, &plan->cols[step->col_off], step->arity);
    st = open_scan(eval, step, rule, &frame->scan);
    return st;
}

static void frame_unbind_state(datalog89_priv_crule *rule, size_t level,
                               struct datalog89_priv_frame *frame)
{
    if (frame->has_tuple == 0)
    {
        return;
    }
    unbind(rule, rule->bound_log + level * rule->tuple_cap, frame->bound_count);
    frame->has_tuple = 0;
}

static void frame_delta_advance(struct datalog89_priv_frame *frame)
{
    frame->next_delta = frame->next_delta + 1;
}

static void frame_delta_take(const struct datalog89_priv_step *step,
                             struct datalog89_priv_frame *frame,
                             const datalog89_priv_delta *delta,
                             const datalog89_const **tuple, int *ready)
{
    if (frame->next_delta == delta->count)
    {
        return;
    }
    *tuple = delta_tuple_at(delta, step->arity, frame->next_delta);
    frame_delta_advance(frame);
    *ready = 1;
}

static int frame_fetch(datalog89_eval *eval, datalog89_priv_crule *rule,
                       const struct datalog89_priv_step *step,
                       struct datalog89_priv_frame *frame,
                       const datalog89_priv_delta *delta,
                       const datalog89_const **tuple, int *ready)
{
    int found;
    int rc;

    *ready = 0;
    if (frame->in_delta != 0)
    {
        frame_delta_take(step, frame, delta, tuple, ready);
        return DATALOG89_OK;
    }
    found = 0;
    rc = eval->store.ops->scan_next(eval->store.ctx, frame->scan, rule->tuple,
                                    &found);
    if (rc != 0)
    {
        return DATALOG89_ESTORE;
    }
    if (found == 0)
    {
        return DATALOG89_OK;
    }
    *tuple = rule->tuple;
    *ready = 1;
    return DATALOG89_OK;
}

static void frame_accept(struct datalog89_priv_frame *frame, size_t bound_count)
{
    frame->bound_count = bound_count;
    frame->has_tuple = 1;
}

/* Fetch one candidate tuple and unify it. done is set when the frame either
 * matched a tuple or ran out of tuples; it stays clear when a candidate
 * failed unification, so the caller keeps advancing. */
static int frame_take(datalog89_eval *eval, datalog89_priv_crule *rule,
                      const struct datalog89_priv_step *step,
                      const struct datalog89_priv_bind *cols,
                      struct datalog89_priv_frame *frame,
                      const datalog89_priv_delta *delta, size_t *log,
                      int *matched, int *done)
{
    const datalog89_const *tuple;
    size_t bound_count;
    int ready;
    int st;

    *matched = 0;
    *done = 1;
    tuple = NULL;
    st = frame_fetch(eval, rule, step, frame, delta, &tuple, &ready);
    if (st != DATALOG89_OK)
    {
        return st;
    }
    if (ready == 0)
    {
        return DATALOG89_OK;
    }
    bound_count = 0;
    unify_cols(rule, cols, step->arity, tuple, log, &bound_count, matched);
    if (*matched != 0)
    {
        frame_accept(frame, bound_count);
        return DATALOG89_OK;
    }
    unbind(rule, log, bound_count);
    *done = 0;
    return DATALOG89_OK;
}

static int frame_next(datalog89_eval *eval, datalog89_priv_crule *rule,
                      const struct datalog89_priv_plan *plan,
                      const struct datalog89_priv_variant *variant,
                      size_t level, struct datalog89_priv_frame *frame,
                      const datalog89_priv_delta *delta, int *matched)
{
    const struct datalog89_priv_step *step;
    const struct datalog89_priv_bind *cols;
    size_t *log;
    int done;
    int st;

    *matched = 0;
    step = &plan->steps[variant->first_step + level];
    cols = &plan->cols[step->col_off];
    log = rule->bound_log + level * rule->tuple_cap;
    frame_unbind_state(rule, level, frame);
    done = 0;
    while (done == 0)
    {
        st = frame_take(eval, rule, step, cols, frame, delta, log, matched,
                        &done);
        if (st != DATALOG89_OK)
        {
            return st;
        }
    }
    return DATALOG89_OK;
}

static void frame_finish(datalog89_eval *eval,
                         struct datalog89_priv_frame *frame)
{
    if (frame->scan == NULL)
    {
        return;
    }
    eval->store.ops->scan_close(eval->store.ctx, frame->scan);
    frame->scan = NULL;
}

static void frames_close(datalog89_eval *eval, datalog89_priv_crule *rule)
{
    size_t i;

    for (i = 0; i < rule->body_count; ++i)
    {
        frame_finish(eval, &rule->frames[i]);
    }
}

/* --- frame machine ------------------------------------------------------ */

struct join_machine
{
    const struct datalog89_priv_plan *plan;
    const struct datalog89_priv_variant *variant;
    const datalog89_priv_delta *delta;
    datalog89_priv_delta_table *sink;
    size_t level;
    int running;
    int st;
};

static void machine_emit(datalog89_eval *eval, datalog89_priv_crule *rule,
                         struct join_machine *m)
{
    m->st = emit_head(eval, rule, m->sink);
    if (m->st != DATALOG89_OK)
    {
        m->running = 0;
        return;
    }
    m->level = m->level - 1;
}

static void machine_ascend(datalog89_eval *eval, datalog89_priv_crule *rule,
                           struct join_machine *m)
{
    frame_finish(eval, &rule->frames[m->level]);
    if (m->level == 0)
    {
        m->running = 0;
        return;
    }
    m->level = m->level - 1;
}

static void machine_descend(datalog89_eval *eval, datalog89_priv_crule *rule,
                            struct join_machine *m)
{
    m->level = m->level + 1;
    if (m->level == m->variant->nsteps)
    {
        return;
    }
    m->st = frame_start(eval, rule, m->plan, m->variant, m->level,
                        &rule->frames[m->level]);
    if (m->st != DATALOG89_OK)
    {
        m->running = 0;
    }
}

static void machine_visit(datalog89_eval *eval, datalog89_priv_crule *rule,
                          struct join_machine *m)
{
    int matched;

    matched = 0;
    m->st = frame_next(eval, rule, m->plan, m->variant, m->level,
                       &rule->frames[m->level], m->delta, &matched);
    if (m->st != DATALOG89_OK)
    {
        m->running = 0;
        return;
    }
    if (matched != 0)
    {
        machine_descend(eval, rule, m);
        return;
    }
    machine_ascend(eval, rule, m);
}

static void machine_step(datalog89_eval *eval, datalog89_priv_crule *rule,
                         struct join_machine *m)
{
    if (m->level == m->variant->nsteps)
    {
        machine_emit(eval, rule, m);
        return;
    }
    machine_visit(eval, rule, m);
}

static int machine_run(datalog89_eval *eval, datalog89_priv_crule *rule,
                       const struct datalog89_priv_plan *plan,
                       const struct datalog89_priv_variant *variant,
                       datalog89_priv_delta_table *sink,
                       const datalog89_priv_delta *delta)
{
    struct join_machine m;
    int st;

    m.plan = plan;
    m.variant = variant;
    m.delta = delta;
    m.sink = sink;
    m.level = 0;
    m.running = 1;
    m.st = DATALOG89_OK;
    while (m.running != 0)
    {
        machine_step(eval, rule, &m);
    }
    st = m.st;
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
    if (variant->nsteps == 0)
    {
        st = emit_head(eval, rule, sink);
        return st;
    }
    st = frame_start(eval, rule, plan, variant, 0, &rule->frames[0]);
    if (st == DATALOG89_OK)
    {
        st = machine_run(eval, rule, plan, variant, sink, delta);
    }
    frames_close(eval, rule);
    return st;
}

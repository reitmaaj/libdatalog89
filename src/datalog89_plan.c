/* datalog89_plan.c - compile the immutable execution plan.
 *
 * The plan is built lazily at the start of a run and cached until a rule is
 * added. It expands every rule into body_count + 1 variants (one per
 * possible delta position plus the no-delta seed variant) and precomputes
 * each step's static binding pattern from the variables bound by earlier
 * steps. Written to the green worker/controller discipline. */

#include "datalog89_priv.h"
#include "datalog89_priv_plan.h"

/* --- pure structural helpers ------------------------------------------- */

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
static int idb_of(const datalog89_eval *eval, const datalog89_priv_crule *rule,
                  size_t delta_pos)
{
    if (delta_pos < rule->body_count)
    {
        if (relation_is_idb(eval, rule->body[delta_pos].relation) != 0)
        {
            return 1;
        }
    }
    return 0;
}

GREEN_PURE
static size_t add_cols(size_t total, const datalog89_priv_crule *rule,
                       size_t atom)
{
    return total + rule->body[atom].arity;
}

GREEN_PURE
static size_t sum(size_t a, size_t b)
{
    return a + b;
}

GREEN_PURE
static size_t atom_col_off(const datalog89_priv_crule *rule, size_t atom)
{
    size_t b;
    size_t off;

    off = 0;
    for (b = 0; b < atom; ++b)
    {
        off = add_cols(off, rule, b);
    }
    return off;
}

GREEN_PURE
static int source_of(size_t s, size_t delta_pos)
{
    if (s == delta_pos)
    {
        return DATALOG89_PRIV_SRC_DELTA;
    }
    return DATALOG89_PRIV_SRC_FULL;
}

GREEN_PURE
static const datalog89_priv_cterm *body_term(const datalog89_priv_crule *rule,
                                             size_t atom, size_t col)
{
    return &rule->body[atom].terms[col];
}

GREEN_PURE
static int slot_bound_prev_atom(const datalog89_priv_crule *rule, size_t atom,
                                size_t slot)
{
    size_t b;
    size_t p;
    const datalog89_priv_cterm *term;

    for (b = 0; b < atom; ++b)
    {
        for (p = 0; p < rule->body[b].arity; ++p)
        {
            term = body_term(rule, b, p);
            if (term->kind == DATALOG89_TERM_VAR)
            {
                if (term->slot == slot)
                {
                    return 1;
                }
            }
        }
    }
    return 0;
}

GREEN_PURE
static int slot_bound_same_atom(const datalog89_priv_crule *rule, size_t atom,
                                size_t col, size_t slot)
{
    size_t p;
    const datalog89_priv_cterm *term;

    for (p = 0; p < col; ++p)
    {
        term = body_term(rule, atom, p);
        if (term->kind == DATALOG89_TERM_VAR)
        {
            if (term->slot == slot)
            {
                return 1;
            }
        }
    }
    return 0;
}

GREEN_PURE
static int bind_kind_of(const datalog89_priv_crule *rule, size_t atom,
                        size_t col, const datalog89_priv_cterm *term)
{
    if (term->kind == DATALOG89_TERM_CONST)
    {
        return DATALOG89_PRIV_BIND_CONST;
    }
    if (slot_bound_prev_atom(rule, atom, term->slot) != 0)
    {
        return DATALOG89_PRIV_BIND_SLOT;
    }
    if (slot_bound_same_atom(rule, atom, col, term->slot) != 0)
    {
        return DATALOG89_PRIV_BIND_EQ;
    }
    return DATALOG89_PRIV_BIND_FREE;
}

/* --- size accounting ---------------------------------------------------- */

static int add_total(size_t *total, size_t add)
{
    if (*total > ((size_t)-1) - add)
    {
        return DATALOG89_ENOMEM;
    }
    *total = *total + add;
    return DATALOG89_OK;
}

static int mul_total(size_t a, size_t b, size_t *out)
{
    if (b != 0)
    {
        if (a > ((size_t)-1) / b)
        {
            return DATALOG89_ENOMEM;
        }
    }
    *out = a * b;
    return DATALOG89_OK;
}

static int rule_totals(const datalog89_priv_crule *rule, size_t *nv, size_t *ns,
                       size_t *nc)
{
    size_t k;
    size_t prod;
    size_t b;
    int st;

    k = rule->body_count;
    st = add_total(nv, k + 1);
    if (st != DATALOG89_OK)
    {
        return st;
    }
    st = mul_total(k, k + 1, &prod);
    if (st != DATALOG89_OK)
    {
        return st;
    }
    st = add_total(ns, prod);
    if (st != DATALOG89_OK)
    {
        return st;
    }
    for (b = 0; b < k; ++b)
    {
        st = add_total(nc, rule->body[b].arity);
        if (st != DATALOG89_OK)
        {
            return st;
        }
    }
    return DATALOG89_OK;
}

static int plan_totals(const datalog89_eval *eval, size_t *nv, size_t *ns,
                       size_t *nc)
{
    size_t r;
    int st;

    *nv = 0;
    *ns = 0;
    *nc = 0;
    for (r = 0; r < eval->rule_count; ++r)
    {
        st = rule_totals(&eval->rules[r], nv, ns, nc);
        if (st != DATALOG89_OK)
        {
            return st;
        }
    }
    return DATALOG89_OK;
}

/* --- storage ------------------------------------------------------------ */

static void plan_init(struct datalog89_priv_plan *plan)
{
    plan->variants = NULL;
    plan->nvariants = 0;
    plan->steps = NULL;
    plan->nsteps = 0;
    plan->cols = NULL;
    plan->ncols = 0;
}

void datalog89_priv_plan_free(struct datalog89_priv_plan *plan)
{
    datalog89_priv_mem_free(plan->variants);
    datalog89_priv_mem_free(plan->steps);
    datalog89_priv_mem_free(plan->cols);
    plan_init(plan);
}

void datalog89_priv_plan_invalidate(datalog89_eval *eval)
{
    if (eval->plan == NULL)
    {
        return;
    }
    datalog89_priv_plan_free(eval->plan);
    datalog89_priv_mem_free(eval->plan);
    eval->plan = NULL;
}

static void *alloc_count(size_t count, size_t size)
{
    void *mem;

    if (size != 0)
    {
        if (count > ((size_t)-1) / size)
        {
            return NULL;
        }
    }
    mem = datalog89_priv_mem_alloc(count * size);
    return mem;
}

static int plan_alloc(struct datalog89_priv_plan *plan, size_t nv, size_t ns,
                      size_t nc)
{
    plan->nvariants = nv;
    plan->nsteps = ns;
    plan->ncols = nc;
    if (nv != 0)
    {
        plan->variants = alloc_count(nv, sizeof(*plan->variants));
        if (plan->variants == NULL)
        {
            return DATALOG89_ENOMEM;
        }
    }
    if (ns != 0)
    {
        plan->steps = alloc_count(ns, sizeof(*plan->steps));
        if (plan->steps == NULL)
        {
            return DATALOG89_ENOMEM;
        }
    }
    if (nc != 0)
    {
        plan->cols = alloc_count(nc, sizeof(*plan->cols));
        if (plan->cols == NULL)
        {
            return DATALOG89_ENOMEM;
        }
    }
    return DATALOG89_OK;
}

/* --- pattern and step writers ------------------------------------------- */

static void bind_set_slot(struct datalog89_priv_bind *dst, size_t slot)
{
    dst->slot = slot;
}

static void bind_set_constant(struct datalog89_priv_bind *dst,
                              datalog89_const constant)
{
    dst->constant = constant;
}

static void bind_write(struct datalog89_priv_bind *dst,
                       const datalog89_priv_crule *rule, size_t atom,
                       size_t col)
{
    const datalog89_priv_cterm *term;

    term = body_term(rule, atom, col);
    dst->kind = (unsigned char)bind_kind_of(rule, atom, col, term);
    dst->slot = 0;
    dst->constant = 0;
    if (term->kind == DATALOG89_TERM_VAR)
    {
        bind_set_slot(dst, term->slot);
        return;
    }
    bind_set_constant(dst, term->constant);
}

static void cols_write(struct datalog89_priv_plan *plan, size_t off,
                       const datalog89_priv_crule *rule, size_t atom)
{
    size_t p;

    for (p = 0; p < rule->body[atom].arity; ++p)
    {
        bind_write(&plan->cols[off + p], rule, atom, p);
    }
}

static void step_write(struct datalog89_priv_plan *plan, size_t index,
                       const datalog89_priv_crule *rule, size_t atom,
                       size_t col_off, int source)
{
    plan->steps[index].relation = rule->body[atom].relation;
    plan->steps[index].arity = rule->body[atom].arity;
    plan->steps[index].col_off = col_off;
    plan->steps[index].source = source;
}

static void variant_steps(struct datalog89_priv_plan *plan, size_t step_off,
                          const datalog89_priv_crule *rule, size_t col_base,
                          size_t delta_pos)
{
    size_t s;

    for (s = 0; s < rule->body_count; ++s)
    {
        step_write(plan, step_off + s, rule, s,
                   col_base + atom_col_off(rule, s), source_of(s, delta_pos));
    }
}

static void variant_write(struct datalog89_priv_plan *plan, size_t index,
                          size_t rule_index, const datalog89_priv_crule *rule,
                          size_t first_step, size_t delta_pos, int idb)
{
    plan->variants[index].head = rule->head.relation;
    plan->variants[index].head_arity = rule->head.arity;
    plan->variants[index].rule_index = rule_index;
    plan->variants[index].first_step = first_step;
    plan->variants[index].nsteps = rule->body_count;
    plan->variants[index].delta_pos = delta_pos;
    plan->variants[index].idb_delta = idb;
}

static void variant_emit(const datalog89_eval *eval,
                         struct datalog89_priv_plan *plan, size_t rule_index,
                         size_t *variant_off, size_t *step_off, size_t col_base,
                         size_t delta_pos)
{
    const datalog89_priv_crule *rule;

    rule = &eval->rules[rule_index];
    variant_write(plan, *variant_off, rule_index, rule, *step_off, delta_pos,
                  idb_of(eval, rule, delta_pos));
    variant_steps(plan, *step_off, rule, col_base, delta_pos);
    *variant_off = *variant_off + 1;
    *step_off = *step_off + rule->body_count;
}

static void cols_step(struct datalog89_priv_plan *plan,
                      const datalog89_priv_crule *rule, size_t b,
                      size_t *col_off, size_t *rule_cols)
{
    cols_write(plan, *col_off + *rule_cols, rule, b);
    *rule_cols = add_cols(*rule_cols, rule, b);
}

static void build_rule(const datalog89_eval *eval,
                       struct datalog89_priv_plan *plan, size_t rule_index,
                       size_t *variant_off, size_t *step_off, size_t *col_off)
{
    const datalog89_priv_crule *rule;
    size_t k;
    size_t b;
    size_t p;
    size_t col_base;
    size_t rule_cols;

    rule = &eval->rules[rule_index];
    k = rule->body_count;
    col_base = *col_off;
    rule_cols = 0;
    for (b = 0; b < k; ++b)
    {
        cols_step(plan, rule, b, col_off, &rule_cols);
    }
    *col_off = sum(*col_off, rule_cols);
    for (p = 0; p < k + 1; ++p)
    {
        variant_emit(eval, plan, rule_index, variant_off, step_off, col_base,
                     p);
    }
}

/* --- build controller --------------------------------------------------- */

int datalog89_priv_plan_build(datalog89_eval *eval)
{
    struct datalog89_priv_plan plan;
    size_t nv;
    size_t ns;
    size_t nc;
    size_t variant_off;
    size_t step_off;
    size_t col_off;
    size_t r;
    int st;

    plan_init(&plan);
    st = plan_totals(eval, &nv, &ns, &nc);
    if (st != DATALOG89_OK)
    {
        return st;
    }
    st = plan_alloc(&plan, nv, ns, nc);
    if (st != DATALOG89_OK)
    {
        datalog89_priv_plan_free(&plan);
        return st;
    }
    variant_off = 0;
    step_off = 0;
    col_off = 0;
    for (r = 0; r < eval->rule_count; ++r)
    {
        build_rule(eval, &plan, r, &variant_off, &step_off, &col_off);
    }
    eval->plan = alloc_count(1, sizeof(*eval->plan));
    if (eval->plan == NULL)
    {
        datalog89_priv_plan_free(&plan);
        return DATALOG89_ENOMEM;
    }
    *eval->plan = plan;
    eval->plan_builds = eval->plan_builds + 1;
    return DATALOG89_OK;
}

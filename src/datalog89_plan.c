/* datalog89_plan.c - compile the immutable execution plan.
 *
 * The plan is built lazily at the start of a run and cached until a rule is
 * added. It first decomposes the predicate dependency graph into strongly
 * connected components (Tarjan, relation level; EDB relations belong to no
 * SCC) in scheduling order. Each rule then expands into variants: a seed
 * variant for every rule, plus one delta variant per body position that
 * references a relation of the rule's own recursive SCC. Every step's static
 * binding pattern is precomputed from the variables bound by earlier steps.
 * Written to the green worker/controller discipline. */

#include <string.h>

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

/* --- relation node table ------------------------------------------------ */

struct rel_tab
{
    datalog89_rel *rels;
    size_t count;
    size_t cap;
};

static void rel_tab_init(struct rel_tab *tab)
{
    tab->rels = NULL;
    tab->count = 0;
    tab->cap = 0;
}

static void rel_tab_free(struct rel_tab *tab)
{
    datalog89_priv_mem_free(tab->rels);
    rel_tab_init(tab);
}

GREEN_PURE
static size_t rel_tab_find(const struct rel_tab *tab, datalog89_rel rel)
{
    size_t i;

    for (i = 0; i < tab->count; ++i)
    {
        if (tab->rels[i] == rel)
        {
            return i;
        }
    }
    return tab->count;
}

static size_t rel_cap_next(size_t cap)
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

static int rel_tab_grow(struct rel_tab *tab)
{
    datalog89_rel *grown;
    size_t cap;

    cap = rel_cap_next(tab->cap);
    if (cap == 0)
    {
        return DATALOG89_ENOMEM;
    }
    grown = datalog89_priv_mem_realloc(tab->rels, cap * sizeof(*grown));
    if (grown == NULL)
    {
        return DATALOG89_ENOMEM;
    }
    tab->rels = grown;
    tab->cap = cap;
    return DATALOG89_OK;
}

static int rel_tab_add(struct rel_tab *tab, datalog89_rel rel)
{
    int st;

    if (rel_tab_find(tab, rel) != tab->count)
    {
        return DATALOG89_OK;
    }
    if (tab->count == tab->cap)
    {
        st = rel_tab_grow(tab);
        if (st != DATALOG89_OK)
        {
            return st;
        }
    }
    tab->rels[tab->count] = rel;
    tab->count = tab->count + 1;
    return DATALOG89_OK;
}

static int rels_collect(const datalog89_eval *eval, struct rel_tab *tab)
{
    size_t r;
    int st;

    for (r = 0; r < eval->rule_count; ++r)
    {
        st = rel_tab_add(tab, eval->rules[r].head.relation);
        if (st != DATALOG89_OK)
        {
            return st;
        }
    }
    return DATALOG89_OK;
}

/* --- adjacency matrix --------------------------------------------------- */

GREEN_PURE
static size_t adj_index(size_t n, size_t from, size_t to)
{
    return from * n + to;
}

GREEN_PURE
static int adj_has(const unsigned char *adj, size_t n, size_t from, size_t to)
{
    if (adj[adj_index(n, from, to)] != 0)
    {
        return 1;
    }
    return 0;
}

static void adj_set(unsigned char *adj, size_t n, size_t from, size_t to)
{
    adj[adj_index(n, from, to)] = 1;
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

static int adj_alloc(size_t n, unsigned char **out)
{
    unsigned char *adj;

    if (n == 0)
    {
        *out = NULL;
        return DATALOG89_OK;
    }
    if (n > ((size_t)-1) / n)
    {
        return DATALOG89_ENOMEM;
    }
    adj = alloc_count(n * n, 1);
    if (adj == NULL)
    {
        return DATALOG89_ENOMEM;
    }
    memset(adj, 0, n * n);
    *out = adj;
    return DATALOG89_OK;
}

static void edge_add(const datalog89_eval *eval, const struct rel_tab *tab,
                     unsigned char *adj, size_t n, size_t hi,
                     datalog89_rel body_rel)
{
    size_t bi;

    if (relation_is_idb(eval, body_rel) == 0)
    {
        return;
    }
    bi = rel_tab_find(tab, body_rel);
    adj_set(adj, n, hi, bi);
}

static void rule_edges(const datalog89_eval *eval, const struct rel_tab *tab,
                       unsigned char *adj, size_t n, size_t rule_index)
{
    const datalog89_priv_crule *rule;
    size_t hi;
    size_t b;

    rule = &eval->rules[rule_index];
    hi = rel_tab_find(tab, rule->head.relation);
    for (b = 0; b < rule->body_count; ++b)
    {
        edge_add(eval, tab, adj, n, hi, rule->body[b].relation);
    }
}

static int adj_build(const datalog89_eval *eval, const struct rel_tab *tab,
                     unsigned char **out)
{
    unsigned char *adj;
    size_t n;
    size_t r;
    int st;

    n = tab->count;
    st = adj_alloc(n, &adj);
    if (st != DATALOG89_OK)
    {
        return st;
    }
    for (r = 0; r < eval->rule_count; ++r)
    {
        rule_edges(eval, tab, adj, n, r);
    }
    *out = adj;
    return DATALOG89_OK;
}

/* --- Tarjan strongly connected components ------------------------------- */

struct tarjan_state
{
    const unsigned char *adj;
    size_t n;
    size_t *index;
    size_t *lowlink;
    unsigned char *onstack;
    size_t *stack;
    size_t stack_count;
    size_t counter;
    size_t *comp;
    unsigned char *comp_recursive;
    size_t ncomp;
    size_t last_popped;
};

static void tarjan_connect(struct tarjan_state *st, size_t v);

static void tarjan_push(struct tarjan_state *st, size_t v)
{
    st->stack[st->stack_count] = v;
    st->stack_count = st->stack_count + 1;
}

static size_t tarjan_pop(struct tarjan_state *st)
{
    size_t v;

    st->stack_count = st->stack_count - 1;
    v = st->stack[st->stack_count];
    return v;
}

static void tarjan_discover(struct tarjan_state *st, size_t v)
{
    st->index[v] = st->counter;
    st->lowlink[v] = st->counter;
    st->counter = st->counter + 1;
    tarjan_push(st, v);
    st->onstack[v] = 1;
}

GREEN_PURE
static size_t low_min(size_t a, size_t b)
{
    if (a < b)
    {
        return a;
    }
    return b;
}

static void tarjan_recurse(struct tarjan_state *st, size_t v, size_t w)
{
    tarjan_connect(st, w);
    st->lowlink[v] = low_min(st->lowlink[v], st->lowlink[w]);
}

static void tarjan_edge(struct tarjan_state *st, size_t v, size_t w)
{
    if (adj_has(st->adj, st->n, v, w) == 0)
    {
        return;
    }
    if (st->index[w] == DATALOG89_PRIV_NO_INDEX)
    {
        tarjan_recurse(st, v, w);
        return;
    }
    if (st->onstack[w] != 0)
    {
        st->lowlink[v] = low_min(st->lowlink[v], st->index[w]);
    }
}

static size_t comp_pop(struct tarjan_state *st, size_t comp, size_t members)
{
    size_t w;

    w = tarjan_pop(st);
    st->onstack[w] = 0;
    st->comp[w] = comp;
    st->last_popped = w;
    return members + 1;
}

GREEN_PURE
static int comp_done(const struct tarjan_state *st, size_t v)
{
    if (st->last_popped == v)
    {
        return 1;
    }
    return 0;
}

GREEN_PURE
static unsigned char comp_is_recursive(const struct tarjan_state *st, size_t v,
                                       size_t members)
{
    if (members > 1)
    {
        return 1;
    }
    if (adj_has(st->adj, st->n, v, v) != 0)
    {
        return 1;
    }
    return 0;
}

static void close_component(struct tarjan_state *st, size_t v)
{
    size_t members;

    if (st->lowlink[v] != st->index[v])
    {
        return;
    }
    members = 0;
    do
    {
        members = comp_pop(st, st->ncomp, members);
    } while (comp_done(st, v) == 0);
    st->comp_recursive[st->ncomp] = comp_is_recursive(st, v, members);
    st->ncomp = st->ncomp + 1;
}

static void tarjan_connect(struct tarjan_state *st, size_t v)
{
    size_t w;

    tarjan_discover(st, v);
    for (w = 0; w < st->n; ++w)
    {
        tarjan_edge(st, v, w);
    }
    close_component(st, v);
}

static void tarjan_node_init(struct tarjan_state *st, size_t v)
{
    st->index[v] = DATALOG89_PRIV_NO_INDEX;
    st->onstack[v] = 0;
}

static void tarjan_free(struct tarjan_state *st)
{
    datalog89_priv_mem_free(st->index);
    datalog89_priv_mem_free(st->lowlink);
    datalog89_priv_mem_free(st->onstack);
    datalog89_priv_mem_free(st->stack);
    datalog89_priv_mem_free(st->comp);
    datalog89_priv_mem_free(st->comp_recursive);
}

static void tarjan_finish(struct tarjan_state *st, size_t **out_comp,
                          unsigned char **out_rec, size_t *out_ncomp)
{
    *out_comp = st->comp;
    *out_rec = st->comp_recursive;
    *out_ncomp = st->ncomp;
    datalog89_priv_mem_free(st->index);
    datalog89_priv_mem_free(st->lowlink);
    datalog89_priv_mem_free(st->onstack);
    datalog89_priv_mem_free(st->stack);
}

static void tarjan_empty(size_t **out_comp, unsigned char **out_rec,
                         size_t *out_ncomp)
{
    *out_comp = NULL;
    *out_rec = NULL;
    *out_ncomp = 0;
}

/* Tarjan over the relation dependency graph. Components are emitted in
 * scheduling order: every body relation of a component's rule lies in an
 * already emitted component (or is EDB). */
static int tarjan_run(const struct rel_tab *tab, const unsigned char *adj,
                      size_t **out_comp, unsigned char **out_rec,
                      size_t *out_ncomp)
{
    struct tarjan_state st;
    size_t n;
    size_t v;
    int rc;

    n = tab->count;
    if (n == 0)
    {
        tarjan_empty(out_comp, out_rec, out_ncomp);
        return DATALOG89_OK;
    }
    st.adj = adj;
    st.n = n;
    st.index = NULL;
    st.lowlink = NULL;
    st.onstack = NULL;
    st.stack = NULL;
    st.comp = NULL;
    st.comp_recursive = NULL;
    st.stack_count = 0;
    st.counter = 0;
    st.ncomp = 0;
    st.last_popped = n;
    st.index = alloc_count(n, sizeof(*st.index));
    if (st.index != NULL)
    {
        st.lowlink = alloc_count(n, sizeof(*st.lowlink));
    }
    if (st.lowlink != NULL)
    {
        st.onstack = alloc_count(n, 1);
    }
    if (st.onstack != NULL)
    {
        st.stack = alloc_count(n, sizeof(*st.stack));
    }
    if (st.stack != NULL)
    {
        st.comp = alloc_count(n, sizeof(*st.comp));
    }
    if (st.comp != NULL)
    {
        st.comp_recursive = alloc_count(n, 1);
    }
    rc = DATALOG89_OK;
    if (st.comp_recursive == NULL)
    {
        rc = DATALOG89_ENOMEM;
    }
    if (rc == DATALOG89_OK)
    {
        for (v = 0; v < n; ++v)
        {
            tarjan_node_init(&st, v);
        }
    }
    if (rc == DATALOG89_OK)
    {
        for (v = 0; v < n; ++v)
        {
            if (st.index[v] != DATALOG89_PRIV_NO_INDEX)
            {
                continue;
            }
            tarjan_connect(&st, v);
        }
    }
    if (rc == DATALOG89_OK)
    {
        tarjan_finish(&st, out_comp, out_rec, out_ncomp);
        return DATALOG89_OK;
    }
    tarjan_free(&st);
    return rc;
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

/* --- variant expansion -------------------------------------------------- */

GREEN_PURE
static size_t head_scc(const struct rel_tab *tab, const size_t *comp,
                       const datalog89_priv_crule *rule)
{
    return comp[rel_tab_find(tab, rule->head.relation)];
}

GREEN_PURE
static int position_delta(const datalog89_eval *eval, const struct rel_tab *tab,
                          const size_t *comp, size_t rule_index, size_t pos,
                          int scc_recursive)
{
    const datalog89_priv_crule *rule;
    size_t hi;
    size_t bi;

    if (scc_recursive == 0)
    {
        return 0;
    }
    rule = &eval->rules[rule_index];
    if (pos == rule->body_count)
    {
        return 0;
    }
    if (relation_is_idb(eval, rule->body[pos].relation) == 0)
    {
        return 0;
    }
    hi = rel_tab_find(tab, rule->head.relation);
    bi = rel_tab_find(tab, rule->body[pos].relation);
    if (comp[hi] != comp[bi])
    {
        return 0;
    }
    return 1;
}

GREEN_PURE
static size_t add_one(size_t n)
{
    return n + 1;
}

GREEN_PURE
static int position_skip(const datalog89_eval *eval, const struct rel_tab *tab,
                         const size_t *comp, size_t rule_index, size_t pos,
                         int scc_recursive)
{
    if (pos == eval->rules[rule_index].body_count)
    {
        return 0;
    }
    if (position_delta(eval, tab, comp, rule_index, pos, scc_recursive) == 0)
    {
        return 1;
    }
    return 0;
}

static int rule_totals(const datalog89_eval *eval, const struct rel_tab *tab,
                       const size_t *comp, const unsigned char *comp_rec,
                       size_t r, size_t *nv, size_t *ns, size_t *nc)
{
    const datalog89_priv_crule *rule;
    size_t k;
    size_t nv_rule;
    size_t prod;
    size_t b;
    size_t scc;
    int rec;
    int st;

    rule = &eval->rules[r];
    k = rule->body_count;
    scc = head_scc(tab, comp, rule);
    rec = (int)comp_rec[scc];
    nv_rule = 1;
    for (b = 0; b < k; ++b)
    {
        if (position_delta(eval, tab, comp, r, b, rec) != 0)
        {
            nv_rule = add_one(nv_rule);
        }
    }
    st = add_total(nv, nv_rule);
    if (st != DATALOG89_OK)
    {
        return st;
    }
    st = mul_total(nv_rule, k, &prod);
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

static int plan_totals(const datalog89_eval *eval, const struct rel_tab *tab,
                       const size_t *comp, const unsigned char *comp_rec,
                       size_t *nv, size_t *ns, size_t *nc)
{
    size_t r;
    int st;

    *nv = 0;
    *ns = 0;
    *nc = 0;
    for (r = 0; r < eval->rule_count; ++r)
    {
        st = rule_totals(eval, tab, comp, comp_rec, r, nv, ns, nc);
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
    plan->sccs = NULL;
    plan->nsccs = 0;
}

void datalog89_priv_plan_free(struct datalog89_priv_plan *plan)
{
    datalog89_priv_mem_free(plan->variants);
    datalog89_priv_mem_free(plan->steps);
    datalog89_priv_mem_free(plan->cols);
    datalog89_priv_mem_free(plan->sccs);
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

static int plan_alloc(struct datalog89_priv_plan *plan, size_t nv, size_t ns,
                      size_t nc, size_t nsccs)
{
    plan->nvariants = nv;
    plan->nsteps = ns;
    plan->ncols = nc;
    plan->nsccs = nsccs;
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
    if (nsccs != 0)
    {
        plan->sccs = alloc_count(nsccs, sizeof(*plan->sccs));
        if (plan->sccs == NULL)
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
                          size_t first_step, size_t delta_pos)
{
    plan->variants[index].head = rule->head.relation;
    plan->variants[index].head_arity = rule->head.arity;
    plan->variants[index].rule_index = rule_index;
    plan->variants[index].first_step = first_step;
    plan->variants[index].nsteps = rule->body_count;
    plan->variants[index].delta_pos = delta_pos;
}

static void variant_emit(const datalog89_eval *eval,
                         struct datalog89_priv_plan *plan, size_t rule_index,
                         size_t *variant_off, size_t *step_off, size_t col_base,
                         size_t delta_pos)
{
    const datalog89_priv_crule *rule;

    rule = &eval->rules[rule_index];
    variant_write(plan, *variant_off, rule_index, rule, *step_off, delta_pos);
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
                       struct datalog89_priv_plan *plan,
                       const struct rel_tab *tab, const size_t *comp,
                       const unsigned char *comp_rec, size_t rule_index,
                       size_t *variant_off, size_t *step_off, size_t *col_off)
{
    const datalog89_priv_crule *rule;
    size_t k;
    size_t b;
    size_t p;
    size_t col_base;
    size_t rule_cols;
    size_t scc;
    int rec;

    rule = &eval->rules[rule_index];
    k = rule->body_count;
    scc = head_scc(tab, comp, rule);
    rec = (int)comp_rec[scc];
    col_base = *col_off;
    rule_cols = 0;
    for (b = 0; b < k; ++b)
    {
        cols_step(plan, rule, b, col_off, &rule_cols);
    }
    *col_off = sum(*col_off, rule_cols);
    for (p = 0; p < k + 1; ++p)
    {
        if (position_skip(eval, tab, comp, rule_index, p, rec) != 0)
        {
            continue;
        }
        variant_emit(eval, plan, rule_index, variant_off, step_off, col_base,
                     p);
    }
}

GREEN_PURE
static size_t plan_scc_count(const struct datalog89_priv_plan *plan,
                             size_t scc_index, size_t variant_off)
{
    return variant_off - plan->sccs[scc_index].first_variant;
}

static void build_scc(const datalog89_eval *eval,
                      struct datalog89_priv_plan *plan,
                      const struct rel_tab *tab, const size_t *comp,
                      const unsigned char *comp_rec, const size_t *rule_scc,
                      size_t scc_index, size_t *variant_off, size_t *step_off,
                      size_t *col_off)
{
    size_t r;

    plan->sccs[scc_index].first_variant = *variant_off;
    plan->sccs[scc_index].recursive = comp_rec[scc_index];
    for (r = 0; r < eval->rule_count; ++r)
    {
        if (rule_scc[r] != scc_index)
        {
            continue;
        }
        build_rule(eval, plan, tab, comp, comp_rec, r, variant_off, step_off,
                   col_off);
    }
    plan->sccs[scc_index].nvariants =
        plan_scc_count(plan, scc_index, *variant_off);
}

/* --- build controller --------------------------------------------------- */

static int rule_scc_build(const datalog89_eval *eval, const struct rel_tab *tab,
                          const size_t *comp, size_t **out)
{
    size_t *map;
    size_t r;

    if (eval->rule_count == 0)
    {
        *out = NULL;
        return DATALOG89_OK;
    }
    map = alloc_count(eval->rule_count, sizeof(*map));
    if (map == NULL)
    {
        return DATALOG89_ENOMEM;
    }
    for (r = 0; r < eval->rule_count; ++r)
    {
        map[r] = head_scc(tab, comp, &eval->rules[r]);
    }
    *out = map;
    return DATALOG89_OK;
}

static void build_temps_free(struct rel_tab *tab, unsigned char *adj,
                             size_t *comp, unsigned char *comp_rec,
                             size_t *rule_scc)
{
    rel_tab_free(tab);
    datalog89_priv_mem_free(adj);
    datalog89_priv_mem_free(comp);
    datalog89_priv_mem_free(comp_rec);
    datalog89_priv_mem_free(rule_scc);
}

static void plan_layout(const datalog89_eval *eval,
                        struct datalog89_priv_plan *plan,
                        const struct rel_tab *tab, const size_t *comp,
                        const unsigned char *comp_rec, const size_t *rule_scc,
                        size_t ncomp)
{
    size_t variant_off;
    size_t step_off;
    size_t col_off;
    size_t s;

    variant_off = 0;
    step_off = 0;
    col_off = 0;
    for (s = 0; s < ncomp; ++s)
    {
        build_scc(eval, plan, tab, comp, comp_rec, rule_scc, s, &variant_off,
                  &step_off, &col_off);
    }
}

int datalog89_priv_plan_build(datalog89_eval *eval)
{
    struct rel_tab tab;
    unsigned char *adj;
    size_t *comp;
    unsigned char *comp_rec;
    size_t ncomp;
    size_t *rule_scc;
    struct datalog89_priv_plan plan;
    size_t nv;
    size_t ns;
    size_t nc;
    int st;

    plan_init(&plan);
    rel_tab_init(&tab);
    adj = NULL;
    comp = NULL;
    comp_rec = NULL;
    ncomp = 0;
    rule_scc = NULL;
    st = rels_collect(eval, &tab);
    if (st == DATALOG89_OK)
    {
        st = adj_build(eval, &tab, &adj);
    }
    if (st == DATALOG89_OK)
    {
        st = tarjan_run(&tab, adj, &comp, &comp_rec, &ncomp);
    }
    if (st == DATALOG89_OK)
    {
        st = rule_scc_build(eval, &tab, comp, &rule_scc);
    }
    if (st == DATALOG89_OK)
    {
        st = plan_totals(eval, &tab, comp, comp_rec, &nv, &ns, &nc);
    }
    if (st == DATALOG89_OK)
    {
        st = plan_alloc(&plan, nv, ns, nc, ncomp);
    }
    if (st == DATALOG89_OK)
    {
        plan_layout(eval, &plan, &tab, comp, comp_rec, rule_scc, ncomp);
    }
    build_temps_free(&tab, adj, comp, comp_rec, rule_scc);
    if (st != DATALOG89_OK)
    {
        datalog89_priv_plan_free(&plan);
        return st;
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

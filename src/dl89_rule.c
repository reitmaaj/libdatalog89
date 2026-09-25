/* dl89_rule.c - rule validation, deep copy/compilation, atomic install.
 *
 * Written to the green worker/controller discipline: nested control bodies
 * delegate to workers; pure helpers carry GREEN_PURE so they may appear in
 * expression position. */

#include "dl89_priv.h"

/* --- pure structural helpers ------------------------------------------- */

GREEN_PURE
static const dl89_atom *atom_of(const dl89_rule *rule, size_t index)
{
    if (index == 0)
    {
        return &rule->head;
    }
    return &rule->body[index - 1];
}

GREEN_PURE
static size_t atom_count_of(const dl89_rule *rule)
{
    return rule->body_count + 1;
}

GREEN_PURE
static size_t body_arity(const dl89_rule *rule, size_t b)
{
    return rule->body[b].arity;
}

GREEN_PURE
static const dl89_atom *rule_body_atom(const dl89_rule *rule, size_t b)
{
    return &rule->body[b];
}

GREEN_PURE
static const dl89_term *head_term(const dl89_rule *rule, size_t p)
{
    return &rule->head.terms[p];
}

GREEN_PURE
static const dl89_term *term_at(const dl89_atom *atom, size_t p)
{
    return &atom->terms[p];
}

GREEN_PURE
static int term_is_var(const dl89_term *term)
{
    if (term->kind == DL89_TERM_VAR)
    {
        return 1;
    }
    return 0;
}

GREEN_PURE
static int term_kind_valid(const dl89_term *term)
{
    if (term->kind == DL89_TERM_CONST)
    {
        return 1;
    }
    return term_is_var(term);
}

GREEN_PURE
static int pair_conflict(const dl89_atom *a, const dl89_atom *b)
{
    if (a->relation != b->relation)
    {
        return 0;
    }
    if (a->arity != b->arity)
    {
        return 1;
    }
    return 0;
}

GREEN_PURE
static int registry_conflict(const dl89_eval *eval, const dl89_atom *atom)
{
    const dl89_priv_arity *entry;

    entry = dl89_priv_registry_find(eval, atom->relation);
    if (entry == NULL)
    {
        return 0;
    }
    if (entry->arity != atom->arity)
    {
        return 1;
    }
    return 0;
}

GREEN_PURE
static size_t var_slot(const dl89_var *ids, size_t count, dl89_var var)
{
    size_t i;

    for (i = 0; i < count; ++i)
    {
        if (ids[i] == var)
        {
            return i;
        }
    }
    return DL89_PRIV_NO_SLOT;
}

/* --- pure validation --------------------------------------------------- */

GREEN_PURE
static int check_atom_terms(const dl89_atom *atom)
{
    size_t p;

    if (atom->arity > 0)
    {
        if (atom->terms == NULL)
        {
            return DL89_EINVAL;
        }
    }
    for (p = 0; p < atom->arity; ++p)
    {
        if (term_kind_valid(&atom->terms[p]) == 0)
        {
            return DL89_EPROGRAM;
        }
    }
    return DL89_OK;
}

GREEN_PURE
static int check_rule_arities(const dl89_rule *rule)
{
    size_t count;
    size_t i;
    size_t j;
    const dl89_atom *a;

    count = atom_count_of(rule);
    for (i = 0; i < count; ++i)
    {
        a = atom_of(rule, i);
        for (j = i + 1; j < count; ++j)
        {
            if (pair_conflict(a, atom_of(rule, j)) != 0)
            {
                return DL89_EPROGRAM;
            }
        }
    }
    return DL89_OK;
}

GREEN_PURE
static int check_rule_registry(const dl89_eval *eval, const dl89_rule *rule)
{
    size_t count;
    size_t i;

    count = atom_count_of(rule);
    for (i = 0; i < count; ++i)
    {
        if (registry_conflict(eval, atom_of(rule, i)) != 0)
        {
            return DL89_EPROGRAM;
        }
    }
    return DL89_OK;
}

GREEN_PURE
static int var_in_body(const dl89_rule *rule, dl89_var var)
{
    size_t b;
    size_t p;
    const dl89_atom *atom;
    const dl89_term *term;

    for (b = 0; b < rule->body_count; ++b)
    {
        atom = rule_body_atom(rule, b);
        for (p = 0; p < atom->arity; ++p)
        {
            term = term_at(atom, p);
            if (term_is_var(term) != 0)
            {
                if (term->u.variable == var)
                {
                    return 1;
                }
            }
        }
    }
    return 0;
}

GREEN_PURE
static int head_var_unsafe(const dl89_rule *rule, const dl89_term *term)
{
    if (rule->body_count == 0)
    {
        return 1;
    }
    if (var_in_body(rule, term->u.variable) == 0)
    {
        return 1;
    }
    return 0;
}

GREEN_PURE
static int check_head_safety(const dl89_rule *rule)
{
    size_t p;
    const dl89_term *term;

    for (p = 0; p < rule->head.arity; ++p)
    {
        term = head_term(rule, p);
        if (term_is_var(term) != 0)
        {
            if (head_var_unsafe(rule, term) != 0)
            {
                return DL89_EPROGRAM;
            }
        }
    }
    return DL89_OK;
}

GREEN_PURE
int dl89_priv_rule_check(const dl89_eval *eval, const dl89_rule *rule)
{
    size_t count;
    size_t i;
    int st;

    if (rule == NULL)
    {
        return DL89_EINVAL;
    }
    if (rule->body_count > 0)
    {
        if (rule->body == NULL)
        {
            return DL89_EINVAL;
        }
    }
    count = atom_count_of(rule);
    for (i = 0; i < count; ++i)
    {
        st = check_atom_terms(atom_of(rule, i));
        if (st != DL89_OK)
        {
            return st;
        }
    }
    st = check_rule_arities(rule);
    if (st != DL89_OK)
    {
        return st;
    }
    st = check_rule_registry(eval, rule);
    if (st != DL89_OK)
    {
        return st;
    }
    return check_head_safety(rule);
}

/* --- compiled-rule storage --------------------------------------------- */

static void crule_init(dl89_priv_crule *rule)
{
    rule->head.relation = 0;
    rule->head.arity = 0;
    rule->head.terms = NULL;
    rule->body_count = 0;
    rule->body = NULL;
    rule->var_count = 0;
    rule->env = NULL;
    rule->bound = NULL;
    rule->values = NULL;
    rule->bvalues = NULL;
    rule->tuple = NULL;
    rule->tuple_cap = 0;
    rule->bound_log = NULL;
}

static void init_catom(dl89_priv_catom *atom)
{
    atom->relation = 0;
    atom->arity = 0;
    atom->terms = NULL;
}

void dl89_priv_crule_release(dl89_priv_crule *rule)
{
    size_t b;

    dl89_priv_mem_free(rule->head.terms);
    for (b = 0; b < rule->body_count; ++b)
    {
        dl89_priv_mem_free(rule->body[b].terms);
    }
    dl89_priv_mem_free(rule->body);
    dl89_priv_mem_free(rule->env);
    dl89_priv_mem_free(rule->bound);
    dl89_priv_mem_free(rule->values);
    dl89_priv_mem_free(rule->bvalues);
    dl89_priv_mem_free(rule->tuple);
    dl89_priv_mem_free(rule->bound_log);
    crule_init(rule);
}

/* --- pure size helpers -------------------------------------------------- */

GREEN_PURE
static size_t add_arity(size_t total, size_t arity)
{
    return total + arity;
}

GREEN_PURE
static int sum_overflows(size_t total, size_t arity)
{
    return total > ((size_t)-1) - arity;
}

GREEN_PURE
static size_t max_arity_of(const dl89_rule *rule)
{
    size_t max;
    size_t b;

    max = rule->head.arity;
    for (b = 0; b < rule->body_count; ++b)
    {
        if (body_arity(rule, b) > max)
        {
            max = body_arity(rule, b);
        }
    }
    return max;
}

static int total_terms(const dl89_rule *rule, size_t *out_total)
{
    size_t total;
    size_t b;

    total = rule->head.arity;
    for (b = 0; b < rule->body_count; ++b)
    {
        if (sum_overflows(total, body_arity(rule, b)) != 0)
        {
            return DL89_ENOMEM;
        }
        total = add_arity(total, body_arity(rule, b));
    }
    *out_total = total;
    return DL89_OK;
}

/* --- deep copy ---------------------------------------------------------- */

static void *alloc_array(size_t count, size_t size)
{
    void *mem;

    if (size != 0)
    {
        if (count > ((size_t)-1) / size)
        {
            return NULL;
        }
    }
    mem = dl89_priv_mem_alloc(count * size);
    return mem;
}

static size_t add_var(dl89_var *ids, size_t *id_count, dl89_var var)
{
    size_t slot;

    slot = *id_count;
    ids[slot] = var;
    *id_count = *id_count + 1;
    return slot;
}

static void set_constant(dl89_priv_cterm *dst, dl89_const constant)
{
    dst->constant = constant;
}

static void copy_term_one(const dl89_term *term, dl89_priv_cterm *dst,
                          dl89_var *ids, size_t *id_count)
{
    size_t slot;

    dst->kind = term->kind;
    dst->constant = 0;
    dst->slot = 0;
    if (term_is_var(term) == 0)
    {
        set_constant(dst, term->u.constant);
        return;
    }
    slot = var_slot(ids, *id_count, term->u.variable);
    if (slot == DL89_PRIV_NO_SLOT)
    {
        slot = add_var(ids, id_count, term->u.variable);
    }
    dst->slot = slot;
}

static int copy_terms(const dl89_atom *src, dl89_priv_catom *dst, dl89_var *ids,
                      size_t *id_count)
{
    size_t p;

    dst->relation = src->relation;
    dst->arity = src->arity;
    dst->terms = NULL;
    if (src->arity == 0)
    {
        return DL89_OK;
    }
    dst->terms = alloc_array(src->arity, sizeof(*dst->terms));
    if (dst->terms == NULL)
    {
        return DL89_ENOMEM;
    }
    for (p = 0; p < src->arity; ++p)
    {
        copy_term_one(&src->terms[p], &dst->terms[p], ids, id_count);
    }
    return DL89_OK;
}

static int copy_body(const dl89_rule *rule, dl89_priv_crule *out, dl89_var *ids,
                     size_t *id_count)
{
    size_t b;
    int st;

    if (rule->body_count == 0)
    {
        return DL89_OK;
    }
    out->body = alloc_array(rule->body_count, sizeof(*out->body));
    if (out->body == NULL)
    {
        return DL89_ENOMEM;
    }
    for (b = 0; b < rule->body_count; ++b)
    {
        init_catom(&out->body[b]);
    }
    out->body_count = rule->body_count;
    for (b = 0; b < rule->body_count; ++b)
    {
        st = copy_terms(rule_body_atom(rule, b), &out->body[b], ids, id_count);
        if (st != DL89_OK)
        {
            return st;
        }
    }
    return DL89_OK;
}

static int log_capacity(size_t max_arity, size_t body_count, size_t *out)
{
    if (body_count != 0)
    {
        if (max_arity > ((size_t)-1) / body_count)
        {
            return DL89_ENOMEM;
        }
    }
    *out = max_arity * body_count;
    return DL89_OK;
}

static int alloc_scratch(dl89_priv_crule *out, size_t max_arity)
{
    size_t n;
    size_t logs;
    int st;

    st = log_capacity(max_arity, out->body_count, &logs);
    if (st != DL89_OK)
    {
        return st;
    }
    n = out->var_count;
    out->env = alloc_array(n, sizeof(*out->env));
    if (out->env == NULL)
    {
        return DL89_ENOMEM;
    }
    out->bound = alloc_array(n, sizeof(*out->bound));
    if (out->bound == NULL)
    {
        return DL89_ENOMEM;
    }
    out->values = alloc_array(max_arity, sizeof(*out->values));
    if (out->values == NULL)
    {
        return DL89_ENOMEM;
    }
    out->bvalues = alloc_array(max_arity, sizeof(*out->bvalues));
    if (out->bvalues == NULL)
    {
        return DL89_ENOMEM;
    }
    out->tuple = alloc_array(max_arity, sizeof(*out->tuple));
    if (out->tuple == NULL)
    {
        return DL89_ENOMEM;
    }
    out->bound_log = alloc_array(logs, sizeof(*out->bound_log));
    if (out->bound_log == NULL)
    {
        return DL89_ENOMEM;
    }
    out->tuple_cap = max_arity;
    return DL89_OK;
}

static int finish_compile(dl89_priv_crule *out, size_t var_count,
                          size_t max_arity)
{
    int st;

    out->var_count = var_count;
    st = alloc_scratch(out, max_arity);
    return st;
}

int dl89_priv_crule_compile(const dl89_rule *rule, dl89_priv_crule *out)
{
    dl89_var *ids;
    size_t total;
    size_t id_count;
    int st;

    crule_init(out);
    st = total_terms(rule, &total);
    if (st != DL89_OK)
    {
        return st;
    }
    ids = alloc_array(total, sizeof(*ids));
    if (ids == NULL)
    {
        return DL89_ENOMEM;
    }
    id_count = 0;
    st = copy_terms(&rule->head, &out->head, ids, &id_count);
    if (st == DL89_OK)
    {
        st = copy_body(rule, out, ids, &id_count);
    }
    if (st == DL89_OK)
    {
        st = finish_compile(out, id_count, max_arity_of(rule));
    }
    dl89_priv_mem_free(ids);
    if (st != DL89_OK)
    {
        dl89_priv_crule_release(out);
        return st;
    }
    return DL89_OK;
}

/* --- atomic installation ------------------------------------------------ */

static int register_atom(dl89_eval *eval, const dl89_atom *atom)
{
    int st;

    if (dl89_priv_registry_find(eval, atom->relation) != NULL)
    {
        return DL89_OK;
    }
    st = dl89_priv_registry_add(eval, atom->relation, atom->arity);
    return st;
}

static int register_rule(dl89_eval *eval, const dl89_rule *rule)
{
    size_t count;
    size_t i;
    int st;

    count = atom_count_of(rule);
    for (i = 0; i < count; ++i)
    {
        st = register_atom(eval, atom_of(rule, i));
        if (st != DL89_OK)
        {
            return st;
        }
    }
    return DL89_OK;
}

GREEN_PURE
static size_t rule_next_cap(size_t cap)
{
    size_t next;

    if (cap == 0)
    {
        return 4;
    }
    if (cap > ((size_t)-1) / 2)
    {
        return 0;
    }
    next = cap * 2;
    if (next > ((size_t)-1) / sizeof(dl89_priv_crule))
    {
        return 0;
    }
    return next;
}

static void rules_take(dl89_eval *eval, dl89_priv_crule *grown, size_t cap)
{
    eval->rules = grown;
    eval->rule_cap = cap;
}

static int rules_grow(dl89_eval *eval)
{
    dl89_priv_crule *grown;
    size_t cap;

    cap = rule_next_cap(eval->rule_cap);
    if (cap == 0)
    {
        return DL89_ENOMEM;
    }
    grown = dl89_priv_mem_realloc(eval->rules, cap * sizeof(*grown));
    if (grown == NULL)
    {
        return DL89_ENOMEM;
    }
    rules_take(eval, grown, cap);
    return DL89_OK;
}

static int rules_append(dl89_eval *eval, const dl89_priv_crule *compiled)
{
    int st;

    if (eval->rule_count == eval->rule_cap)
    {
        st = rules_grow(eval);
        if (st != DL89_OK)
        {
            return st;
        }
    }
    eval->rules[eval->rule_count] = *compiled;
    eval->rule_count = eval->rule_count + 1;
    return DL89_OK;
}

static void install_rollback(dl89_eval *eval, size_t start,
                             dl89_priv_crule *compiled)
{
    dl89_priv_registry_truncate(eval, start);
    dl89_priv_crule_release(compiled);
}

int dl89_priv_rule_install(dl89_eval *eval, const dl89_rule *rule)
{
    dl89_priv_crule compiled;
    size_t start;
    int st;

    st = dl89_priv_rule_check(eval, rule);
    if (st != DL89_OK)
    {
        return st;
    }
    st = dl89_priv_crule_compile(rule, &compiled);
    if (st != DL89_OK)
    {
        return st;
    }
    start = eval->arity_count;
    st = register_rule(eval, rule);
    if (st != DL89_OK)
    {
        install_rollback(eval, start, &compiled);
        return st;
    }
    st = rules_append(eval, &compiled);
    if (st != DL89_OK)
    {
        install_rollback(eval, start, &compiled);
        return st;
    }
    return DL89_OK;
}

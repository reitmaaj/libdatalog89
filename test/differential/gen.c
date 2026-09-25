/* gen.c - deterministic generator of range-restricted Datalog programs.
 *
 * Shape: 1..6 relations with arity 0..3, 1..8 constants, 0..8 rules, body
 * length 0..4, 1..4 variables per rule, 0..20 initial facts. Generated
 * programs include recursion, repeated variables, constants, nullary
 * predicates, duplicate rules, and empty relations. The same seed always
 * yields the same case. */

#include <string.h>

#include "gen.h"

static unsigned long gen_rand(unsigned long *state)
{
    unsigned long x;

    x = *state;
    x = x ^ (x << 13);
    x = x ^ (x >> 7);
    x = x ^ (x << 17);
    *state = x;
    return x;
}

static size_t gen_below(unsigned long *state, size_t limit)
{
    if (limit == 0)
    {
        return 0;
    }
    return (size_t)(gen_rand(state) % (unsigned long)limit);
}

static dl89_term *gen_next_term(gen_rule *rule)
{
    dl89_term *term;

    term = &rule->terms[rule->term_count];
    rule->term_count = rule->term_count + 1;
    return term;
}

static void gen_set_var(dl89_term *term, size_t var_count, unsigned long *state)
{
    term->kind = DL89_TERM_VAR;
    term->u.variable = (dl89_var)(1 + gen_below(state, var_count));
}

static void gen_set_const(dl89_term *term, const gen_case *c,
                          unsigned long *state)
{
    term->kind = DL89_TERM_CONST;
    term->u.constant = c->constants[gen_below(state, c->constant_count)];
}

static void gen_rule_body(gen_case *c, gen_rule *rule, size_t var_count,
                          unsigned long *state)
{
    size_t b;
    size_t p;

    rule->rule.body_count = gen_below(state, GEN_MAX_BODY + 1);
    for (b = 0; b < rule->rule.body_count; ++b)
    {
        size_t ri;
        size_t arity;

        ri = gen_below(state, c->relation_count);
        arity = c->relations[ri].arity;
        rule->body[b].relation = c->relations[ri].relation;
        rule->body[b].arity = arity;
        rule->body[b].terms = NULL;
        if (arity > 0)
        {
            dl89_term *first;

            first = gen_next_term(rule);
            if (gen_below(state, 3) == 0)
            {
                gen_set_const(first, c, state);
            }
            else
            {
                gen_set_var(first, var_count, state);
            }
            rule->body[b].terms = first;
            for (p = 1; p < arity; ++p)
            {
                dl89_term *term;

                term = gen_next_term(rule);
                if (gen_below(state, 4) == 0)
                {
                    *term = *first;
                }
                else
                {
                    if (gen_below(state, 3) == 0)
                    {
                        gen_set_const(term, c, state);
                    }
                    else
                    {
                        gen_set_var(term, var_count, state);
                    }
                }
            }
        }
    }
}

static size_t gen_body_var_count(const gen_rule *rule)
{
    size_t b;
    size_t p;
    size_t n;

    n = 0;
    for (b = 0; b < rule->rule.body_count; ++b)
    {
        for (p = 0; p < rule->body[b].arity; ++p)
        {
            if (rule->body[b].terms[p].kind == DL89_TERM_VAR)
            {
                n = n + 1;
            }
        }
    }
    return n;
}

static void gen_pick_body_var(const gen_rule *rule, size_t which,
                              dl89_term *out)
{
    size_t b;
    size_t p;
    size_t n;

    n = 0;
    for (b = 0; b < rule->rule.body_count; ++b)
    {
        for (p = 0; p < rule->body[b].arity; ++p)
        {
            if (rule->body[b].terms[p].kind == DL89_TERM_VAR)
            {
                if (n == which)
                {
                    *out = rule->body[b].terms[p];
                    return;
                }
                n = n + 1;
            }
        }
    }
}

static void gen_rule_head(gen_case *c, gen_rule *rule, unsigned long *state)
{
    size_t ri;
    size_t arity;
    size_t p;
    size_t vars;

    ri = gen_below(state, c->relation_count);
    if (rule->rule.body_count > 0)
    {
        if (gen_below(state, 4) == 0)
        {
            size_t b;

            b = gen_below(state, rule->rule.body_count);
            rule->head.relation = rule->body[b].relation;
        }
        else
        {
            rule->head.relation = c->relations[ri].relation;
        }
    }
    else
    {
        rule->head.relation = c->relations[ri].relation;
    }
    arity = 0;
    for (p = 0; p < c->relation_count; ++p)
    {
        if (c->relations[p].relation == rule->head.relation)
        {
            arity = c->relations[p].arity;
        }
    }
    rule->head.arity = arity;
    rule->head.terms = NULL;
    vars = gen_body_var_count(rule);
    for (p = 0; p < arity; ++p)
    {
        dl89_term *term;

        term = gen_next_term(rule);
        if (p == 0)
        {
            rule->head.terms = term;
        }
        if (vars > 0)
        {
            if (gen_below(state, 3) != 0)
            {
                size_t pick;

                pick = gen_below(state, vars);
                gen_pick_body_var(rule, pick, term);
                continue;
            }
        }
        gen_set_const(term, c, state);
    }
}

static void gen_build_rule(gen_case *c, gen_rule *rule, unsigned long *state)
{
    size_t var_count;

    rule->term_count = 0;
    var_count = 1 + gen_below(state, 4);
    gen_rule_body(c, rule, var_count, state);
    gen_rule_head(c, rule, state);
    rule->rule.body = rule->body;
    rule->rule.head = rule->head;
    if (rule->rule.body_count == 0)
    {
        rule->rule.body = NULL;
    }
}

static void gen_facts(gen_case *c, unsigned long *state)
{
    size_t i;
    size_t p;

    c->fact_count = gen_below(state, GEN_MAX_FACTS + 1);
    for (i = 0; i < c->fact_count; ++i)
    {
        size_t ri;
        size_t arity;

        ri = gen_below(state, c->relation_count);
        arity = c->relations[ri].arity;
        c->fact_relations[i] = c->relations[ri].relation;
        c->fact_arities[i] = arity;
        for (p = 0; p < arity; ++p)
        {
            c->facts[i][p] = c->constants[gen_below(state, c->constant_count)];
        }
    }
}

void gen_build(gen_case *c, unsigned long seed)
{
    unsigned long state;
    size_t i;
    size_t r;

    memset(c, 0, sizeof(*c));
    c->seed = seed;
    state = seed;
    if (state == 0)
    {
        state = 1;
    }
    c->relation_count = 1 + gen_below(&state, GEN_MAX_RELATIONS);
    for (i = 0; i < c->relation_count; ++i)
    {
        c->relations[i].relation = (dl89_rel)(i + 1);
        c->relations[i].arity = gen_below(&state, GEN_MAX_ARITY + 1);
    }
    c->constant_count = 1 + gen_below(&state, GEN_MAX_CONSTANTS);
    for (i = 0; i < c->constant_count; ++i)
    {
        c->constants[i] = (dl89_const)(100 + gen_below(&state, 900));
    }
    c->rule_count = gen_below(&state, GEN_MAX_RULES + 1);
    for (r = 0; r < c->rule_count; ++r)
    {
        gen_build_rule(c, &c->rules[r], &state);
    }
    gen_facts(c, &state);
}

void gen_print(const gen_case *c, FILE *out)
{
    size_t i;
    size_t p;
    size_t b;

    fprintf(out, "seed %lu\n", c->seed);
    fprintf(out, "relations:");
    for (i = 0; i < c->relation_count; ++i)
    {
        fprintf(out, " r%lu/%lu", (unsigned long)c->relations[i].relation,
                (unsigned long)c->relations[i].arity);
    }
    fprintf(out, "\nconstants:");
    for (i = 0; i < c->constant_count; ++i)
    {
        fprintf(out, " c%lu=%lu", (unsigned long)(i + 1),
                (unsigned long)c->constants[i]);
    }
    fprintf(out, "\nrules:\n");
    for (i = 0; i < c->rule_count; ++i)
    {
        const gen_rule *rule;

        rule = &c->rules[i];
        fprintf(out, "  r%lu(", (unsigned long)rule->head.relation);
        for (p = 0; p < rule->head.arity; ++p)
        {
            if (p > 0)
            {
                fprintf(out, ", ");
            }
            if (rule->head.terms[p].kind == DL89_TERM_CONST)
            {
                fprintf(out, "%lu",
                        (unsigned long)rule->head.terms[p].u.constant);
            }
            else
            {
                fprintf(out, "X%lu",
                        (unsigned long)rule->head.terms[p].u.variable);
            }
        }
        fprintf(out, ")");
        if (rule->rule.body_count > 0)
        {
            fprintf(out, " :- ");
        }
        for (b = 0; b < rule->rule.body_count; ++b)
        {
            if (b > 0)
            {
                fprintf(out, ", ");
            }
            fprintf(out, "r%lu(", (unsigned long)rule->body[b].relation);
            for (p = 0; p < rule->body[b].arity; ++p)
            {
                if (p > 0)
                {
                    fprintf(out, ", ");
                }
                if (rule->body[b].terms[p].kind == DL89_TERM_CONST)
                {
                    fprintf(out, "%lu",
                            (unsigned long)rule->body[b].terms[p].u.constant);
                }
                else
                {
                    fprintf(out, "X%lu",
                            (unsigned long)rule->body[b].terms[p].u.variable);
                }
            }
            fprintf(out, ")");
        }
        fprintf(out, "\n");
    }
    fprintf(out, "facts:\n");
    for (i = 0; i < c->fact_count; ++i)
    {
        fprintf(out, "  r%lu(", (unsigned long)c->fact_relations[i]);
        for (p = 0; p < c->fact_arities[i]; ++p)
        {
            if (p > 0)
            {
                fprintf(out, ", ");
            }
            fprintf(out, "%lu", (unsigned long)c->facts[i][p]);
        }
        fprintf(out, ")\n");
    }
}

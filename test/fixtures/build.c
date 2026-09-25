/* build.c - rule/atom/term builders for the unit tests. */

#include "build.h"

void mk_const(dl89_term *term, dl89_const constant)
{
    term->kind = DL89_TERM_CONST;
    term->u.constant = constant;
}

void mk_var(dl89_term *term, dl89_var variable)
{
    term->kind = DL89_TERM_VAR;
    term->u.variable = variable;
}

void mk_atom(dl89_atom *atom, dl89_rel relation, size_t arity,
             const dl89_term *terms)
{
    atom->relation = relation;
    atom->arity = arity;
    atom->terms = terms;
}

void mk_rule(dl89_rule *rule, const dl89_atom *head, size_t body_count,
             const dl89_atom *body)
{
    rule->head = *head;
    rule->body_count = body_count;
    rule->body = body;
}

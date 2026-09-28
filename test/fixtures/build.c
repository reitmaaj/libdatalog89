/* build.c - rule/atom/term builders for the unit tests. */

#include "build.h"

void mk_const(datalog89_term *term, datalog89_const constant)
{
    term->kind = DATALOG89_TERM_CONST;
    term->u.constant = constant;
}

void mk_var(datalog89_term *term, datalog89_var variable)
{
    term->kind = DATALOG89_TERM_VAR;
    term->u.variable = variable;
}

void mk_atom(datalog89_atom *atom, datalog89_rel relation, size_t arity,
             const datalog89_term *terms)
{
    atom->relation = relation;
    atom->arity = arity;
    atom->terms = terms;
}

void mk_rule(datalog89_rule *rule, const datalog89_atom *head,
             size_t body_count, const datalog89_atom *body)
{
    rule->head = *head;
    rule->body_count = body_count;
    rule->body = body;
}

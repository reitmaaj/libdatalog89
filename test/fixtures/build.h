#ifndef DATALOG89_BUILD_H
#define DATALOG89_BUILD_H

/* Small builders shared by the unit tests. */

#include <datalog89.h>

void mk_const(datalog89_term *term, datalog89_const constant);
void mk_var(datalog89_term *term, datalog89_var variable);
void mk_atom(datalog89_atom *atom, datalog89_rel relation, size_t arity,
             const datalog89_term *terms);
void mk_rule(datalog89_rule *rule, const datalog89_atom *head, size_t body_count,
             const datalog89_atom *body);

#endif

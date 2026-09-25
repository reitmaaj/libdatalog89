#ifndef DL89_BUILD_H
#define DL89_BUILD_H

/* Small builders shared by the unit tests. */

#include <dl89.h>

void mk_const(dl89_term *term, dl89_const constant);
void mk_var(dl89_term *term, dl89_var variable);
void mk_atom(dl89_atom *atom, dl89_rel relation, size_t arity,
             const dl89_term *terms);
void mk_rule(dl89_rule *rule, const dl89_atom *head, size_t body_count,
             const dl89_atom *body);

#endif

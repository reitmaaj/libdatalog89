#ifndef DL89_GEN_H
#define DL89_GEN_H

#include <stdio.h>

#include <dl89.h>

#define GEN_MAX_RELATIONS 6
#define GEN_MAX_CONSTANTS 8
#define GEN_MAX_RULES 8
#define GEN_MAX_BODY 4
#define GEN_MAX_ARITY 3
#define GEN_MAX_TERMS 32
#define GEN_MAX_FACTS 20

typedef struct
{
    dl89_rel relation;
    size_t arity;
} gen_relation;

typedef struct
{
    dl89_term terms[GEN_MAX_TERMS];
    size_t term_count;
    dl89_atom body[GEN_MAX_BODY];
    dl89_atom head;
    dl89_rule rule;
} gen_rule;

typedef struct
{
    unsigned long seed;
    gen_relation relations[GEN_MAX_RELATIONS];
    size_t relation_count;
    dl89_const constants[GEN_MAX_CONSTANTS];
    size_t constant_count;
    gen_rule rules[GEN_MAX_RULES];
    size_t rule_count;
    dl89_const facts[GEN_MAX_FACTS][GEN_MAX_ARITY];
    dl89_rel fact_relations[GEN_MAX_FACTS];
    size_t fact_arities[GEN_MAX_FACTS];
    size_t fact_count;
} gen_case;

void gen_build(gen_case *c, unsigned long seed);
void gen_print(const gen_case *c, FILE *out);

#endif

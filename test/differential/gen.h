#ifndef DATALOG89_GEN_H
#define DATALOG89_GEN_H

#include <stdio.h>

#include <datalog89.h>

#define GEN_MAX_RELATIONS 6
#define GEN_MAX_CONSTANTS 8
#define GEN_MAX_RULES 8
#define GEN_MAX_BODY 4
#define GEN_MAX_ARITY 3
#define GEN_MAX_TERMS 32
#define GEN_MAX_FACTS 20

typedef struct
{
    datalog89_rel relation;
    size_t arity;
} gen_relation;

typedef struct
{
    datalog89_term terms[GEN_MAX_TERMS];
    size_t term_count;
    datalog89_atom body[GEN_MAX_BODY];
    datalog89_atom head;
    datalog89_rule rule;
} gen_rule;

typedef struct
{
    unsigned long seed;
    gen_relation relations[GEN_MAX_RELATIONS];
    size_t relation_count;
    datalog89_const constants[GEN_MAX_CONSTANTS];
    size_t constant_count;
    gen_rule rules[GEN_MAX_RULES];
    size_t rule_count;
    datalog89_const facts[GEN_MAX_FACTS][GEN_MAX_ARITY];
    datalog89_rel fact_relations[GEN_MAX_FACTS];
    size_t fact_arities[GEN_MAX_FACTS];
    size_t fact_count;
} gen_case;

void gen_build(gen_case *c, unsigned long seed);
void gen_print(const gen_case *c, FILE *out);

#endif

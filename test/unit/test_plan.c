/* test_plan.c - white-box tests for the compiled execution plan.
 *
 * These tests pin plan structure through the datalog89_test_* hooks. Semantic
 * suites stay join-order agnostic; this file is the exception by design. */

#include <datalog89.h>

#include <test.h>

#include "build.h"
#include "datalog89_test.h"
#include "expect.h"
#include "ref_store.h"
#include "symbols.h"

int datalog89_test_failures = 0;

/* Shape program: joined(X,Y) :- same(X), same(Y) and
 * p(X,Y) :- q(X,Z), joined(Z,Y). Each rule has 2 body atoms, so the plan
 * holds 6 variants and 12 steps in rule-major order. */
enum
{
    PLAN_TOTAL_VARIANTS = 6
};

static datalog89_eval *make_eval(ref_store *store)
{
    datalog89_eval_config config;
    datalog89_eval *eval;

    config.store = ref_store_datalog89(store);
    eval = NULL;
    T_STATUS(datalog89_eval_create(&config, &eval), DATALOG89_OK);
    return eval;
}

static void add_shape_rules(datalog89_eval *eval, datalog89_rule *rules,
                            datalog89_term *terms, datalog89_atom *atoms)
{
    mk_var(&terms[0], V_X);
    mk_var(&terms[1], V_Y);
    mk_var(&terms[2], V_X);
    mk_var(&terms[3], V_Y);
    mk_atom(&atoms[0], R_JOINED, 2, &terms[0]);
    mk_atom(&atoms[1], R_SAME, 1, &terms[2]);
    mk_atom(&atoms[2], R_SAME, 1, &terms[3]);
    mk_rule(&rules[0], &atoms[0], 2, &atoms[1]);

    mk_var(&terms[4], V_X);
    mk_var(&terms[5], V_Y);
    mk_var(&terms[6], V_X);
    mk_var(&terms[7], V_Z);
    mk_var(&terms[8], V_Z);
    mk_var(&terms[9], V_Y);
    mk_atom(&atoms[3], R_P, 2, &terms[4]);
    mk_atom(&atoms[4], R_Q, 2, &terms[6]);
    mk_atom(&atoms[5], R_JOINED, 2, &terms[8]);
    mk_rule(&rules[1], &atoms[3], 2, &atoms[4]);

    T_STATUS(datalog89_eval_add_rule(eval, &rules[0]), DATALOG89_OK);
    T_STATUS(datalog89_eval_add_rule(eval, &rules[1]), DATALOG89_OK);
}

static void seed1(datalog89_eval *eval, datalog89_rel relation,
                  datalog89_const value)
{
    datalog89_const tuple[1];

    tuple[0] = value;
    T_STATUS(datalog89_eval_add_fact(eval, relation, 1, tuple), DATALOG89_OK);
}

static void seed2(datalog89_eval *eval, datalog89_rel relation,
                  datalog89_const a, datalog89_const b)
{
    datalog89_const tuple[2];

    tuple[0] = a;
    tuple[1] = b;
    T_STATUS(datalog89_eval_add_fact(eval, relation, 2, tuple), DATALOG89_OK);
}

static void test_plan_cache(void)
{
    ref_store *store;
    datalog89_eval *eval;
    datalog89_rule rules[3];
    datalog89_term terms[14];
    datalog89_atom atoms[6];

    store = ref_store_new();
    eval = make_eval(store);
    add_shape_rules(eval, rules, terms, atoms);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    T_EQ_SIZE(datalog89_test_plan_builds(eval), 1);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    T_EQ_SIZE(datalog89_test_plan_builds(eval), 1);
    mk_var(&terms[10], V_X);
    mk_var(&terms[11], V_X);
    mk_atom(&atoms[4], R_LEFT, 1, &terms[10]);
    mk_atom(&atoms[5], R_R, 1, &terms[11]);
    mk_rule(&rules[2], &atoms[4], 1, &atoms[5]);
    T_STATUS(datalog89_eval_add_rule(eval, &rules[2]), DATALOG89_OK);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    T_EQ_SIZE(datalog89_test_plan_builds(eval), 2);
    seed2(eval, R_Q, C_A, C_B);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    T_EQ_SIZE(datalog89_test_plan_builds(eval), 2);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

static void check_step(const datalog89_eval *eval, size_t variant, size_t step,
                       datalog89_rel relation, size_t arity,
                       datalog89_test_src source, size_t nbound, size_t nconst)
{
    datalog89_test_step_info info;

    info.relation = 0;
    info.arity = 0;
    info.source = DATALOG89_TEST_SRC_FULL;
    info.nbound = 0;
    info.nconst = 0;
    T_STATUS(datalog89_test_plan_step_info(eval, variant, step, &info),
             DATALOG89_OK);
    T_EQ_SIZE(info.relation, relation);
    T_EQ_SIZE(info.arity, arity);
    T_EQ_SIZE((size_t)info.source, (size_t)source);
    T_EQ_SIZE(info.nbound, nbound);
    T_EQ_SIZE(info.nconst, nconst);
}

static void test_variant_shape(void)
{
    ref_store *store;
    datalog89_eval *eval;
    datalog89_rule rules[2];
    datalog89_term terms[14];
    datalog89_atom atoms[6];
    datalog89_test_variant_info v;
    datalog89_const pair[2];

    store = ref_store_new();
    eval = make_eval(store);
    T_EQ_SIZE(datalog89_test_plan_builds(eval), 0);
    T_EQ_SIZE(datalog89_test_plan_variant_count(eval), 0);
    T_STATUS(datalog89_test_plan_variant_info(eval, 0, &v), DATALOG89_EINVAL);
    add_shape_rules(eval, rules, terms, atoms);
    seed1(eval, R_SAME, C_A);
    seed1(eval, R_SAME, C_B);
    seed2(eval, R_Q, C_A, C_B);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    T_EQ_SIZE(datalog89_test_plan_variant_count(eval), PLAN_TOTAL_VARIANTS);
    T_STATUS(datalog89_test_plan_variant_info(eval, 0, &v), DATALOG89_OK);
    T_EQ_SIZE(v.head, R_JOINED);
    T_EQ_SIZE(v.head_arity, 2);
    T_EQ_SIZE(v.nsteps, 2);
    T_EQ_SIZE(v.delta_pos, 0);
    T_EQ_SIZE((size_t)v.idb_delta, 0);
    check_step(eval, 0, 0, R_SAME, 1, DATALOG89_TEST_SRC_DELTA, 0, 0);
    check_step(eval, 0, 1, R_SAME, 1, DATALOG89_TEST_SRC_FULL, 0, 0);
    T_STATUS(datalog89_test_plan_variant_info(eval, 1, &v), DATALOG89_OK);
    T_EQ_SIZE(v.delta_pos, 1);
    T_EQ_SIZE(v.nsteps, 2);
    T_EQ_SIZE((size_t)v.idb_delta, 0);
    check_step(eval, 1, 0, R_SAME, 1, DATALOG89_TEST_SRC_FULL, 0, 0);
    check_step(eval, 1, 1, R_SAME, 1, DATALOG89_TEST_SRC_DELTA, 0, 0);
    T_STATUS(datalog89_test_plan_variant_info(eval, 2, &v), DATALOG89_OK);
    T_EQ_SIZE(v.delta_pos, 2);
    T_EQ_SIZE(v.nsteps, 2);
    T_EQ_SIZE((size_t)v.idb_delta, 0);
    check_step(eval, 2, 0, R_SAME, 1, DATALOG89_TEST_SRC_FULL, 0, 0);
    check_step(eval, 2, 1, R_SAME, 1, DATALOG89_TEST_SRC_FULL, 0, 0);
    T_STATUS(datalog89_test_plan_variant_info(eval, 3, &v), DATALOG89_OK);
    T_EQ_SIZE(v.head, R_P);
    T_EQ_SIZE(v.head_arity, 2);
    T_EQ_SIZE(v.nsteps, 2);
    T_EQ_SIZE(v.delta_pos, 0);
    T_EQ_SIZE((size_t)v.idb_delta, 0);
    check_step(eval, 3, 0, R_Q, 2, DATALOG89_TEST_SRC_DELTA, 0, 0);
    check_step(eval, 3, 1, R_JOINED, 2, DATALOG89_TEST_SRC_FULL, 1, 0);
    T_STATUS(datalog89_test_plan_variant_info(eval, 4, &v), DATALOG89_OK);
    T_EQ_SIZE(v.delta_pos, 1);
    T_EQ_SIZE((size_t)v.idb_delta, 1);
    check_step(eval, 4, 0, R_Q, 2, DATALOG89_TEST_SRC_FULL, 0, 0);
    check_step(eval, 4, 1, R_JOINED, 2, DATALOG89_TEST_SRC_DELTA, 1, 0);
    T_STATUS(datalog89_test_plan_variant_info(eval, 5, &v), DATALOG89_OK);
    T_EQ_SIZE(v.delta_pos, 2);
    T_EQ_SIZE(v.nsteps, 2);
    T_EQ_SIZE((size_t)v.idb_delta, 0);
    check_step(eval, 5, 0, R_Q, 2, DATALOG89_TEST_SRC_FULL, 0, 0);
    check_step(eval, 5, 1, R_JOINED, 2, DATALOG89_TEST_SRC_FULL, 1, 0);
    pair[0] = C_A;
    pair[1] = C_B;
    expect_tuple(store, R_JOINED, 2, pair);
    pair[0] = C_A;
    pair[1] = C_A;
    expect_tuple(store, R_P, 2, pair);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_constant_pattern(void)
{
    ref_store *store;
    datalog89_eval *eval;
    datalog89_rule rule;
    datalog89_term terms[6];
    datalog89_atom atoms[3];
    datalog89_const pair[2];

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&terms[0], V_X);
    mk_const(&terms[1], C_A);
    mk_var(&terms[2], V_X);
    mk_var(&terms[3], V_Z);
    mk_var(&terms[4], V_Z);
    mk_const(&terms[5], C_A);
    mk_atom(&atoms[0], R_JOINED, 2, &terms[0]);
    mk_atom(&atoms[1], R_Q, 2, &terms[2]);
    mk_atom(&atoms[2], R_SAME, 2, &terms[4]);
    mk_rule(&rule, &atoms[0], 2, &atoms[1]);
    T_STATUS(datalog89_eval_add_rule(eval, &rule), DATALOG89_OK);
    seed2(eval, R_SAME, C_B, C_A);
    seed2(eval, R_Q, C_A, C_B);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    check_step(eval, 0, 0, R_Q, 2, DATALOG89_TEST_SRC_DELTA, 0, 0);
    check_step(eval, 2, 0, R_Q, 2, DATALOG89_TEST_SRC_FULL, 0, 0);
    check_step(eval, 2, 1, R_SAME, 2, DATALOG89_TEST_SRC_FULL, 1, 1);
    pair[0] = C_A;
    pair[1] = C_A;
    expect_tuple(store, R_JOINED, 2, pair);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_hook_validation(void)
{
    ref_store *store;
    datalog89_eval *eval;
    datalog89_rule rules[2];
    datalog89_term terms[14];
    datalog89_atom atoms[6];
    datalog89_test_variant_info v;
    datalog89_test_step_info s;

    store = ref_store_new();
    eval = make_eval(store);
    add_shape_rules(eval, rules, terms, atoms);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    T_STATUS(datalog89_test_plan_variant_info(NULL, 0, &v), DATALOG89_EINVAL);
    T_STATUS(datalog89_test_plan_variant_info(eval, 0, NULL), DATALOG89_EINVAL);
    T_STATUS(datalog89_test_plan_variant_info(eval, PLAN_TOTAL_VARIANTS, &v),
             DATALOG89_EINVAL);
    T_STATUS(datalog89_test_plan_step_info(NULL, 0, 0, &s), DATALOG89_EINVAL);
    T_STATUS(datalog89_test_plan_step_info(eval, 0, 0, NULL), DATALOG89_EINVAL);
    T_STATUS(datalog89_test_plan_step_info(eval, PLAN_TOTAL_VARIANTS, 0, &s),
             DATALOG89_EINVAL);
    T_STATUS(datalog89_test_plan_step_info(eval, 0, 2, &s), DATALOG89_EINVAL);
    T_EQ_SIZE(datalog89_test_plan_builds(NULL), 0);
    T_EQ_SIZE(datalog89_test_plan_variant_count(NULL), 0);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

static void dump_plan(const datalog89_eval *eval, datalog89_rel *heads,
                      size_t *delta_positions, int *idb_flags,
                      datalog89_rel *step_rels, size_t *step_sources,
                      size_t total_steps)
{
    datalog89_test_variant_info v;
    datalog89_test_step_info s;
    size_t i;
    size_t j;
    size_t slot;

    slot = 0;
    for (i = 0; i < PLAN_TOTAL_VARIANTS; ++i)
    {
        T_STATUS(datalog89_test_plan_variant_info(eval, i, &v), DATALOG89_OK);
        heads[i] = v.head;
        delta_positions[i] = v.delta_pos;
        idb_flags[i] = (int)v.idb_delta;
        for (j = 0; j < v.nsteps; ++j)
        {
            T_STATUS(datalog89_test_plan_step_info(eval, i, j, &s),
                     DATALOG89_OK);
            step_rels[slot] = s.relation;
            step_sources[slot] = (size_t)s.source;
            slot = slot + 1;
        }
    }
    T_EQ_SIZE(slot, total_steps);
}

static void compare_dumps(const datalog89_rel *a_heads,
                          const datalog89_rel *b_heads, const size_t *a_delta,
                          const size_t *b_delta, const int *a_idb,
                          const int *b_idb, const datalog89_rel *a_rels,
                          const datalog89_rel *b_rels, const size_t *a_sources,
                          const size_t *b_sources, size_t total_steps)
{
    size_t i;

    for (i = 0; i < PLAN_TOTAL_VARIANTS; ++i)
    {
        T_EQ_SIZE(a_heads[i], b_heads[i]);
        T_EQ_SIZE(a_delta[i], b_delta[i]);
        T_EQ_SIZE((size_t)a_idb[i], (size_t)b_idb[i]);
    }
    for (i = 0; i < total_steps; ++i)
    {
        T_EQ_SIZE(a_rels[i], b_rels[i]);
        T_EQ_SIZE(a_sources[i], b_sources[i]);
    }
}

static void test_plan_immutable_and_deterministic(void)
{
    ref_store *store;
    ref_store *second_store;
    datalog89_eval *eval;
    datalog89_eval *second;
    datalog89_rule rules[2];
    datalog89_rule second_rules[2];
    datalog89_term terms[14];
    datalog89_term second_terms[14];
    datalog89_atom atoms[6];
    datalog89_atom second_atoms[6];
    datalog89_rel heads[PLAN_TOTAL_VARIANTS];
    datalog89_rel after_heads[PLAN_TOTAL_VARIANTS];
    size_t delta[PLAN_TOTAL_VARIANTS];
    size_t after_delta[PLAN_TOTAL_VARIANTS];
    int idb[PLAN_TOTAL_VARIANTS];
    int after_idb[PLAN_TOTAL_VARIANTS];
    datalog89_rel rels[12];
    datalog89_rel after_rels[12];
    size_t sources[12];
    size_t after_sources[12];

    store = ref_store_new();
    eval = make_eval(store);
    add_shape_rules(eval, rules, terms, atoms);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    dump_plan(eval, heads, delta, idb, rels, sources, 12);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    dump_plan(eval, after_heads, after_delta, after_idb, after_rels,
              after_sources, 12);
    compare_dumps(heads, after_heads, delta, after_delta, idb, after_idb, rels,
                  after_rels, sources, after_sources, 12);
    second_store = ref_store_new();
    second = make_eval(second_store);
    add_shape_rules(second, second_rules, second_terms, second_atoms);
    T_STATUS(datalog89_eval_run(second), DATALOG89_OK);
    dump_plan(second, after_heads, after_delta, after_idb, after_rels,
              after_sources, 12);
    compare_dumps(heads, after_heads, delta, after_delta, idb, after_idb, rels,
                  after_rels, sources, after_sources, 12);
    datalog89_eval_destroy(second);
    ref_store_free(second_store);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_zero_body_rule(void)
{
    ref_store *store;
    datalog89_eval *eval;
    datalog89_rule rule;
    datalog89_term terms[2];
    datalog89_atom head;
    datalog89_test_variant_info v;
    datalog89_const pair[2];

    store = ref_store_new();
    eval = make_eval(store);
    mk_const(&terms[0], C_A);
    mk_const(&terms[1], C_B);
    mk_atom(&head, R_EDGE, 2, &terms[0]);
    mk_rule(&rule, &head, 0, NULL);
    T_STATUS(datalog89_eval_add_rule(eval, &rule), DATALOG89_OK);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    T_EQ_SIZE(datalog89_test_plan_variant_count(eval), 1);
    T_STATUS(datalog89_test_plan_variant_info(eval, 0, &v), DATALOG89_OK);
    T_EQ_SIZE(v.head, R_EDGE);
    T_EQ_SIZE(v.nsteps, 0);
    T_EQ_SIZE(v.delta_pos, 0);
    T_EQ_SIZE((size_t)v.idb_delta, 0);
    pair[0] = C_A;
    pair[1] = C_B;
    expect_tuple(store, R_EDGE, 2, pair);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

int main(void)
{
    test_plan_cache();
    test_variant_shape();
    test_constant_pattern();
    test_hook_validation();
    test_plan_immutable_and_deterministic();
    test_zero_body_rule();
    if (datalog89_test_failures == 0)
    {
        return 0;
    }
    return 1;
}

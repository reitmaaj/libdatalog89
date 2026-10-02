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
 * p(X,Y) :- q(X,Z), joined(Z,Y). Both rules are nonrecursive, so the plan
 * holds 2 SCCs, 2 variants (one seed each), and 4 steps. */
enum
{
    SHAPE_VARIANTS = 2,
    SHAPE_STEPS = 4
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

static void add_tc_rules(datalog89_eval *eval, datalog89_rule *rules,
                         datalog89_term *terms, datalog89_atom *atoms)
{
    mk_var(&terms[0], V_X);
    mk_var(&terms[1], V_Y);
    mk_var(&terms[2], V_X);
    mk_var(&terms[3], V_Y);
    mk_atom(&atoms[0], R_PATH, 2, &terms[0]);
    mk_atom(&atoms[1], R_EDGE, 2, &terms[2]);
    mk_rule(&rules[0], &atoms[0], 1, &atoms[1]);

    mk_var(&terms[4], V_X);
    mk_var(&terms[5], V_Z);
    mk_var(&terms[6], V_X);
    mk_var(&terms[7], V_Y);
    mk_var(&terms[8], V_Y);
    mk_var(&terms[9], V_Z);
    mk_atom(&atoms[2], R_PATH, 2, &terms[4]);
    mk_atom(&atoms[3], R_PATH, 2, &terms[6]);
    mk_atom(&atoms[4], R_EDGE, 2, &terms[8]);
    mk_rule(&rules[1], &atoms[2], 2, &atoms[3]);

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

static void test_nonrecursive_shape(void)
{
    ref_store *store;
    datalog89_eval *eval;
    datalog89_rule rules[2];
    datalog89_term terms[14];
    datalog89_atom atoms[6];
    datalog89_test_variant_info v;
    datalog89_test_scc_info s;
    datalog89_const pair[2];

    store = ref_store_new();
    eval = make_eval(store);
    T_EQ_SIZE(datalog89_test_plan_builds(eval), 0);
    T_EQ_SIZE(datalog89_test_plan_variant_count(eval), 0);
    T_EQ_SIZE(datalog89_test_plan_scc_count(eval), 0);
    T_STATUS(datalog89_test_plan_variant_info(eval, 0, &v), DATALOG89_EINVAL);
    add_shape_rules(eval, rules, terms, atoms);
    seed1(eval, R_SAME, C_A);
    seed1(eval, R_SAME, C_B);
    seed2(eval, R_Q, C_A, C_B);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    T_EQ_SIZE(datalog89_test_plan_scc_count(eval), 2);
    T_EQ_SIZE(datalog89_test_plan_variant_count(eval), SHAPE_VARIANTS);
    T_STATUS(datalog89_test_plan_scc_info(eval, 0, &s), DATALOG89_OK);
    T_EQ_SIZE(s.first_variant, 0);
    T_EQ_SIZE(s.nvariants, 1);
    T_EQ_SIZE((size_t)s.recursive, 0);
    T_STATUS(datalog89_test_plan_scc_info(eval, 1, &s), DATALOG89_OK);
    T_EQ_SIZE(s.first_variant, 1);
    T_EQ_SIZE(s.nvariants, 1);
    T_EQ_SIZE((size_t)s.recursive, 0);
    T_STATUS(datalog89_test_plan_variant_info(eval, 0, &v), DATALOG89_OK);
    T_EQ_SIZE(v.head, R_JOINED);
    T_EQ_SIZE(v.head_arity, 2);
    T_EQ_SIZE(v.nsteps, 2);
    T_EQ_SIZE(v.delta_pos, 2);
    T_EQ_SIZE((size_t)v.recursive, 0);
    check_step(eval, 0, 0, R_SAME, 1, DATALOG89_TEST_SRC_FULL, 0, 0);
    check_step(eval, 0, 1, R_SAME, 1, DATALOG89_TEST_SRC_FULL, 0, 0);
    T_STATUS(datalog89_test_plan_variant_info(eval, 1, &v), DATALOG89_OK);
    T_EQ_SIZE(v.head, R_P);
    T_EQ_SIZE(v.head_arity, 2);
    T_EQ_SIZE(v.nsteps, 2);
    T_EQ_SIZE(v.delta_pos, 2);
    T_EQ_SIZE((size_t)v.recursive, 0);
    check_step(eval, 1, 0, R_Q, 2, DATALOG89_TEST_SRC_FULL, 0, 0);
    check_step(eval, 1, 1, R_JOINED, 2, DATALOG89_TEST_SRC_FULL, 1, 0);
    pair[0] = C_A;
    pair[1] = C_B;
    expect_tuple(store, R_JOINED, 2, pair);
    pair[0] = C_A;
    pair[1] = C_A;
    expect_tuple(store, R_P, 2, pair);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_recursive_scc(void)
{
    ref_store *store;
    datalog89_eval *eval;
    datalog89_rule rules[2];
    datalog89_term terms[10];
    datalog89_atom atoms[5];
    datalog89_test_variant_info v;
    datalog89_test_scc_info s;
    datalog89_const pair[2];

    store = ref_store_new();
    eval = make_eval(store);
    add_tc_rules(eval, rules, terms, atoms);
    seed2(eval, R_EDGE, C_A, C_B);
    seed2(eval, R_EDGE, C_B, C_C);
    seed2(eval, R_EDGE, C_C, C_D);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    T_EQ_SIZE(datalog89_test_plan_scc_count(eval), 1);
    T_STATUS(datalog89_test_plan_scc_info(eval, 0, &s), DATALOG89_OK);
    T_EQ_SIZE(s.first_variant, 0);
    T_EQ_SIZE(s.nvariants, 3);
    T_EQ_SIZE((size_t)s.recursive, 1);
    T_EQ_SIZE(datalog89_test_plan_variant_count(eval), 3);
    T_STATUS(datalog89_test_plan_variant_info(eval, 0, &v), DATALOG89_OK);
    T_EQ_SIZE(v.head, R_PATH);
    T_EQ_SIZE(v.nsteps, 1);
    T_EQ_SIZE(v.delta_pos, 1);
    T_EQ_SIZE((size_t)v.recursive, 0);
    check_step(eval, 0, 0, R_EDGE, 2, DATALOG89_TEST_SRC_FULL, 0, 0);
    T_STATUS(datalog89_test_plan_variant_info(eval, 1, &v), DATALOG89_OK);
    T_EQ_SIZE(v.head, R_PATH);
    T_EQ_SIZE(v.nsteps, 2);
    T_EQ_SIZE(v.delta_pos, 0);
    T_EQ_SIZE((size_t)v.recursive, 1);
    check_step(eval, 1, 0, R_PATH, 2, DATALOG89_TEST_SRC_DELTA, 0, 0);
    check_step(eval, 1, 1, R_EDGE, 2, DATALOG89_TEST_SRC_FULL, 1, 0);
    T_STATUS(datalog89_test_plan_variant_info(eval, 2, &v), DATALOG89_OK);
    T_EQ_SIZE(v.delta_pos, 2);
    T_EQ_SIZE((size_t)v.recursive, 0);
    check_step(eval, 2, 0, R_PATH, 2, DATALOG89_TEST_SRC_FULL, 0, 0);
    check_step(eval, 2, 1, R_EDGE, 2, DATALOG89_TEST_SRC_FULL, 1, 0);
    expect_count(store, R_PATH, 2, 6);
    pair[0] = C_A;
    pair[1] = C_D;
    expect_tuple(store, R_PATH, 2, pair);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_mutual_scc(void)
{
    ref_store *store;
    datalog89_eval *eval;
    datalog89_rule rules[2];
    datalog89_term terms[4];
    datalog89_atom atoms[4];
    datalog89_test_variant_info v;
    datalog89_test_scc_info s;
    datalog89_const a[1];

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&terms[0], V_X);
    mk_var(&terms[1], V_X);
    mk_atom(&atoms[0], R_P, 1, &terms[0]);
    mk_atom(&atoms[1], R_Q, 1, &terms[1]);
    mk_rule(&rules[0], &atoms[0], 1, &atoms[1]);
    mk_var(&terms[2], V_X);
    mk_var(&terms[3], V_X);
    mk_atom(&atoms[2], R_Q, 1, &terms[2]);
    mk_atom(&atoms[3], R_P, 1, &terms[3]);
    mk_rule(&rules[1], &atoms[2], 1, &atoms[3]);
    T_STATUS(datalog89_eval_add_rule(eval, &rules[0]), DATALOG89_OK);
    T_STATUS(datalog89_eval_add_rule(eval, &rules[1]), DATALOG89_OK);
    seed1(eval, R_P, C_A);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    T_EQ_SIZE(datalog89_test_plan_scc_count(eval), 1);
    T_STATUS(datalog89_test_plan_scc_info(eval, 0, &s), DATALOG89_OK);
    T_EQ_SIZE(s.first_variant, 0);
    T_EQ_SIZE(s.nvariants, 4);
    T_EQ_SIZE((size_t)s.recursive, 1);
    T_EQ_SIZE(datalog89_test_plan_variant_count(eval), 4);
    T_STATUS(datalog89_test_plan_variant_info(eval, 0, &v), DATALOG89_OK);
    T_EQ_SIZE(v.head, R_P);
    T_EQ_SIZE(v.delta_pos, 0);
    T_EQ_SIZE((size_t)v.recursive, 1);
    check_step(eval, 0, 0, R_Q, 1, DATALOG89_TEST_SRC_DELTA, 0, 0);
    T_STATUS(datalog89_test_plan_variant_info(eval, 1, &v), DATALOG89_OK);
    T_EQ_SIZE(v.delta_pos, 1);
    T_EQ_SIZE((size_t)v.recursive, 0);
    T_STATUS(datalog89_test_plan_variant_info(eval, 2, &v), DATALOG89_OK);
    T_EQ_SIZE(v.head, R_Q);
    T_EQ_SIZE(v.delta_pos, 0);
    T_EQ_SIZE((size_t)v.recursive, 1);
    check_step(eval, 2, 0, R_P, 1, DATALOG89_TEST_SRC_DELTA, 0, 0);
    T_STATUS(datalog89_test_plan_variant_info(eval, 3, &v), DATALOG89_OK);
    T_EQ_SIZE(v.delta_pos, 1);
    T_EQ_SIZE((size_t)v.recursive, 0);
    a[0] = C_A;
    expect_tuple(store, R_Q, 1, a);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_scc_chain(void)
{
    ref_store *store;
    datalog89_eval *eval;
    datalog89_rule rules[3];
    datalog89_term terms[6];
    datalog89_atom atoms[6];
    datalog89_test_scc_info s;
    datalog89_test_variant_info v;
    datalog89_const a[1];

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&terms[0], V_X);
    mk_var(&terms[1], V_X);
    mk_atom(&atoms[0], R_R, 1, &terms[0]);
    mk_atom(&atoms[1], R_SAME, 1, &terms[1]);
    mk_rule(&rules[0], &atoms[0], 1, &atoms[1]);
    mk_var(&terms[2], V_X);
    mk_var(&terms[3], V_X);
    mk_atom(&atoms[2], R_Q, 1, &terms[2]);
    mk_atom(&atoms[3], R_R, 1, &terms[3]);
    mk_rule(&rules[1], &atoms[2], 1, &atoms[3]);
    mk_var(&terms[4], V_X);
    mk_var(&terms[5], V_X);
    mk_atom(&atoms[4], R_P, 1, &terms[4]);
    mk_atom(&atoms[5], R_Q, 1, &terms[5]);
    mk_rule(&rules[2], &atoms[4], 1, &atoms[5]);
    T_STATUS(datalog89_eval_add_rule(eval, &rules[0]), DATALOG89_OK);
    T_STATUS(datalog89_eval_add_rule(eval, &rules[1]), DATALOG89_OK);
    T_STATUS(datalog89_eval_add_rule(eval, &rules[2]), DATALOG89_OK);
    seed1(eval, R_SAME, C_A);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    T_EQ_SIZE(datalog89_test_plan_scc_count(eval), 3);
    T_EQ_SIZE(datalog89_test_plan_variant_count(eval), 3);
    T_STATUS(datalog89_test_plan_variant_info(eval, 0, &v), DATALOG89_OK);
    T_EQ_SIZE(v.head, R_R);
    T_STATUS(datalog89_test_plan_variant_info(eval, 1, &v), DATALOG89_OK);
    T_EQ_SIZE(v.head, R_Q);
    T_STATUS(datalog89_test_plan_variant_info(eval, 2, &v), DATALOG89_OK);
    T_EQ_SIZE(v.head, R_P);
    T_STATUS(datalog89_test_plan_scc_info(eval, 0, &s), DATALOG89_OK);
    T_EQ_SIZE((size_t)s.recursive, 0);
    T_STATUS(datalog89_test_plan_scc_info(eval, 1, &s), DATALOG89_OK);
    T_EQ_SIZE((size_t)s.recursive, 0);
    T_STATUS(datalog89_test_plan_scc_info(eval, 2, &s), DATALOG89_OK);
    T_EQ_SIZE((size_t)s.recursive, 0);
    T_EQ_SIZE(ref_store_scan_opens(store), 3);
    a[0] = C_A;
    expect_tuple(store, R_P, 1, a);
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
    check_step(eval, 0, 0, R_SAME, 2, DATALOG89_TEST_SRC_FULL, 0, 1);
    check_step(eval, 0, 1, R_Q, 2, DATALOG89_TEST_SRC_FULL, 1, 0);
    pair[0] = C_A;
    pair[1] = C_A;
    expect_tuple(store, R_JOINED, 2, pair);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

/* p(X,Z) :- q(X,Y), p(Y,Z): the recursive atom sits at body position 1.
 * The delta variant must lead with p and scan q with Y bound, so the full q
 * relation is never rescanned per delta tuple. */
static void test_delta_first(void)
{
    ref_store *store;
    datalog89_eval *eval;
    datalog89_rule rule;
    datalog89_term terms[6];
    datalog89_atom atoms[3];
    datalog89_test_variant_info v;
    datalog89_const pair[2];
    size_t i;

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&terms[0], V_X);
    mk_var(&terms[1], V_Z);
    mk_var(&terms[2], V_X);
    mk_var(&terms[3], V_Y);
    mk_var(&terms[4], V_Y);
    mk_var(&terms[5], V_Z);
    mk_atom(&atoms[0], R_P, 2, &terms[0]);
    mk_atom(&atoms[1], R_Q, 2, &terms[2]);
    mk_atom(&atoms[2], R_P, 2, &terms[4]);
    mk_rule(&rule, &atoms[0], 2, &atoms[1]);
    T_STATUS(datalog89_eval_add_rule(eval, &rule), DATALOG89_OK);
    for (i = 0; i < 40; ++i)
    {
        seed2(eval, R_Q, C_A, (datalog89_const)(1000 + i));
        seed2(eval, R_P, (datalog89_const)(1000 + i),
              (datalog89_const)(2000 + i));
    }
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    T_EQ_SIZE(datalog89_test_plan_variant_count(eval), 2);
    T_STATUS(datalog89_test_plan_variant_info(eval, 0, &v), DATALOG89_OK);
    T_EQ_SIZE(v.head, R_P);
    T_EQ_SIZE(v.delta_pos, 1);
    T_EQ_SIZE((size_t)v.recursive, 1);
    check_step(eval, 0, 0, R_P, 2, DATALOG89_TEST_SRC_DELTA, 0, 0);
    check_step(eval, 0, 1, R_Q, 2, DATALOG89_TEST_SRC_FULL, 1, 0);
    T_STATUS(datalog89_test_plan_variant_info(eval, 1, &v), DATALOG89_OK);
    T_EQ_SIZE(v.delta_pos, 2);
    check_step(eval, 1, 0, R_Q, 2, DATALOG89_TEST_SRC_FULL, 0, 0);
    check_step(eval, 1, 1, R_P, 2, DATALOG89_TEST_SRC_FULL, 1, 0);
    expect_count(store, R_P, 2, 80);
    pair[0] = C_A;
    pair[1] = 2000;
    expect_tuple(store, R_P, 2, pair);
    pair[1] = 2039;
    expect_tuple(store, R_P, 2, pair);
    T_ASSERT(ref_store_scan_nexts(store) < 500);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

/* r(Z,X) :- a(X), b(Y), c(X,Y), d(Y,Z): the greedy picks the atom with the
 * most bound columns, yielding a, c, b, d instead of body order. */
static void test_greedy_order(void)
{
    ref_store *store;
    datalog89_eval *eval;
    datalog89_rule rule;
    datalog89_term terms[8];
    datalog89_atom atoms[5];
    datalog89_const pair[2];

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&terms[0], V_Z);
    mk_var(&terms[1], V_X);
    mk_var(&terms[2], V_X);
    mk_var(&terms[3], V_Y);
    mk_var(&terms[4], V_X);
    mk_var(&terms[5], V_Y);
    mk_var(&terms[6], V_Y);
    mk_var(&terms[7], V_Z);
    mk_atom(&atoms[0], R_P, 2, &terms[0]);
    mk_atom(&atoms[1], R_LEFT, 1, &terms[2]);
    mk_atom(&atoms[2], R_RIGHT, 1, &terms[3]);
    mk_atom(&atoms[3], R_JOINED, 2, &terms[4]);
    mk_atom(&atoms[4], R_SAME, 2, &terms[6]);
    mk_rule(&rule, &atoms[0], 4, &atoms[1]);
    T_STATUS(datalog89_eval_add_rule(eval, &rule), DATALOG89_OK);
    seed1(eval, R_LEFT, C_A);
    seed1(eval, R_RIGHT, C_B);
    seed2(eval, R_JOINED, C_A, C_B);
    seed2(eval, R_SAME, C_B, C_C);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    check_step(eval, 0, 0, R_LEFT, 1, DATALOG89_TEST_SRC_FULL, 0, 0);
    check_step(eval, 0, 1, R_JOINED, 2, DATALOG89_TEST_SRC_FULL, 1, 0);
    check_step(eval, 0, 2, R_RIGHT, 1, DATALOG89_TEST_SRC_FULL, 1, 0);
    check_step(eval, 0, 3, R_SAME, 2, DATALOG89_TEST_SRC_FULL, 1, 0);
    pair[0] = C_C;
    pair[1] = C_A;
    expect_tuple(store, R_P, 2, pair);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

/* v(X) :- w(X) and w(X) :- s(X), t(A), v(X): one SCC. In the w-rule's delta
 * variant the forced v step binds X, then t(A) and s(X) both observe one
 * bound column; the constant tiebreak puts t before s. */
static void test_const_tiebreak(void)
{
    ref_store *store;
    datalog89_eval *eval;
    datalog89_rule rules[2];
    datalog89_term terms[6];
    datalog89_atom atoms[6];
    datalog89_test_variant_info v;
    datalog89_const a[1];

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&terms[0], V_X);
    mk_var(&terms[1], V_X);
    mk_atom(&atoms[0], R_R, 1, &terms[0]);
    mk_atom(&atoms[1], R_Q, 1, &terms[1]);
    mk_rule(&rules[0], &atoms[0], 1, &atoms[1]);
    mk_var(&terms[2], V_X);
    mk_var(&terms[3], V_X);
    mk_const(&terms[4], C_A);
    mk_var(&terms[5], V_X);
    mk_atom(&atoms[2], R_Q, 1, &terms[2]);
    mk_atom(&atoms[3], R_SAME, 1, &terms[3]);
    mk_atom(&atoms[4], R_LEFT, 1, &terms[4]);
    mk_atom(&atoms[5], R_R, 1, &terms[5]);
    mk_rule(&rules[1], &atoms[2], 3, &atoms[3]);
    T_STATUS(datalog89_eval_add_rule(eval, &rules[0]), DATALOG89_OK);
    T_STATUS(datalog89_eval_add_rule(eval, &rules[1]), DATALOG89_OK);
    seed1(eval, R_R, C_A);
    seed1(eval, R_SAME, C_A);
    seed1(eval, R_LEFT, C_A);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    T_EQ_SIZE(datalog89_test_plan_variant_count(eval), 4);
    T_STATUS(datalog89_test_plan_variant_info(eval, 2, &v), DATALOG89_OK);
    T_EQ_SIZE(v.head, R_Q);
    T_EQ_SIZE(v.delta_pos, 2);
    T_EQ_SIZE((size_t)v.recursive, 1);
    check_step(eval, 2, 0, R_R, 1, DATALOG89_TEST_SRC_DELTA, 0, 0);
    check_step(eval, 2, 1, R_LEFT, 1, DATALOG89_TEST_SRC_FULL, 0, 1);
    check_step(eval, 2, 2, R_SAME, 1, DATALOG89_TEST_SRC_FULL, 1, 0);
    a[0] = C_A;
    expect_tuple(store, R_Q, 1, a);
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
    datalog89_test_step_info st;
    datalog89_test_scc_info s;

    store = ref_store_new();
    eval = make_eval(store);
    add_shape_rules(eval, rules, terms, atoms);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    T_STATUS(datalog89_test_plan_variant_info(NULL, 0, &v), DATALOG89_EINVAL);
    T_STATUS(datalog89_test_plan_variant_info(eval, 0, NULL), DATALOG89_EINVAL);
    T_STATUS(datalog89_test_plan_variant_info(eval, SHAPE_VARIANTS, &v),
             DATALOG89_EINVAL);
    T_STATUS(datalog89_test_plan_step_info(NULL, 0, 0, &st), DATALOG89_EINVAL);
    T_STATUS(datalog89_test_plan_step_info(eval, 0, 0, NULL), DATALOG89_EINVAL);
    T_STATUS(datalog89_test_plan_step_info(eval, SHAPE_VARIANTS, 0, &st),
             DATALOG89_EINVAL);
    T_STATUS(datalog89_test_plan_step_info(eval, 0, 2, &st), DATALOG89_EINVAL);
    T_STATUS(datalog89_test_plan_scc_info(NULL, 0, &s), DATALOG89_EINVAL);
    T_STATUS(datalog89_test_plan_scc_info(eval, 0, NULL), DATALOG89_EINVAL);
    T_STATUS(datalog89_test_plan_scc_info(eval, 2, &s), DATALOG89_EINVAL);
    T_EQ_SIZE(datalog89_test_plan_builds(NULL), 0);
    T_EQ_SIZE(datalog89_test_plan_variant_count(NULL), 0);
    T_EQ_SIZE(datalog89_test_plan_scc_count(NULL), 0);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

static void dump_plan(const datalog89_eval *eval, datalog89_rel *heads,
                      size_t *delta_positions, int *recursive_flags,
                      datalog89_rel *step_rels, size_t *step_sources,
                      size_t *scc_first, size_t *scc_counts, int *scc_recursive,
                      size_t total_steps)
{
    datalog89_test_variant_info v;
    datalog89_test_step_info st;
    datalog89_test_scc_info s;
    size_t i;
    size_t j;
    size_t slot;

    slot = 0;
    for (i = 0; i < SHAPE_VARIANTS; ++i)
    {
        T_STATUS(datalog89_test_plan_variant_info(eval, i, &v), DATALOG89_OK);
        heads[i] = v.head;
        delta_positions[i] = v.delta_pos;
        recursive_flags[i] = (int)v.recursive;
        for (j = 0; j < v.nsteps; ++j)
        {
            T_STATUS(datalog89_test_plan_step_info(eval, i, j, &st),
                     DATALOG89_OK);
            step_rels[slot] = st.relation;
            step_sources[slot] = (size_t)st.source;
            slot = slot + 1;
        }
    }
    T_EQ_SIZE(slot, total_steps);
    for (i = 0; i < 2; ++i)
    {
        T_STATUS(datalog89_test_plan_scc_info(eval, i, &s), DATALOG89_OK);
        scc_first[i] = s.first_variant;
        scc_counts[i] = s.nvariants;
        scc_recursive[i] = (int)s.recursive;
    }
}

static void compare_dumps(const datalog89_rel *a_heads,
                          const datalog89_rel *b_heads, const size_t *a_delta,
                          const size_t *b_delta, const int *a_rec,
                          const int *b_rec, const datalog89_rel *a_rels,
                          const datalog89_rel *b_rels, const size_t *a_sources,
                          const size_t *b_sources, const size_t *a_first,
                          const size_t *b_first, const size_t *a_counts,
                          const size_t *b_counts, const int *a_srec,
                          const int *b_srec, size_t total_steps)
{
    size_t i;

    for (i = 0; i < SHAPE_VARIANTS; ++i)
    {
        T_EQ_SIZE(a_heads[i], b_heads[i]);
        T_EQ_SIZE(a_delta[i], b_delta[i]);
        T_EQ_SIZE((size_t)a_rec[i], (size_t)b_rec[i]);
    }
    for (i = 0; i < total_steps; ++i)
    {
        T_EQ_SIZE(a_rels[i], b_rels[i]);
        T_EQ_SIZE(a_sources[i], b_sources[i]);
    }
    for (i = 0; i < 2; ++i)
    {
        T_EQ_SIZE(a_first[i], b_first[i]);
        T_EQ_SIZE(a_counts[i], b_counts[i]);
        T_EQ_SIZE((size_t)a_srec[i], (size_t)b_srec[i]);
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
    datalog89_rel heads[SHAPE_VARIANTS];
    datalog89_rel after_heads[SHAPE_VARIANTS];
    size_t delta[SHAPE_VARIANTS];
    size_t after_delta[SHAPE_VARIANTS];
    int rec[SHAPE_VARIANTS];
    int after_rec[SHAPE_VARIANTS];
    datalog89_rel rels[SHAPE_STEPS];
    datalog89_rel after_rels[SHAPE_STEPS];
    size_t sources[SHAPE_STEPS];
    size_t after_sources[SHAPE_STEPS];
    size_t first[2];
    size_t after_first[2];
    size_t counts[2];
    size_t after_counts[2];
    int srec[2];
    int after_srec[2];

    store = ref_store_new();
    eval = make_eval(store);
    add_shape_rules(eval, rules, terms, atoms);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    dump_plan(eval, heads, delta, rec, rels, sources, first, counts, srec,
              SHAPE_STEPS);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    dump_plan(eval, after_heads, after_delta, after_rec, after_rels,
              after_sources, after_first, after_counts, after_srec,
              SHAPE_STEPS);
    compare_dumps(heads, after_heads, delta, after_delta, rec, after_rec, rels,
                  after_rels, sources, after_sources, first, after_first,
                  counts, after_counts, srec, after_srec, SHAPE_STEPS);
    second_store = ref_store_new();
    second = make_eval(second_store);
    add_shape_rules(second, second_rules, second_terms, second_atoms);
    T_STATUS(datalog89_eval_run(second), DATALOG89_OK);
    dump_plan(second, after_heads, after_delta, after_rec, after_rels,
              after_sources, after_first, after_counts, after_srec,
              SHAPE_STEPS);
    compare_dumps(heads, after_heads, delta, after_delta, rec, after_rec, rels,
                  after_rels, sources, after_sources, first, after_first,
                  counts, after_counts, srec, after_srec, SHAPE_STEPS);
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
    T_EQ_SIZE((size_t)v.recursive, 0);
    pair[0] = C_A;
    pair[1] = C_B;
    expect_tuple(store, R_EDGE, 2, pair);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

/* PL-18: a chain join over 40 body atoms evaluates exactly; the evaluator
 * walks an explicit frame stack, so body length is not bounded by the C
 * call stack. */
static void test_long_body(void)
{
    ref_store *store;
    datalog89_eval *eval;
    datalog89_rule rule;
    datalog89_term head_terms[2];
    datalog89_term body_terms[80];
    datalog89_atom head;
    datalog89_atom body[40];
    datalog89_const pair[2];
    size_t i;

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], (datalog89_var)1);
    mk_var(&head_terms[1], (datalog89_var)41);
    for (i = 0; i < 40; ++i)
    {
        mk_var(&body_terms[2 * i], (datalog89_var)(i + 1));
        mk_var(&body_terms[2 * i + 1], (datalog89_var)(i + 2));
        mk_atom(&body[i], (datalog89_rel)(9000 + i), 2, &body_terms[2 * i]);
    }
    mk_atom(&head, 8999, 2, head_terms);
    mk_rule(&rule, &head, 40, body);
    T_STATUS(datalog89_eval_add_rule(eval, &rule), DATALOG89_OK);
    for (i = 0; i < 40; ++i)
    {
        pair[0] = (datalog89_const)i;
        pair[1] = (datalog89_const)(i + 1);
        T_STATUS(
            datalog89_eval_add_fact(eval, (datalog89_rel)(9000 + i), 2, pair),
            DATALOG89_OK);
    }
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    pair[0] = 0;
    pair[1] = 40;
    expect_tuple(store, 8999, 2, pair);
    expect_count(store, 8999, 2, 1);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

int main(void)
{
    test_plan_cache();
    test_nonrecursive_shape();
    test_recursive_scc();
    test_mutual_scc();
    test_scc_chain();
    test_constant_pattern();
    test_delta_first();
    test_greedy_order();
    test_const_tiebreak();
    test_hook_validation();
    test_plan_immutable_and_deterministic();
    test_zero_body_rule();
    test_long_body();
    if (datalog89_test_failures == 0)
    {
        return 0;
    }
    return 1;
}

/* test_facts.c - fact insertion, set semantics, persistence, nullary. */

#include <datalog89.h>

#include <test.h>

#include "build.h"
#include "expect.h"
#include "ref_store.h"
#include "symbols.h"

int datalog89_test_failures = 0;

static datalog89_eval *make_eval(ref_store *store)
{
    datalog89_eval_config config;
    datalog89_eval *eval;

    config.store = ref_store_datalog89(store);
    eval = NULL;
    T_STATUS(datalog89_eval_create(&config, &eval), DATALOG89_OK);
    return eval;
}

static void test_one_fact(void)
{
    ref_store *store;
    datalog89_eval *eval;
    datalog89_const fact[1];

    store = ref_store_new();
    eval = make_eval(store);
    fact[0] = C_A;
    T_STATUS(datalog89_eval_add_fact(eval, R_P, 1, fact), DATALOG89_OK);
    expect_tuple(store, R_P, 1, fact);
    expect_count(store, R_P, 1, 1);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_duplicate_fact(void)
{
    ref_store *store;
    datalog89_eval *eval;
    datalog89_const fact[1];

    store = ref_store_new();
    eval = make_eval(store);
    fact[0] = C_A;
    T_STATUS(datalog89_eval_add_fact(eval, R_P, 1, fact), DATALOG89_OK);
    T_STATUS(datalog89_eval_add_fact(eval, R_P, 1, fact), DATALOG89_OK);
    expect_count(store, R_P, 1, 1);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_multi_column_fact(void)
{
    ref_store *store;
    datalog89_eval *eval;
    datalog89_const fact[2];

    store = ref_store_new();
    eval = make_eval(store);
    fact[0] = C_A;
    fact[1] = C_B;
    T_STATUS(datalog89_eval_add_fact(eval, R_EDGE, 2, fact), DATALOG89_OK);
    expect_tuple(store, R_EDGE, 2, fact);
    expect_count(store, R_EDGE, 2, 1);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_fact_persists_across_empty_run(void)
{
    ref_store *store;
    datalog89_eval *eval;
    datalog89_const fact[1];

    store = ref_store_new();
    eval = make_eval(store);
    fact[0] = C_A;
    T_STATUS(datalog89_eval_add_fact(eval, R_P, 1, fact), DATALOG89_OK);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    expect_tuple(store, R_P, 1, fact);
    expect_count(store, R_P, 1, 1);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_fact_between_runs(void)
{
    ref_store *store;
    datalog89_eval *eval;
    datalog89_term head_terms[1];
    datalog89_term body_terms[1];
    datalog89_atom head;
    datalog89_atom body[1];
    datalog89_rule rule;
    datalog89_const fact[1];

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(datalog89_eval_add_rule(eval, &rule), DATALOG89_OK);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    expect_count(store, R_P, 1, 0);
    fact[0] = C_A;
    T_STATUS(datalog89_eval_add_fact(eval, R_Q, 1, fact), DATALOG89_OK);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    expect_tuple(store, R_P, 1, fact);
    expect_count(store, R_P, 1, 1);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_nullary_fact(void)
{
    ref_store *store;
    datalog89_eval *eval;
    datalog89_term head_terms[1];
    datalog89_atom head;
    datalog89_rule rule;

    store = ref_store_new();
    eval = make_eval(store);
    T_STATUS(datalog89_eval_add_fact(eval, R_FLAG, 0, NULL), DATALOG89_OK);
    T_STATUS(datalog89_eval_add_fact(eval, R_FLAG, 0, NULL), DATALOG89_OK);
    expect_count(store, R_FLAG, 0, 1);
    mk_const(&head_terms[0], C_A);
    mk_atom(&head, R_P, 1, head_terms);
    mk_rule(&rule, &head, 0, NULL);
    T_STATUS(datalog89_eval_add_rule(eval, &rule), DATALOG89_OK);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_sparse_identifiers(void)
{
    ref_store *store;
    datalog89_eval *eval;
    datalog89_term head_terms[1];
    datalog89_term body_terms[1];
    datalog89_atom head;
    datalog89_atom body[1];
    datalog89_rule rule;
    datalog89_const fact[1];
    datalog89_rel sparse_rel;

    sparse_rel = 1000003;
    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, sparse_rel, 1, head_terms);
    mk_atom(&body[0], DATALOG89_ULMAX, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(datalog89_eval_add_rule(eval, &rule), DATALOG89_OK);
    fact[0] = 0;
    T_STATUS(datalog89_eval_add_fact(eval, DATALOG89_ULMAX, 1, fact),
             DATALOG89_OK);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    expect_tuple(store, sparse_rel, 1, fact);
    expect_count(store, sparse_rel, 1, 1);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

int main(void)
{
    test_one_fact();
    test_duplicate_fact();
    test_multi_column_fact();
    test_fact_persists_across_empty_run();
    test_fact_between_runs();
    test_nullary_fact();
    test_sparse_identifiers();
    if (datalog89_test_failures == 0)
    {
        return 0;
    }
    return 1;
}

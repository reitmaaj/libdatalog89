/* test_rules.c - rule copying, ownership, and validation. */

#include <string.h>

#include <dl89.h>

#include <test.h>

#include "build.h"
#include "expect.h"
#include "ref_store.h"
#include "symbols.h"

int dl89_test_failures = 0;

static dl89_eval *make_eval(ref_store *store)
{
    dl89_eval_config config;
    dl89_eval *eval;

    config.store = ref_store_dl89(store);
    eval = NULL;
    T_STATUS(dl89_eval_create(&config, &eval), DL89_OK);
    return eval;
}

static void test_deep_copy(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_term body_terms[1];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;
    dl89_const seed[1];
    dl89_const want[1];
    size_t i;

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    for (i = 0; i < sizeof(head_terms); ++i)
    {
        ((unsigned char *)head_terms)[i] = 0xAA;
    }
    for (i = 0; i < sizeof(body_terms); ++i)
    {
        ((unsigned char *)body_terms)[i] = 0xAA;
    }
    for (i = 0; i < sizeof(head); ++i)
    {
        ((unsigned char *)&head)[i] = 0xAA;
    }
    for (i = 0; i < sizeof(body); ++i)
    {
        ((unsigned char *)body)[i] = 0xAA;
    }
    for (i = 0; i < sizeof(rule); ++i)
    {
        ((unsigned char *)&rule)[i] = 0xAA;
    }
    seed[0] = C_A;
    want[0] = C_A;
    T_STATUS(dl89_eval_add_fact(eval, R_Q, 1, seed), DL89_OK);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    expect_tuple(store, R_P, 1, want);
    expect_count(store, R_P, 1, 1);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_independent_copies(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_term body_terms[1];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;
    dl89_const seed[1];
    dl89_const a[1];

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    mk_atom(&head, R_R, 1, head_terms);
    mk_atom(&body[0], R_P, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    seed[0] = C_A;
    a[0] = C_A;
    T_STATUS(dl89_eval_add_fact(eval, R_Q, 1, seed), DL89_OK);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    expect_tuple(store, R_P, 1, a);
    expect_tuple(store, R_R, 1, a);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_destroy_releases(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[2];
    dl89_term body_terms[2];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;
    int i;

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], V_X);
    mk_const(&head_terms[1], C_A);
    mk_var(&body_terms[0], V_X);
    mk_const(&body_terms[1], C_B);
    mk_atom(&head, R_P, 2, head_terms);
    mk_atom(&body[0], R_Q, 2, body_terms);
    mk_rule(&rule, &head, 1, body);
    for (i = 0; i < 100; ++i)
    {
        T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    }
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_invalid_head_kind(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_atom head;

    store = ref_store_new();
    eval = make_eval(store);
    head_terms[0].kind = 99;
    mk_atom(&head, R_P, 1, head_terms);
    {
        dl89_rule rule;
        mk_rule(&rule, &head, 0, NULL);
        T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_EPROGRAM);
    }
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    expect_count(store, R_P, 1, 0);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_invalid_body_kind(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_term body_terms[1];
    dl89_atom head;
    dl89_atom body[1];

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], V_X);
    body_terms[0].kind = 0;
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    {
        dl89_rule rule;
        mk_rule(&rule, &head, 1, body);
        T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_EPROGRAM);
    }
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_unsafe_head_var(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_term body_terms[1];
    dl89_atom head;
    dl89_atom body[1];

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_Y);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    {
        dl89_rule rule;
        mk_rule(&rule, &head, 1, body);
        T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_EPROGRAM);
    }
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_partially_unsafe_head(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[2];
    dl89_term body_terms[1];
    dl89_atom head;
    dl89_atom body[1];

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], V_X);
    mk_var(&head_terms[1], V_Y);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_P, 2, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    {
        dl89_rule rule;
        mk_rule(&rule, &head, 1, body);
        T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_EPROGRAM);
    }
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_variable_free_rule(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_term body_terms[1];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;
    dl89_const seed[1];
    dl89_const want[1];

    store = ref_store_new();
    eval = make_eval(store);
    mk_const(&head_terms[0], C_A);
    mk_var(&body_terms[0], V_Y);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    seed[0] = C_B;
    want[0] = C_A;
    T_STATUS(dl89_eval_add_fact(eval, R_Q, 1, seed), DL89_OK);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    expect_tuple(store, R_P, 1, want);
    expect_count(store, R_P, 1, 1);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_ground_fact(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_atom head;
    dl89_rule rule;
    dl89_const want[1];

    store = ref_store_new();
    eval = make_eval(store);
    mk_const(&head_terms[0], C_A);
    mk_atom(&head, R_P, 1, head_terms);
    mk_rule(&rule, &head, 0, NULL);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    want[0] = C_A;
    expect_tuple(store, R_P, 1, want);
    expect_count(store, R_P, 1, 1);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_nonground_zero_body(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_atom head;
    dl89_rule rule;

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], V_X);
    mk_atom(&head, R_P, 1, head_terms);
    mk_rule(&rule, &head, 0, NULL);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_EPROGRAM);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_repeated_variable_ok(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_term body_terms[2];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_var(&body_terms[1], V_X);
    mk_atom(&head, R_SAME, 1, head_terms);
    mk_atom(&body[0], R_EDGE, 2, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_arity_conflict_in_rule(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_term body_terms[2];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_var(&body_terms[1], V_Y);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_P, 2, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_EPROGRAM);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_arity_conflict_across_rules(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_term body_terms[1];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;
    dl89_const seed[1];
    dl89_const want[1];

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    {
        dl89_term two_terms[2];
        dl89_atom head2;
        dl89_atom body2[1];
        dl89_rule rule2;
        mk_var(&two_terms[0], V_X);
        mk_var(&two_terms[1], V_Y);
        mk_atom(&head2, R_P, 2, two_terms);
        mk_atom(&body2[0], R_Q, 1, body_terms);
        mk_rule(&rule2, &head2, 1, body2);
        T_STATUS(dl89_eval_add_rule(eval, &rule2), DL89_EPROGRAM);
    }
    seed[0] = C_A;
    want[0] = C_A;
    T_STATUS(dl89_eval_add_fact(eval, R_Q, 1, seed), DL89_OK);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    expect_tuple(store, R_P, 1, want);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_arity_conflict_add_fact(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[2];
    dl89_term body_terms[1];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;
    dl89_const fact1[1];
    dl89_const fact2[2];

    store = ref_store_new();
    eval = make_eval(store);
    fact1[0] = C_A;
    fact2[0] = C_A;
    fact2[1] = C_B;
    T_STATUS(dl89_eval_add_fact(eval, R_P, 1, fact1), DL89_OK);
    T_STATUS(dl89_eval_add_fact(eval, R_P, 2, fact2), DL89_EPROGRAM);
    mk_var(&head_terms[0], V_X);
    mk_var(&head_terms[1], V_Y);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_P, 2, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_EPROGRAM);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_zero_identifiers(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_term body_terms[1];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;
    dl89_const zero[1];
    dl89_const want[1];

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], 0);
    mk_var(&body_terms[0], 0);
    mk_atom(&head, 0, 1, head_terms);
    mk_atom(&body[0], 0, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    zero[0] = 0;
    want[0] = 0;
    T_STATUS(dl89_eval_add_fact(eval, 0, 1, zero), DL89_OK);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    expect_tuple(store, 0, 1, want);
    expect_count(store, 0, 1, 1);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_max_identifiers(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_term body_terms[1];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;
    dl89_const seed[1];
    dl89_const want[1];

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], DL89_ULMAX);
    mk_var(&body_terms[0], DL89_ULMAX);
    mk_atom(&head, DL89_ULMAX, 1, head_terms);
    mk_atom(&body[0], DL89_ULMAX, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    seed[0] = DL89_ULMAX;
    want[0] = DL89_ULMAX;
    T_STATUS(dl89_eval_add_fact(eval, DL89_ULMAX, 1, seed), DL89_OK);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    expect_tuple(store, DL89_ULMAX, 1, want);
    expect_count(store, DL89_ULMAX, 1, 1);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

int main(void)
{
    test_deep_copy();
    test_independent_copies();
    test_destroy_releases();
    test_invalid_head_kind();
    test_invalid_body_kind();
    test_unsafe_head_var();
    test_partially_unsafe_head();
    test_variable_free_rule();
    test_ground_fact();
    test_nonground_zero_body();
    test_repeated_variable_ok();
    test_arity_conflict_in_rule();
    test_arity_conflict_across_rules();
    test_arity_conflict_add_fact();
    test_zero_identifiers();
    test_max_identifiers();
    if (dl89_test_failures == 0)
    {
        return 0;
    }
    return 1;
}

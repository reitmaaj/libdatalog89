/* test_fixedpoint.c - idempotence, reruns, monotonicity, nullary recursion. */

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

static void seed2(dl89_eval *eval, dl89_rel relation, dl89_const a,
                  dl89_const b)
{
    dl89_const tuple[2];

    tuple[0] = a;
    tuple[1] = b;
    T_STATUS(dl89_eval_add_fact(eval, relation, 2, tuple), DL89_OK);
}

static void add_tc_rules(dl89_eval *eval, dl89_rule *rules, dl89_term *h1,
                         dl89_term *b1, dl89_term *h2, dl89_term *b2,
                         dl89_atom *a1, dl89_atom *a2, dl89_atom *a3,
                         dl89_atom *a4)
{
    mk_var(&h1[0], V_X);
    mk_var(&h1[1], V_Y);
    mk_var(&b1[0], V_X);
    mk_var(&b1[1], V_Y);
    mk_atom(a1, R_PATH, 2, h1);
    mk_atom(a2, R_EDGE, 2, b1);
    mk_rule(&rules[0], a1, 1, a2);

    mk_var(&h2[0], V_X);
    mk_var(&h2[1], V_Z);
    mk_var(&b2[0], V_X);
    mk_var(&b2[1], V_Y);
    mk_var(&b2[2], V_Y);
    mk_var(&b2[3], V_Z);
    mk_atom(a3, R_PATH, 2, h2);
    mk_atom(&a4[0], R_PATH, 2, &b2[0]);
    mk_atom(&a4[1], R_EDGE, 2, &b2[2]);
    mk_rule(&rules[1], a3, 2, a4);

    T_STATUS(dl89_eval_add_rule(eval, &rules[0]), DL89_OK);
    T_STATUS(dl89_eval_add_rule(eval, &rules[1]), DL89_OK);
}

static void test_idempotent_second_run(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_rule rules[2];
    dl89_term h1[2];
    dl89_term b1[2];
    dl89_term h2[2];
    dl89_term b2[4];
    dl89_atom a1;
    dl89_atom a2;
    dl89_atom a3;
    dl89_atom a4[2];
    size_t total;

    store = ref_store_new();
    eval = make_eval(store);
    add_tc_rules(eval, rules, h1, b1, h2, b2, &a1, &a2, &a3, a4);
    seed2(eval, R_EDGE, C_A, C_B);
    seed2(eval, R_EDGE, C_B, C_C);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    total = ref_store_total(store);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    T_EQ_SIZE(ref_store_total(store), total);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_new_fact_after_closure(void)
{
    ref_store *store;
    dl89_eval *eval;
    ref_store *full_store;
    dl89_eval *full_eval;
    dl89_rule rules[2];
    dl89_term h1[2];
    dl89_term b1[2];
    dl89_term h2[2];
    dl89_term b2[4];
    dl89_atom a1;
    dl89_atom a2;
    dl89_atom a3;
    dl89_atom a4[2];
    dl89_rule full_rules[2];
    dl89_term fh1[2];
    dl89_term fb1[2];
    dl89_term fh2[2];
    dl89_term fb2[4];
    dl89_atom fa1;
    dl89_atom fa2;
    dl89_atom fa3;
    dl89_atom fa4[2];

    store = ref_store_new();
    eval = make_eval(store);
    add_tc_rules(eval, rules, h1, b1, h2, b2, &a1, &a2, &a3, a4);
    seed2(eval, R_EDGE, C_A, C_B);
    seed2(eval, R_EDGE, C_B, C_C);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    seed2(eval, R_EDGE, C_C, C_D);
    T_STATUS(dl89_eval_run(eval), DL89_OK);

    full_store = ref_store_new();
    full_eval = make_eval(full_store);
    add_tc_rules(full_eval, full_rules, fh1, fb1, fh2, fb2, &fa1, &fa2, &fa3,
                 fa4);
    seed2(full_eval, R_EDGE, C_A, C_B);
    seed2(full_eval, R_EDGE, C_B, C_C);
    seed2(full_eval, R_EDGE, C_C, C_D);
    T_STATUS(dl89_eval_run(full_eval), DL89_OK);

    T_ASSERT(ref_store_equals(store, full_store) == 1);
    dl89_eval_destroy(eval);
    ref_store_free(store);
    dl89_eval_destroy(full_eval);
    ref_store_free(full_store);
}

static void test_new_rule_after_closure(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_term body_terms[1];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;
    dl89_const a[1];

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    a[0] = C_A;
    T_STATUS(dl89_eval_add_fact(eval, R_Q, 1, a), DL89_OK);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    expect_tuple(store, R_P, 1, a);
    mk_atom(&head, R_R, 1, head_terms);
    mk_atom(&body[0], R_P, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    expect_tuple(store, R_R, 1, a);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_monotone(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_term body_terms[1];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;
    dl89_const a[1];
    dl89_const b[1];
    size_t before;

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    a[0] = C_A;
    T_STATUS(dl89_eval_add_fact(eval, R_Q, 1, a), DL89_OK);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    before = ref_store_total(store);
    b[0] = C_B;
    T_STATUS(dl89_eval_add_fact(eval, R_Q, 1, b), DL89_OK);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    T_ASSERT(ref_store_total(store) >= before);
    expect_tuple(store, R_P, 1, a);
    expect_tuple(store, R_P, 1, b);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_nullary_recursion(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;

    store = ref_store_new();
    eval = make_eval(store);
    mk_atom(&head, R_P, 0, NULL);
    mk_atom(&body[0], R_Q, 0, NULL);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    mk_atom(&head, R_Q, 0, NULL);
    mk_atom(&body[0], R_P, 0, NULL);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    T_STATUS(dl89_eval_add_fact(eval, R_P, 0, NULL), DL89_OK);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    expect_count(store, R_P, 0, 1);
    expect_count(store, R_Q, 0, 1);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

int main(void)
{
    test_idempotent_second_run();
    test_new_fact_after_closure();
    test_new_rule_after_closure();
    test_monotone();
    test_nullary_recursion();
    if (dl89_test_failures == 0)
    {
        return 0;
    }
    return 1;
}

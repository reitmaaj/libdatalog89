/* test_eval.c - nonrecursive joins, unification, constants, nullary bodies. */

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

static void seed1(dl89_eval *eval, dl89_rel relation, dl89_const value)
{
    dl89_const tuple[1];

    tuple[0] = value;
    T_STATUS(dl89_eval_add_fact(eval, relation, 1, tuple), DL89_OK);
}

static void seed2(dl89_eval *eval, dl89_rel relation, dl89_const a,
                  dl89_const b)
{
    dl89_const tuple[2];

    tuple[0] = a;
    tuple[1] = b;
    T_STATUS(dl89_eval_add_fact(eval, relation, 2, tuple), DL89_OK);
}

static void test_unary_copy(void)
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

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    seed1(eval, R_Q, C_A);
    seed1(eval, R_Q, C_B);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    a[0] = C_A;
    b[0] = C_B;
    expect_tuple(store, R_P, 1, a);
    expect_tuple(store, R_P, 1, b);
    expect_count(store, R_P, 1, 2);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_constant_filtering(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_term body_terms[2];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;
    dl89_const a[1];
    dl89_const d[1];

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_const(&body_terms[1], C_B);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_EDGE, 2, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    seed2(eval, R_EDGE, C_A, C_B);
    seed2(eval, R_EDGE, C_A, C_C);
    seed2(eval, R_EDGE, C_D, C_B);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    a[0] = C_A;
    d[0] = C_D;
    expect_tuple(store, R_P, 1, a);
    expect_tuple(store, R_P, 1, d);
    expect_count(store, R_P, 1, 2);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_constant_in_head(void)
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
    mk_const(&head_terms[0], C_A);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    seed1(eval, R_Q, C_B);
    seed1(eval, R_Q, C_C);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    a[0] = C_A;
    expect_tuple(store, R_P, 1, a);
    expect_count(store, R_P, 1, 1);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_repeated_variable(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_term body_terms[2];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;
    dl89_const a[1];
    dl89_const b[1];

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_var(&body_terms[1], V_X);
    mk_atom(&head, R_SAME, 1, head_terms);
    mk_atom(&body[0], R_EDGE, 2, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    seed2(eval, R_EDGE, C_A, C_A);
    seed2(eval, R_EDGE, C_A, C_B);
    seed2(eval, R_EDGE, C_B, C_B);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    a[0] = C_A;
    b[0] = C_B;
    expect_tuple(store, R_SAME, 1, a);
    expect_tuple(store, R_SAME, 1, b);
    expect_count(store, R_SAME, 1, 2);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void build_join_rule(dl89_rule *rule, dl89_term *head_terms,
                            dl89_term *body_terms, dl89_atom *head,
                            dl89_atom *body)
{
    mk_var(&head_terms[0], V_X);
    mk_var(&head_terms[1], V_Z);
    mk_var(&body_terms[0], V_X);
    mk_var(&body_terms[1], V_Y);
    mk_var(&body_terms[2], V_Y);
    mk_var(&body_terms[3], V_Z);
    mk_atom(head, R_JOINED, 2, head_terms);
    mk_atom(&body[0], R_LEFT, 2, &body_terms[0]);
    mk_atom(&body[1], R_RIGHT, 2, &body_terms[2]);
    mk_rule(rule, head, 2, body);
}

static void test_two_way_join(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[2];
    dl89_term body_terms[4];
    dl89_atom head;
    dl89_atom body[2];
    dl89_rule rule;
    dl89_const ad[2];
    dl89_const ae[2];

    store = ref_store_new();
    eval = make_eval(store);
    build_join_rule(&rule, head_terms, body_terms, &head, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    seed2(eval, R_LEFT, C_A, C_B);
    seed2(eval, R_LEFT, C_A, C_C);
    seed2(eval, R_RIGHT, C_B, C_D);
    seed2(eval, R_RIGHT, C_C, C_E);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    ad[0] = C_A;
    ad[1] = C_D;
    ae[0] = C_A;
    ae[1] = C_E;
    expect_tuple(store, R_JOINED, 2, ad);
    expect_tuple(store, R_JOINED, 2, ae);
    expect_count(store, R_JOINED, 2, 2);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_failed_join(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[2];
    dl89_term body_terms[4];
    dl89_atom head;
    dl89_atom body[2];
    dl89_rule rule;

    store = ref_store_new();
    eval = make_eval(store);
    build_join_rule(&rule, head_terms, body_terms, &head, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    seed2(eval, R_LEFT, C_A, C_B);
    seed2(eval, R_RIGHT, C_C, C_D);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    expect_count(store, R_JOINED, 2, 0);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_cross_product(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[2];
    dl89_term body_terms[2];
    dl89_atom head;
    dl89_atom body[2];
    dl89_rule rule;
    dl89_const ac[2];
    dl89_const ad[2];
    dl89_const bc[2];
    dl89_const bd[2];

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], V_X);
    mk_var(&head_terms[1], V_Y);
    mk_var(&body_terms[0], V_X);
    mk_var(&body_terms[1], V_Y);
    mk_atom(&head, R_JOINED, 2, head_terms);
    mk_atom(&body[0], R_P, 1, &body_terms[0]);
    mk_atom(&body[1], R_Q, 1, &body_terms[1]);
    mk_rule(&rule, &head, 2, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    seed1(eval, R_P, C_A);
    seed1(eval, R_P, C_B);
    seed1(eval, R_Q, C_C);
    seed1(eval, R_Q, C_D);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    ac[0] = C_A;
    ac[1] = C_C;
    ad[0] = C_A;
    ad[1] = C_D;
    bc[0] = C_B;
    bc[1] = C_C;
    bd[0] = C_B;
    bd[1] = C_D;
    expect_tuple(store, R_JOINED, 2, ac);
    expect_tuple(store, R_JOINED, 2, ad);
    expect_tuple(store, R_JOINED, 2, bc);
    expect_tuple(store, R_JOINED, 2, bd);
    expect_count(store, R_JOINED, 2, 4);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_three_way_intersection(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_term body_terms[4];
    dl89_atom head;
    dl89_atom body[3];
    dl89_rule rule;
    dl89_const a[1];

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_var(&body_terms[1], V_X);
    mk_var(&body_terms[2], V_X);
    mk_const(&body_terms[3], C_E);
    mk_atom(&head, R_R, 1, head_terms);
    mk_atom(&body[0], R_P, 1, &body_terms[0]);
    mk_atom(&body[1], R_Q, 1, &body_terms[1]);
    mk_atom(&body[2], R_EDGE, 2, &body_terms[2]);
    mk_rule(&rule, &head, 3, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    seed1(eval, R_P, C_A);
    seed1(eval, R_P, C_B);
    seed1(eval, R_Q, C_A);
    seed1(eval, R_Q, C_C);
    seed2(eval, R_EDGE, C_A, C_E);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    a[0] = C_A;
    expect_tuple(store, R_R, 1, a);
    expect_count(store, R_R, 1, 1);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_repeated_body_relation(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[2];
    dl89_term body_terms[4];
    dl89_atom head;
    dl89_atom body[2];
    dl89_rule rule;
    dl89_const ac[2];
    dl89_const ad[2];
    dl89_const be[2];

    store = ref_store_new();
    eval = make_eval(store);
    mk_var(&head_terms[0], V_X);
    mk_var(&head_terms[1], V_Z);
    mk_var(&body_terms[0], V_X);
    mk_var(&body_terms[1], V_Y);
    mk_var(&body_terms[2], V_Y);
    mk_var(&body_terms[3], V_Z);
    mk_atom(&head, R_PATH, 2, head_terms);
    mk_atom(&body[0], R_EDGE, 2, &body_terms[0]);
    mk_atom(&body[1], R_EDGE, 2, &body_terms[2]);
    mk_rule(&rule, &head, 2, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    seed2(eval, R_EDGE, C_A, C_B);
    seed2(eval, R_EDGE, C_B, C_C);
    seed2(eval, R_EDGE, C_B, C_D);
    seed2(eval, R_EDGE, C_C, C_E);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    ac[0] = C_A;
    ac[1] = C_C;
    ad[0] = C_A;
    ad[1] = C_D;
    be[0] = C_B;
    be[1] = C_E;
    expect_tuple(store, R_PATH, 2, ac);
    expect_tuple(store, R_PATH, 2, ad);
    expect_tuple(store, R_PATH, 2, be);
    expect_count(store, R_PATH, 2, 3);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_duplicate_derivations(void)
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
    mk_atom(&body[0], R_R, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    seed1(eval, R_Q, C_A);
    seed1(eval, R_R, C_A);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    a[0] = C_A;
    expect_tuple(store, R_P, 1, a);
    expect_count(store, R_P, 1, 1);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_nullary_body(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;
    dl89_const a[1];

    store = ref_store_new();
    eval = make_eval(store);
    mk_const(&head_terms[0], C_A);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_FLAG, 0, NULL);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    expect_count(store, R_P, 1, 0);
    T_STATUS(dl89_eval_add_fact(eval, R_FLAG, 0, NULL), DL89_OK);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    a[0] = C_A;
    expect_tuple(store, R_P, 1, a);
    expect_count(store, R_P, 1, 1);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_derived_nullary(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_term body_terms[1];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;

    store = ref_store_new();
    eval = make_eval(store);
    mk_const(&head_terms[0], C_A);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_FLAG, 0, NULL);
    mk_atom(&body[0], R_P, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    seed1(eval, R_P, C_A);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    expect_count(store, R_FLAG, 0, 1);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

int main(void)
{
    test_unary_copy();
    test_constant_filtering();
    test_constant_in_head();
    test_repeated_variable();
    test_two_way_join();
    test_failed_join();
    test_cross_product();
    test_three_way_intersection();
    test_repeated_body_relation();
    test_duplicate_derivations();
    test_nullary_body();
    test_derived_nullary();
    if (dl89_test_failures == 0)
    {
        return 0;
    }
    return 1;
}

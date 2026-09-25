/* test_recursive.c - recursive fixed-point semantics. */

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

static void build_tc(dl89_rule *rules, dl89_term *base_head,
                     dl89_term *base_body, dl89_term *rec_head,
                     dl89_term *rec_body, dl89_atom *base_head_atom,
                     dl89_atom *base_body_atom, dl89_atom *rec_head_atom,
                     dl89_atom *rec_body_atoms)
{
    mk_var(&base_head[0], V_X);
    mk_var(&base_head[1], V_Y);
    mk_var(&base_body[0], V_X);
    mk_var(&base_body[1], V_Y);
    mk_atom(base_head_atom, R_PATH, 2, base_head);
    mk_atom(base_body_atom, R_EDGE, 2, base_body);
    mk_rule(&rules[0], base_head_atom, 1, base_body_atom);

    mk_var(&rec_head[0], V_X);
    mk_var(&rec_head[1], V_Z);
    mk_var(&rec_body[0], V_X);
    mk_var(&rec_body[1], V_Y);
    mk_var(&rec_body[2], V_Y);
    mk_var(&rec_body[3], V_Z);
    mk_atom(rec_head_atom, R_PATH, 2, rec_head);
    mk_atom(&rec_body_atoms[0], R_PATH, 2, &rec_body[0]);
    mk_atom(&rec_body_atoms[1], R_EDGE, 2, &rec_body[2]);
    mk_rule(&rules[1], rec_head_atom, 2, rec_body_atoms);
}

static void expect_path(ref_store *store, dl89_const a, dl89_const b)
{
    dl89_const tuple[2];

    tuple[0] = a;
    tuple[1] = b;
    expect_tuple(store, R_PATH, 2, tuple);
}

static void test_chain_closure(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_rule rules[2];
    dl89_term base_head[2];
    dl89_term base_body[2];
    dl89_term rec_head[2];
    dl89_term rec_body[4];
    dl89_atom base_head_atom;
    dl89_atom base_body_atom;
    dl89_atom rec_head_atom;
    dl89_atom rec_body_atoms[2];

    store = ref_store_new();
    eval = make_eval(store);
    build_tc(rules, base_head, base_body, rec_head, rec_body, &base_head_atom,
             &base_body_atom, &rec_head_atom, rec_body_atoms);
    T_STATUS(dl89_eval_add_rule(eval, &rules[0]), DL89_OK);
    T_STATUS(dl89_eval_add_rule(eval, &rules[1]), DL89_OK);
    seed2(eval, R_EDGE, C_A, C_B);
    seed2(eval, R_EDGE, C_B, C_C);
    seed2(eval, R_EDGE, C_C, C_D);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    expect_path(store, C_A, C_B);
    expect_path(store, C_B, C_C);
    expect_path(store, C_C, C_D);
    expect_path(store, C_A, C_C);
    expect_path(store, C_B, C_D);
    expect_path(store, C_A, C_D);
    expect_count(store, R_PATH, 2, 6);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_multiple_rounds(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_rule rules[2];
    dl89_term base_head[2];
    dl89_term base_body[2];
    dl89_term rec_head[2];
    dl89_term rec_body[4];
    dl89_atom base_head_atom;
    dl89_atom base_body_atom;
    dl89_atom rec_head_atom;
    dl89_atom rec_body_atoms[2];

    store = ref_store_new();
    eval = make_eval(store);
    build_tc(rules, base_head, base_body, rec_head, rec_body, &base_head_atom,
             &base_body_atom, &rec_head_atom, rec_body_atoms);
    T_STATUS(dl89_eval_add_rule(eval, &rules[0]), DL89_OK);
    T_STATUS(dl89_eval_add_rule(eval, &rules[1]), DL89_OK);
    seed2(eval, R_EDGE, C_A, C_B);
    seed2(eval, R_EDGE, C_B, C_C);
    seed2(eval, R_EDGE, C_C, C_D);
    seed2(eval, R_EDGE, C_D, C_E);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    expect_path(store, C_A, C_E);
    expect_count(store, R_PATH, 2, 10);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_cyclic_graph(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_rule rules[2];
    dl89_term base_head[2];
    dl89_term base_body[2];
    dl89_term rec_head[2];
    dl89_term rec_body[4];
    dl89_atom base_head_atom;
    dl89_atom base_body_atom;
    dl89_atom rec_head_atom;
    dl89_atom rec_body_atoms[2];

    store = ref_store_new();
    eval = make_eval(store);
    build_tc(rules, base_head, base_body, rec_head, rec_body, &base_head_atom,
             &base_body_atom, &rec_head_atom, rec_body_atoms);
    T_STATUS(dl89_eval_add_rule(eval, &rules[0]), DL89_OK);
    T_STATUS(dl89_eval_add_rule(eval, &rules[1]), DL89_OK);
    seed2(eval, R_EDGE, C_A, C_B);
    seed2(eval, R_EDGE, C_B, C_A);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    expect_path(store, C_A, C_B);
    expect_path(store, C_B, C_A);
    expect_path(store, C_A, C_A);
    expect_path(store, C_B, C_B);
    expect_count(store, R_PATH, 2, 4);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_self_loop(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_rule rules[2];
    dl89_term base_head[2];
    dl89_term base_body[2];
    dl89_term rec_head[2];
    dl89_term rec_body[4];
    dl89_atom base_head_atom;
    dl89_atom base_body_atom;
    dl89_atom rec_head_atom;
    dl89_atom rec_body_atoms[2];

    store = ref_store_new();
    eval = make_eval(store);
    build_tc(rules, base_head, base_body, rec_head, rec_body, &base_head_atom,
             &base_body_atom, &rec_head_atom, rec_body_atoms);
    T_STATUS(dl89_eval_add_rule(eval, &rules[0]), DL89_OK);
    T_STATUS(dl89_eval_add_rule(eval, &rules[1]), DL89_OK);
    seed2(eval, R_EDGE, C_A, C_A);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    expect_path(store, C_A, C_A);
    expect_count(store, R_PATH, 2, 1);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_mutual_recursion(void)
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
    mk_atom(&head, R_Q, 1, head_terms);
    mk_atom(&body[0], R_R, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    mk_atom(&head, R_R, 1, head_terms);
    mk_atom(&body[0], R_P, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    a[0] = C_A;
    T_STATUS(dl89_eval_add_fact(eval, R_Q, 1, a), DL89_OK);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    expect_tuple(store, R_P, 1, a);
    expect_tuple(store, R_Q, 1, a);
    expect_tuple(store, R_R, 1, a);
    expect_count(store, R_P, 1, 1);
    expect_count(store, R_Q, 1, 1);
    expect_count(store, R_R, 1, 1);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_recursive_branching(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_rule rules[2];
    dl89_term base_head[2];
    dl89_term base_body[2];
    dl89_term rec_head[2];
    dl89_term rec_body[4];
    dl89_atom base_head_atom;
    dl89_atom base_body_atom;
    dl89_atom rec_head_atom;
    dl89_atom rec_body_atoms[2];

    store = ref_store_new();
    eval = make_eval(store);
    build_tc(rules, base_head, base_body, rec_head, rec_body, &base_head_atom,
             &base_body_atom, &rec_head_atom, rec_body_atoms);
    T_STATUS(dl89_eval_add_rule(eval, &rules[0]), DL89_OK);
    T_STATUS(dl89_eval_add_rule(eval, &rules[1]), DL89_OK);
    seed2(eval, R_EDGE, C_A, C_B);
    seed2(eval, R_EDGE, C_A, C_C);
    seed2(eval, R_EDGE, C_B, C_D);
    seed2(eval, R_EDGE, C_C, C_D);
    seed2(eval, R_EDGE, C_D, C_E);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    expect_path(store, C_A, C_E);
    expect_path(store, C_B, C_E);
    expect_path(store, C_C, C_E);
    expect_count(store, R_PATH, 2, 9);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_recursion_with_equality(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_rule rules[3];
    dl89_term base_head[2];
    dl89_term base_body[2];
    dl89_term rec_head[2];
    dl89_term rec_body[4];
    dl89_term cyc_head[1];
    dl89_term cyc_body[2];
    dl89_atom base_head_atom;
    dl89_atom base_body_atom;
    dl89_atom rec_head_atom;
    dl89_atom rec_body_atoms[2];
    dl89_atom cyc_head_atom;
    dl89_atom cyc_body_atom;
    dl89_const a[1];
    dl89_const b[1];

    store = ref_store_new();
    eval = make_eval(store);
    build_tc(rules, base_head, base_body, rec_head, rec_body, &base_head_atom,
             &base_body_atom, &rec_head_atom, rec_body_atoms);
    mk_var(&cyc_head[0], V_X);
    mk_var(&cyc_body[0], V_X);
    mk_var(&cyc_body[1], V_X);
    mk_atom(&cyc_head_atom, R_SAME, 1, cyc_head);
    mk_atom(&cyc_body_atom, R_PATH, 2, cyc_body);
    mk_rule(&rules[2], &cyc_head_atom, 1, &cyc_body_atom);
    T_STATUS(dl89_eval_add_rule(eval, &rules[0]), DL89_OK);
    T_STATUS(dl89_eval_add_rule(eval, &rules[1]), DL89_OK);
    T_STATUS(dl89_eval_add_rule(eval, &rules[2]), DL89_OK);
    seed2(eval, R_EDGE, C_A, C_B);
    seed2(eval, R_EDGE, C_B, C_A);
    seed2(eval, R_EDGE, C_B, C_C);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    a[0] = C_A;
    b[0] = C_B;
    expect_tuple(store, R_SAME, 1, a);
    expect_tuple(store, R_SAME, 1, b);
    expect_count(store, R_SAME, 1, 2);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void build_tc_eval(dl89_eval **eval_out, ref_store **store_out,
                          dl89_rule *rules, dl89_term *base_head,
                          dl89_term *base_body, dl89_term *rec_head,
                          dl89_term *rec_body, dl89_atom *base_head_atom,
                          dl89_atom *base_body_atom, dl89_atom *rec_head_atom,
                          dl89_atom *rec_body_atoms)
{
    ref_store *store;
    dl89_eval *eval;

    store = ref_store_new();
    eval = make_eval(store);
    build_tc(rules, base_head, base_body, rec_head, rec_body, base_head_atom,
             base_body_atom, rec_head_atom, rec_body_atoms);
    *store_out = store;
    *eval_out = eval;
}

static void test_rule_order_independence(void)
{
    ref_store *stores[2];
    dl89_eval *evals[2];
    dl89_rule rules[2];
    dl89_term base_head[2];
    dl89_term base_body[2];
    dl89_term rec_head[2];
    dl89_term rec_body[4];
    dl89_atom base_head_atom;
    dl89_atom base_body_atom;
    dl89_atom rec_head_atom;
    dl89_atom rec_body_atoms[2];
    int order;
    int i;

    for (order = 0; order < 2; ++order)
    {
        build_tc_eval(&evals[order], &stores[order], rules, base_head,
                      base_body, rec_head, rec_body, &base_head_atom,
                      &base_body_atom, &rec_head_atom, rec_body_atoms);
        for (i = 0; i < 2; ++i)
        {
            int which;

            which = i;
            if (order == 1)
            {
                which = 1 - i;
            }
            T_STATUS(dl89_eval_add_rule(evals[order], &rules[which]), DL89_OK);
        }
        seed2(evals[order], R_EDGE, C_A, C_B);
        seed2(evals[order], R_EDGE, C_B, C_C);
        T_STATUS(dl89_eval_run(evals[order]), DL89_OK);
    }
    T_ASSERT(ref_store_equals(stores[0], stores[1]) == 1);
    for (order = 0; order < 2; ++order)
    {
        dl89_eval_destroy(evals[order]);
        ref_store_free(stores[order]);
    }
}

static void test_tuple_order_independence(void)
{
    ref_store *stores[2];
    dl89_eval *evals[2];
    dl89_rule rules[2];
    dl89_term base_head[2];
    dl89_term base_body[2];
    dl89_term rec_head[2];
    dl89_term rec_body[4];
    dl89_atom base_head_atom;
    dl89_atom base_body_atom;
    dl89_atom rec_head_atom;
    dl89_atom rec_body_atoms[2];
    static const dl89_const pairs[3][2] = {
        {C_A, C_B},
        {C_B, C_C},
        {C_C, C_D},
    };
    int order;
    int i;
    int which;

    for (order = 0; order < 2; ++order)
    {
        build_tc_eval(&evals[order], &stores[order], rules, base_head,
                      base_body, rec_head, rec_body, &base_head_atom,
                      &base_body_atom, &rec_head_atom, rec_body_atoms);
        T_STATUS(dl89_eval_add_rule(evals[order], &rules[0]), DL89_OK);
        T_STATUS(dl89_eval_add_rule(evals[order], &rules[1]), DL89_OK);
        for (i = 0; i < 3; ++i)
        {
            which = i;
            if (order == 1)
            {
                which = 2 - i;
            }
            seed2(evals[order], R_EDGE, pairs[which][0], pairs[which][1]);
        }
        T_STATUS(dl89_eval_run(evals[order]), DL89_OK);
    }
    T_ASSERT(ref_store_equals(stores[0], stores[1]) == 1);
    for (order = 0; order < 2; ++order)
    {
        dl89_eval_destroy(evals[order]);
        ref_store_free(stores[order]);
    }
}

int main(void)
{
    test_chain_closure();
    test_multiple_rounds();
    test_cyclic_graph();
    test_self_loop();
    test_mutual_recursion();
    test_recursive_branching();
    test_recursion_with_equality();
    test_rule_order_independence();
    test_tuple_order_independence();
    if (dl89_test_failures == 0)
    {
        return 0;
    }
    return 1;
}

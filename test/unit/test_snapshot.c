/* test_snapshot.c - rule snapshots, reentrancy rejection, inert rule data. */

#include <string.h>

#include <dl89.h>

#include <test.h>

#include "build.h"
#include "expect.h"
#include "fault_store.h"
#include "ref_store.h"
#include "symbols.h"

int dl89_test_failures = 0;

typedef struct
{
    dl89_eval *eval;
    dl89_rule rule;
    dl89_const fact[1];
    int mode;
    int used;
    int rule_status;
    int fact_status;
    int run_status;
    void *buf;
    size_t len;
} snap_ctx;

static void snap_hook(void *ctx)
{
    snap_ctx *sc;

    sc = ctx;
    if (sc->used != 0)
    {
        return;
    }
    sc->used = 1;
    if (sc->mode == 0)
    {
        sc->rule_status = dl89_eval_add_rule(sc->eval, &sc->rule);
    }
    if (sc->mode == 1)
    {
        sc->fact_status = dl89_eval_add_fact(sc->eval, R_FLAG, 1, sc->fact);
    }
    if (sc->mode == 2)
    {
        sc->run_status = dl89_eval_run(sc->eval);
    }
    if (sc->mode == 3)
    {
        memset(sc->buf, 0xAA, sc->len);
    }
}

static void setup_copy_program(snap_ctx *sc, dl89_term *head_terms,
                               dl89_term *body_terms, dl89_atom *head,
                               dl89_atom *body, dl89_rule *rule)
{
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(head, R_P, 1, head_terms);
    mk_atom(body, R_Q, 1, body_terms);
    mk_rule(rule, head, 1, body);
    T_STATUS(dl89_eval_add_rule(sc->eval, rule), DL89_OK);
    sc->fact[0] = C_A;
    T_STATUS(dl89_eval_add_fact(sc->eval, R_Q, 1, sc->fact), DL89_OK);
}

static void test_add_rule_reentrancy(void)
{
    ref_store *store;
    fault_store *fs;
    dl89_eval_config config;
    dl89_eval *eval;
    snap_ctx sc;
    dl89_term head_terms[1];
    dl89_term body_terms[1];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;
    dl89_const a[1];

    store = ref_store_new();
    fs = fault_store_new(store);
    config.store = fault_store_dl89(fs);
    eval = NULL;
    T_STATUS(dl89_eval_create(&config, &eval), DL89_OK);
    memset(&sc, 0, sizeof(sc));
    sc.eval = eval;
    sc.mode = 0;
    setup_copy_program(&sc, head_terms, body_terms, &head, body, &rule);
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_R, 1, head_terms);
    mk_atom(&body[0], R_P, 1, body_terms);
    mk_rule(&sc.rule, &head, 1, body);
    fault_store_set_hook(fs, snap_hook, &sc);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    T_STATUS(sc.rule_status, DL89_EBUSY);
    a[0] = C_A;
    expect_tuple(store, R_P, 1, a);
    expect_count(store, R_R, 1, 0);
    dl89_eval_destroy(eval);
    fault_store_free(fs);
    ref_store_free(store);
}

static void test_add_fact_reentrancy(void)
{
    ref_store *store;
    fault_store *fs;
    dl89_eval_config config;
    dl89_eval *eval;
    snap_ctx sc;
    dl89_term head_terms[1];
    dl89_term body_terms[1];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;

    store = ref_store_new();
    fs = fault_store_new(store);
    config.store = fault_store_dl89(fs);
    eval = NULL;
    T_STATUS(dl89_eval_create(&config, &eval), DL89_OK);
    memset(&sc, 0, sizeof(sc));
    sc.eval = eval;
    sc.mode = 1;
    setup_copy_program(&sc, head_terms, body_terms, &head, body, &rule);
    sc.fact[0] = C_B;
    fault_store_set_hook(fs, snap_hook, &sc);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    T_STATUS(sc.fact_status, DL89_EBUSY);
    expect_count(store, R_FLAG, 1, 0);
    dl89_eval_destroy(eval);
    fault_store_free(fs);
    ref_store_free(store);
}

static void test_recursive_run_reentrancy(void)
{
    ref_store *store;
    fault_store *fs;
    dl89_eval_config config;
    dl89_eval *eval;
    snap_ctx sc;
    dl89_term head_terms[1];
    dl89_term body_terms[1];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;
    dl89_const a[1];

    store = ref_store_new();
    fs = fault_store_new(store);
    config.store = fault_store_dl89(fs);
    eval = NULL;
    T_STATUS(dl89_eval_create(&config, &eval), DL89_OK);
    memset(&sc, 0, sizeof(sc));
    sc.eval = eval;
    sc.mode = 2;
    setup_copy_program(&sc, head_terms, body_terms, &head, body, &rule);
    fault_store_set_hook(fs, snap_hook, &sc);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    T_STATUS(sc.run_status, DL89_EBUSY);
    a[0] = C_A;
    expect_tuple(store, R_P, 1, a);
    dl89_eval_destroy(eval);
    fault_store_free(fs);
    ref_store_free(store);
}

static void test_inert_rule_data(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[3];
    dl89_term body_terms[1];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;
    dl89_const a[1];
    dl89_const enc[3];
    dl89_eval_config config;

    store = ref_store_new();
    config.store = ref_store_dl89(store);
    eval = NULL;
    T_STATUS(dl89_eval_create(&config, &eval), DL89_OK);

    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);

    mk_const(&head_terms[0], R_R);
    mk_const(&head_terms[1], R_P);
    mk_var(&head_terms[2], V_X);
    mk_atom(&head, R_JOINED, 3, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);

    a[0] = C_A;
    T_STATUS(dl89_eval_add_fact(eval, R_Q, 1, a), DL89_OK);
    T_STATUS(dl89_eval_run(eval), DL89_OK);

    enc[0] = R_R;
    enc[1] = R_P;
    enc[2] = C_A;
    expect_count(store, R_JOINED, 3, 1);
    expect_tuple(store, R_JOINED, 3, enc);
    expect_count(store, R_R, 1, 0);
    expect_tuple(store, R_P, 1, a);
    expect_count(store, R_P, 1, 1);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_rule_buffer_mutation_during_run(void)
{
    ref_store *store;
    fault_store *fs;
    dl89_eval_config config;
    dl89_eval *eval;
    snap_ctx sc;
    dl89_term head_terms[1];
    dl89_term body_terms[1];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;
    dl89_const a[1];

    store = ref_store_new();
    fs = fault_store_new(store);
    config.store = fault_store_dl89(fs);
    eval = NULL;
    T_STATUS(dl89_eval_create(&config, &eval), DL89_OK);
    memset(&sc, 0, sizeof(sc));
    sc.eval = eval;
    sc.mode = 3;
    sc.buf = head_terms;
    sc.len = sizeof(head_terms);
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    sc.fact[0] = C_A;
    T_STATUS(dl89_eval_add_fact(eval, R_Q, 1, sc.fact), DL89_OK);
    fault_store_set_hook(fs, snap_hook, &sc);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    a[0] = C_A;
    expect_tuple(store, R_P, 1, a);
    expect_count(store, R_P, 1, 1);
    dl89_eval_destroy(eval);
    fault_store_free(fs);
    ref_store_free(store);
}

int main(void)
{
    test_add_rule_reentrancy();
    test_add_fact_reentrancy();
    test_recursive_run_reentrancy();
    test_inert_rule_data();
    test_rule_buffer_mutation_during_run();
    if (dl89_test_failures == 0)
    {
        return 0;
    }
    return 1;
}

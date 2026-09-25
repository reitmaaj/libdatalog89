/* test_allocfail.c - allocation-failure sweep over create, add_rule, run.
 *
 * Linked against the core archive without src/dl89_priv_mem.o so fault_mem.c
 * supplies the memory seam. Each allocation site is failed exactly once. */

#include <dl89.h>

#include <test.h>

#include "build.h"
#include "expect.h"
#include "fault_mem.h"
#include "ref_store.h"
#include "symbols.h"

int dl89_test_failures = 0;

static dl89_eval *create_eval(ref_store *store)
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

static void test_create_sweep(void)
{
    ref_store *store;
    dl89_eval_config config;
    dl89_eval *eval;
    unsigned long total;
    unsigned long n;

    store = ref_store_new();
    config.store = ref_store_dl89(store);
    fault_mem_reset();
    eval = NULL;
    T_STATUS(dl89_eval_create(&config, &eval), DL89_OK);
    T_ASSERT(eval != NULL);
    total = fault_mem_allocations();
    dl89_eval_destroy(eval);
    T_ASSERT(total > 0);
    for (n = 1; n <= total; ++n)
    {
        fault_mem_reset();
        fault_mem_fail_at(n);
        eval = NULL;
        T_STATUS(dl89_eval_create(&config, &eval), DL89_ENOMEM);
        T_ASSERT(eval == NULL);
    }
    fault_mem_disable();
    eval = NULL;
    T_STATUS(dl89_eval_create(&config, &eval), DL89_OK);
    T_ASSERT(eval != NULL);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void make_copy_rule(dl89_term *head_terms, dl89_term *body_terms,
                           dl89_atom *head, dl89_atom *body, dl89_rule *rule)
{
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(head, R_R, 1, head_terms);
    mk_atom(body, R_Q, 1, body_terms);
    mk_rule(rule, head, 1, body);
}

static void make_new_rule(dl89_term *head_terms, dl89_term *body_terms,
                          dl89_atom *head, dl89_atom *body, dl89_rule *rule,
                          dl89_rel relation, size_t arity)
{
    size_t p;

    for (p = 0; p < arity; ++p)
    {
        mk_var(&head_terms[p], (dl89_var)(p + 1));
        mk_var(&body_terms[p], (dl89_var)(p + 1));
    }
    mk_atom(head, relation, arity, head_terms);
    mk_atom(body, R_SAME, arity, body_terms);
    mk_rule(rule, head, 1, body);
}

static void test_add_rule_sweep(void)
{
    ref_store *store;
    ref_store *measure_store;
    dl89_eval *eval;
    dl89_eval *measure;
    dl89_term head_terms[2];
    dl89_term body_terms[2];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule copy_rule;
    dl89_rule new_rule;
    dl89_rule probe_rule;
    unsigned long total;
    unsigned long n;
    dl89_const a[1];

    store = ref_store_new();
    eval = create_eval(store);
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&copy_rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &copy_rule), DL89_OK);
    seed1(eval, R_Q, C_A);

    fault_mem_reset();
    make_copy_rule(head_terms, body_terms, &head, body, &copy_rule);
    T_STATUS(dl89_eval_add_rule(eval, &copy_rule), DL89_OK);
    total = fault_mem_allocations();
    T_ASSERT(total > 0);

    fault_mem_disable();
    for (n = 1; n <= total; ++n)
    {
        fault_mem_reset();
        fault_mem_fail_at(n);
        T_STATUS(dl89_eval_add_rule(eval, &copy_rule), DL89_ENOMEM);
    }
    fault_mem_disable();
    T_STATUS(dl89_eval_add_rule(eval, &copy_rule), DL89_OK);

    /* Sweep a rule that registers new relations, covering the registry
     * growth allocations. After each failure a rule with a conflicting arity
     * for the same relations must still be accepted. */
    measure_store = ref_store_new();
    measure = create_eval(measure_store);
    make_copy_rule(head_terms, body_terms, &head, body, &copy_rule);
    T_STATUS(dl89_eval_add_rule(measure, &copy_rule), DL89_OK);
    make_new_rule(head_terms, body_terms, &head, body, &new_rule, R_FLAG, 2);
    fault_mem_reset();
    T_STATUS(dl89_eval_add_rule(measure, &new_rule), DL89_OK);
    total = fault_mem_allocations();
    fault_mem_disable();
    dl89_eval_destroy(measure);
    ref_store_free(measure_store);
    T_ASSERT(total > 0);

    for (n = 1; n <= total; ++n)
    {
        ref_store *s2;
        dl89_eval *e2;

        s2 = ref_store_new();
        e2 = create_eval(s2);
        make_copy_rule(head_terms, body_terms, &head, body, &copy_rule);
        T_STATUS(dl89_eval_add_rule(e2, &copy_rule), DL89_OK);
        make_new_rule(head_terms, body_terms, &head, body, &new_rule, R_FLAG,
                      2);
        fault_mem_reset();
        fault_mem_fail_at(n);
        T_STATUS(dl89_eval_add_rule(e2, &new_rule), DL89_ENOMEM);
        fault_mem_disable();
        make_new_rule(head_terms, body_terms, &head, body, &probe_rule, R_FLAG,
                      1);
        T_STATUS(dl89_eval_add_rule(e2, &probe_rule), DL89_OK);
        dl89_eval_destroy(e2);
        ref_store_free(s2);
    }

    T_STATUS(dl89_eval_run(eval), DL89_OK);
    a[0] = C_A;
    expect_tuple(store, R_P, 1, a);
    dl89_eval_destroy(eval);
    ref_store_free(store);
}

static void test_run_sweep(void)
{
    ref_store *measure_store;
    dl89_eval *measure_eval;
    dl89_term head_terms[1];
    dl89_term body_terms[1];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;
    unsigned long total;
    unsigned long n;
    dl89_const a[1];

    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&rule, &head, 1, body);

    measure_store = ref_store_new();
    measure_eval = create_eval(measure_store);
    T_STATUS(dl89_eval_add_rule(measure_eval, &rule), DL89_OK);
    seed1(measure_eval, R_Q, C_A);
    fault_mem_reset();
    T_STATUS(dl89_eval_run(measure_eval), DL89_OK);
    total = fault_mem_allocations();
    fault_mem_disable();
    dl89_eval_destroy(measure_eval);
    ref_store_free(measure_store);

    for (n = 1; n <= total; ++n)
    {
        ref_store *store;
        dl89_eval *eval;

        store = ref_store_new();
        eval = create_eval(store);
        T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
        seed1(eval, R_Q, C_A);
        fault_mem_reset();
        fault_mem_fail_at(n);
        T_STATUS(dl89_eval_run(eval), DL89_ENOMEM);
        fault_mem_disable();
        dl89_eval_destroy(eval);
        ref_store_free(store);
    }

    {
        ref_store *store;
        dl89_eval *eval;

        store = ref_store_new();
        eval = create_eval(store);
        T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
        seed1(eval, R_Q, C_A);
        fault_mem_disable();
        T_STATUS(dl89_eval_run(eval), DL89_OK);
        a[0] = C_A;
        expect_tuple(store, R_P, 1, a);
        expect_count(store, R_P, 1, 1);
        dl89_eval_destroy(eval);
        ref_store_free(store);
    }
}

static void add_tc_rules(dl89_eval *eval)
{
    dl89_term head_terms[2];
    dl89_term body_terms[4];
    dl89_atom head;
    dl89_atom body[2];
    dl89_rule rule;

    mk_var(&head_terms[0], V_X);
    mk_var(&head_terms[1], V_Y);
    mk_var(&body_terms[0], V_X);
    mk_var(&body_terms[1], V_Y);
    mk_atom(&head, R_PATH, 2, head_terms);
    mk_atom(&body[0], R_EDGE, 2, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);

    mk_var(&head_terms[0], V_X);
    mk_var(&head_terms[1], V_Z);
    mk_var(&body_terms[0], V_X);
    mk_var(&body_terms[1], V_Y);
    mk_var(&body_terms[2], V_Y);
    mk_var(&body_terms[3], V_Z);
    mk_atom(&head, R_PATH, 2, head_terms);
    mk_atom(&body[0], R_PATH, 2, &body_terms[0]);
    mk_atom(&body[1], R_EDGE, 2, &body_terms[2]);
    mk_rule(&rule, &head, 2, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
}

static void test_add_fact_sweep(void)
{
    ref_store *store;
    dl89_eval *eval;
    dl89_term head_terms[2];
    dl89_term body_terms[2];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;
    dl89_const fact[1];
    unsigned long total;
    unsigned long n;

    store = ref_store_new();
    eval = create_eval(store);
    fact[0] = C_A;
    fault_mem_reset();
    T_STATUS(dl89_eval_add_fact(eval, 55, 1, fact), DL89_OK);
    total = fault_mem_allocations();
    fault_mem_disable();
    dl89_eval_destroy(eval);
    ref_store_free(store);
    T_ASSERT(total > 0);

    for (n = 1; n <= total; ++n)
    {
        store = ref_store_new();
        eval = create_eval(store);
        fault_mem_reset();
        fault_mem_fail_at(n);
        T_STATUS(dl89_eval_add_fact(eval, 55, 1, fact), DL89_ENOMEM);
        fault_mem_disable();
        mk_var(&head_terms[0], V_X);
        mk_var(&head_terms[1], V_Y);
        mk_var(&body_terms[0], V_X);
        mk_var(&body_terms[1], V_Y);
        mk_atom(&head, 55, 2, head_terms);
        mk_atom(&body[0], R_Q, 2, body_terms);
        mk_rule(&rule, &head, 1, body);
        T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
        dl89_eval_destroy(eval);
        ref_store_free(store);
    }
}

static void seed2(dl89_eval *eval, dl89_rel relation, dl89_const a,
                  dl89_const b)
{
    dl89_const tuple[2];

    tuple[0] = a;
    tuple[1] = b;
    T_STATUS(dl89_eval_add_fact(eval, relation, 2, tuple), DL89_OK);
}

static void test_recursive_run_sweep(void)
{
    ref_store *store;
    dl89_eval *eval;
    unsigned long total;
    unsigned long n;

    store = ref_store_new();
    eval = create_eval(store);
    add_tc_rules(eval);
    seed2(eval, R_EDGE, C_A, C_B);
    seed2(eval, R_EDGE, C_B, C_C);
    seed2(eval, R_EDGE, C_C, C_D);
    fault_mem_reset();
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    total = fault_mem_allocations();
    fault_mem_disable();
    dl89_eval_destroy(eval);
    ref_store_free(store);
    T_ASSERT(total > 0);

    for (n = 1; n <= total; ++n)
    {
        store = ref_store_new();
        eval = create_eval(store);
        add_tc_rules(eval);
        seed2(eval, R_EDGE, C_A, C_B);
        seed2(eval, R_EDGE, C_B, C_C);
        seed2(eval, R_EDGE, C_C, C_D);
        fault_mem_reset();
        fault_mem_fail_at(n);
        T_STATUS(dl89_eval_run(eval), DL89_ENOMEM);
        fault_mem_disable();
        T_STATUS(dl89_eval_run(eval), DL89_OK);
        expect_count(store, R_PATH, 2, 6);
        dl89_eval_destroy(eval);
        ref_store_free(store);
    }
}

int main(void)
{
    test_create_sweep();
    test_add_rule_sweep();
    test_run_sweep();
    test_add_fact_sweep();
    test_recursive_run_sweep();
    if (dl89_test_failures == 0)
    {
        return 0;
    }
    return 1;
}

/* test_store_failures.c - store callback failure propagation and scan cleanup.
 */

#include <dl89.h>

#include <test.h>

#include "build.h"
#include "expect.h"
#include "fault_store.h"
#include "ref_store.h"
#include "symbols.h"

int dl89_test_failures = 0;

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

static void check_clean(const fault_store *fs)
{
    T_EQ_SIZE(fault_store_live_scans(fs), 0);
    T_EQ_SIZE(fault_store_total_opens(fs), fault_store_total_closes(fs));
    T_EQ_SIZE(fault_store_arg_errors(fs), 0);
}

static void test_scan_open_failure(void)
{
    ref_store *store;
    fault_store *fs;
    dl89_eval_config config;
    dl89_eval *eval;
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
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    seed1(eval, R_Q, C_A);
    fault_store_fail_scan_open(fs, 1);
    T_STATUS(dl89_eval_run(eval), DL89_ESTORE);
    check_clean(fs);
    dl89_eval_destroy(eval);
    fault_store_free(fs);
    ref_store_free(store);
}

static void test_first_scan_next_failure(void)
{
    ref_store *store;
    fault_store *fs;
    dl89_eval_config config;
    dl89_eval *eval;
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
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    seed1(eval, R_Q, C_A);
    fault_store_fail_scan_next(fs, 1);
    T_STATUS(dl89_eval_run(eval), DL89_ESTORE);
    check_clean(fs);
    dl89_eval_destroy(eval);
    fault_store_free(fs);
    ref_store_free(store);
}

static void test_later_scan_next_failure(void)
{
    ref_store *store;
    fault_store *fs;
    dl89_eval_config config;
    dl89_eval *eval;
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
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    seed1(eval, R_Q, C_A);
    seed1(eval, R_Q, C_B);
    fault_store_fail_scan_next(fs, 2);
    T_STATUS(dl89_eval_run(eval), DL89_ESTORE);
    check_clean(fs);
    dl89_eval_destroy(eval);
    fault_store_free(fs);
    ref_store_free(store);
}

static void test_insert_failure(void)
{
    ref_store *store;
    fault_store *fs;
    dl89_eval_config config;
    dl89_eval *eval;
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
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    seed1(eval, R_Q, C_A);
    fault_store_fail_insert(fs, fault_store_insert_calls(fs) + 1);
    T_STATUS(dl89_eval_run(eval), DL89_ESTORE);
    check_clean(fs);
    dl89_eval_destroy(eval);
    fault_store_free(fs);
    ref_store_free(store);
}

static void test_failure_after_nested_joins(void)
{
    ref_store *store;
    fault_store *fs;
    dl89_eval_config config;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_term body_terms[4];
    dl89_atom head;
    dl89_atom body[3];
    dl89_rule rule;

    store = ref_store_new();
    fs = fault_store_new(store);
    config.store = fault_store_dl89(fs);
    eval = NULL;
    T_STATUS(dl89_eval_create(&config, &eval), DL89_OK);
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
    seed1(eval, R_Q, C_A);
    seed2(eval, R_EDGE, C_A, C_E);
    fault_store_fail_scan_next(fs, 3);
    T_STATUS(dl89_eval_run(eval), DL89_ESTORE);
    check_clean(fs);
    dl89_eval_destroy(eval);
    fault_store_free(fs);
    ref_store_free(store);
}

static void test_destructible_after_failure(void)
{
    ref_store *store;
    fault_store *fs;
    dl89_eval_config config;
    dl89_eval *eval;
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
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    seed1(eval, R_Q, C_A);
    fault_store_fail_scan_open(fs, 1);
    T_STATUS(dl89_eval_run(eval), DL89_ESTORE);
    dl89_eval_destroy(eval);
    fault_store_free(fs);
    ref_store_free(store);
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

/* Round 0 opens the edge scan and the empty path scan; the first delta-round
 * scan is therefore the third scan_open and must propagate as DL89_ESTORE. */
static void test_delta_round_failure(void)
{
    ref_store *store;
    fault_store *fs;
    dl89_eval_config config;
    dl89_eval *eval;

    store = ref_store_new();
    fs = fault_store_new(store);
    config.store = fault_store_dl89(fs);
    eval = NULL;
    T_STATUS(dl89_eval_create(&config, &eval), DL89_OK);
    add_tc_rules(eval);
    seed2(eval, R_EDGE, C_A, C_B);
    seed2(eval, R_EDGE, C_B, C_C);
    fault_store_fail_scan_open(fs, 3);
    T_STATUS(dl89_eval_run(eval), DL89_ESTORE);
    check_clean(fs);
    dl89_eval_destroy(eval);
    fault_store_free(fs);
    ref_store_free(store);
}

/* A failed insert for a brand-new relation must roll the arity registry
 * back: a rule using another arity for that relation stays acceptable. */
static void test_add_fact_store_failure_rollback(void)
{
    ref_store *store;
    fault_store *fs;
    dl89_eval_config config;
    dl89_eval *eval;
    dl89_term head_terms[2];
    dl89_term body_terms[2];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;
    dl89_const fact[1];

    store = ref_store_new();
    fs = fault_store_new(store);
    config.store = fault_store_dl89(fs);
    eval = NULL;
    T_STATUS(dl89_eval_create(&config, &eval), DL89_OK);
    fact[0] = C_A;
    fault_store_fail_insert(fs, 1);
    T_STATUS(dl89_eval_add_fact(eval, 55, 1, fact), DL89_ESTORE);
    mk_var(&head_terms[0], V_X);
    mk_var(&head_terms[1], V_Y);
    mk_var(&body_terms[0], V_X);
    mk_var(&body_terms[1], V_Y);
    mk_atom(&head, 55, 2, head_terms);
    mk_atom(&body[0], R_Q, 2, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    dl89_eval_destroy(eval);
    fault_store_free(fs);
    ref_store_free(store);
}

/* A failed run must leave the evaluator reusable: a subsequent run over the
 * same store computes exactly the same closure as a fresh evaluator. */
static void test_rerun_after_failure(void)
{
    ref_store *store;
    ref_store *fresh_store;
    fault_store *fs;
    dl89_eval_config config;
    dl89_eval *eval;
    dl89_eval *fresh;

    store = ref_store_new();
    fresh_store = ref_store_new();
    fs = fault_store_new(store);
    config.store = fault_store_dl89(fs);
    eval = NULL;
    T_STATUS(dl89_eval_create(&config, &eval), DL89_OK);
    add_tc_rules(eval);
    seed2(eval, R_EDGE, C_A, C_B);
    seed2(eval, R_EDGE, C_B, C_C);
    fault_store_fail_scan_open(fs, 1);
    T_STATUS(dl89_eval_run(eval), DL89_ESTORE);
    check_clean(fs);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    T_EQ_SIZE(ref_store_count(store, R_PATH, 2), 3);

    config.store = ref_store_dl89(fresh_store);
    fresh = NULL;
    T_STATUS(dl89_eval_create(&config, &fresh), DL89_OK);
    add_tc_rules(fresh);
    seed2(fresh, R_EDGE, C_A, C_B);
    seed2(fresh, R_EDGE, C_B, C_C);
    T_STATUS(dl89_eval_run(fresh), DL89_OK);
    T_ASSERT(ref_store_equals(store, fresh_store) == 1);

    dl89_eval_destroy(fresh);
    dl89_eval_destroy(eval);
    fault_store_free(fs);
    ref_store_free(store);
    ref_store_free(fresh_store);
}

int main(void)
{
    test_scan_open_failure();
    test_first_scan_next_failure();
    test_later_scan_next_failure();
    test_insert_failure();
    test_failure_after_nested_joins();
    test_destructible_after_failure();
    test_delta_round_failure();
    test_add_fact_store_failure_rollback();
    test_rerun_after_failure();
    if (dl89_test_failures == 0)
    {
        return 0;
    }
    return 1;
}

/* test_api.c - evaluator construction, null inputs, missing callbacks. */

#include <datalog89.h>

#include <test.h>

#include "build.h"
#include "ref_store.h"
#include "symbols.h"

int datalog89_test_failures = 0;

static void test_valid_create_destroy(void)
{
    ref_store *store;
    datalog89_eval_config config;
    datalog89_eval *eval;

    store = ref_store_new();
    T_ASSERT(store != NULL);
    config.store = ref_store_datalog89(store);
    eval = NULL;
    T_STATUS(datalog89_eval_create(&config, &eval), DATALOG89_OK);
    T_ASSERT(eval != NULL);
    datalog89_eval_destroy(eval);
    datalog89_eval_destroy(NULL);
    ref_store_free(store);
}

static void test_null_inputs(void)
{
    ref_store *store;
    datalog89_eval_config config;
    datalog89_eval *eval;

    store = ref_store_new();
    T_ASSERT(store != NULL);
    config.store = ref_store_datalog89(store);
    eval = NULL;
    T_STATUS(datalog89_eval_create(NULL, &eval), DATALOG89_EINVAL);
    T_ASSERT(eval == NULL);
    T_STATUS(datalog89_eval_create(&config, NULL), DATALOG89_EINVAL);
    ref_store_free(store);
}

static void check_missing(int which)
{
    ref_store *store;
    datalog89_store good;
    datalog89_store_ops ops;
    datalog89_eval_config config;
    datalog89_eval *eval;

    store = ref_store_new();
    T_ASSERT(store != NULL);
    good = ref_store_datalog89(store);
    ops = *good.ops;
    if (which == 0)
    {
        ops.insert = NULL;
    }
    if (which == 1)
    {
        ops.scan_open = NULL;
    }
    if (which == 2)
    {
        ops.scan_next = NULL;
    }
    if (which == 3)
    {
        ops.scan_close = NULL;
    }
    config.store.ctx = store;
    config.store.ops = &ops;
    eval = NULL;
    T_STATUS(datalog89_eval_create(&config, &eval), DATALOG89_EINVAL);
    T_ASSERT(eval == NULL);
    ref_store_free(store);
}

static void test_missing_callbacks(void)
{
    int which;

    for (which = 0; which < 4; ++which)
    {
        check_missing(which);
    }
}

static void test_null_ops(void)
{
    ref_store *store;
    datalog89_eval_config config;
    datalog89_eval *eval;

    store = ref_store_new();
    T_ASSERT(store != NULL);
    config.store.ctx = store;
    config.store.ops = NULL;
    eval = NULL;
    T_STATUS(datalog89_eval_create(&config, &eval), DATALOG89_EINVAL);
    T_ASSERT(eval == NULL);
    ref_store_free(store);
}

static void test_null_evaluator_calls(void)
{
    datalog89_term head_terms[1];
    datalog89_atom head;
    datalog89_rule rule;
    datalog89_const tuple[1];

    tuple[0] = C_A;
    mk_const(&head_terms[0], C_A);
    mk_atom(&head, R_P, 1, head_terms);
    mk_rule(&rule, &head, 0, NULL);
    T_STATUS(datalog89_eval_add_rule(NULL, &rule), DATALOG89_EINVAL);
    T_STATUS(datalog89_eval_add_rule(NULL, NULL), DATALOG89_EINVAL);
    T_STATUS(datalog89_eval_add_fact(NULL, R_P, 1, tuple), DATALOG89_EINVAL);
    T_STATUS(datalog89_eval_run(NULL), DATALOG89_EINVAL);
}

static void test_null_rule_and_tuple(void)
{
    ref_store *store;
    datalog89_eval_config config;
    datalog89_eval *eval;

    store = ref_store_new();
    T_ASSERT(store != NULL);
    config.store = ref_store_datalog89(store);
    eval = NULL;
    T_STATUS(datalog89_eval_create(&config, &eval), DATALOG89_OK);
    T_ASSERT(eval != NULL);
    T_STATUS(datalog89_eval_add_rule(eval, NULL), DATALOG89_EINVAL);
    T_STATUS(datalog89_eval_add_fact(eval, R_P, 1, NULL), DATALOG89_EINVAL);
    T_STATUS(datalog89_eval_add_fact(eval, R_FLAG, 0, NULL), DATALOG89_OK);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

int main(void)
{
    test_valid_create_destroy();
    test_null_inputs();
    test_missing_callbacks();
    test_null_ops();
    test_null_evaluator_calls();
    test_null_rule_and_tuple();
    if (datalog89_test_failures == 0)
    {
        return 0;
    }
    return 1;
}

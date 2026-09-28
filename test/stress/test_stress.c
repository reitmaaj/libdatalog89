/* test_stress.c - bounded stress correctness (not a benchmark).
 * DATALOG89_STRESS_N selects the chain/fan-out size (default 1000). */

#include <stdio.h>
#include <stdlib.h>

#include <datalog89.h>

#include <test.h>

#include "build.h"
#include "hash_store.h"
#include "symbols.h"

int datalog89_test_failures = 0;

static size_t stress_n(void)
{
    const char *env;
    unsigned long n;

    env = getenv("DATALOG89_STRESS_N");
    if (env == NULL)
    {
        return 1000;
    }
    n = strtoul(env, NULL, 10);
    if (n == 0)
    {
        return 1000;
    }
    return (size_t)n;
}

static void seed2(datalog89_eval *eval, datalog89_rel relation,
                  datalog89_const a, datalog89_const b)
{
    datalog89_const tuple[2];

    tuple[0] = a;
    tuple[1] = b;
    T_STATUS(datalog89_eval_add_fact(eval, relation, 2, tuple), DATALOG89_OK);
}

static void add_tc(datalog89_eval *eval)
{
    datalog89_term head_terms[2];
    datalog89_term body_terms[4];
    datalog89_atom head;
    datalog89_atom body[2];
    datalog89_rule rule;

    mk_var(&head_terms[0], V_X);
    mk_var(&head_terms[1], V_Y);
    mk_var(&body_terms[0], V_X);
    mk_var(&body_terms[1], V_Y);
    mk_atom(&head, R_PATH, 2, head_terms);
    mk_atom(&body[0], R_EDGE, 2, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(datalog89_eval_add_rule(eval, &rule), DATALOG89_OK);

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
    T_STATUS(datalog89_eval_add_rule(eval, &rule), DATALOG89_OK);
}

static int scan_paths(hash_store *hs, size_t n, size_t *out_count, int *valid)
{
    datalog89_store store;
    datalog89_scan *scan;
    unsigned char bound[2];
    datalog89_const values[2];
    datalog89_const tuple[2];
    size_t count;
    int found;
    int rc;

    store = hash_store_datalog89(hs);
    bound[0] = 0;
    bound[1] = 0;
    values[0] = 0;
    values[1] = 0;
    scan = NULL;
    rc = store.ops->scan_open(store.ctx, R_PATH, 2, values, bound, &scan);
    if (rc != 0)
    {
        return 0;
    }
    count = 0;
    *valid = 1;
    for (;;)
    {
        found = 0;
        rc = store.ops->scan_next(store.ctx, scan, tuple, &found);
        if (rc != 0)
        {
            store.ops->scan_close(store.ctx, scan);
            return 0;
        }
        if (found == 0)
        {
            break;
        }
        count = count + 1;
        if (tuple[0] >= tuple[1])
        {
            *valid = 0;
        }
        if (tuple[0] < 1)
        {
            *valid = 0;
        }
        if (tuple[1] > n + 1)
        {
            *valid = 0;
        }
    }
    store.ops->scan_close(store.ctx, scan);
    *out_count = count;
    return 1;
}

static void test_long_chain(void)
{
    hash_store *hs;
    datalog89_eval_config config;
    datalog89_eval *eval;
    size_t n;
    size_t i;
    size_t count;
    int valid;
    datalog89_const want[2];

    n = stress_n();
    hs = hash_store_new();
    config.store = hash_store_datalog89(hs);
    eval = NULL;
    T_STATUS(datalog89_eval_create(&config, &eval), DATALOG89_OK);
    add_tc(eval);
    for (i = 1; i <= n; ++i)
    {
        seed2(eval, R_EDGE, (datalog89_const)i, (datalog89_const)(i + 1));
    }
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    T_ASSERT(scan_paths(hs, n, &count, &valid) == 1);
    T_ASSERT(valid == 1);
    T_EQ_SIZE(count, n * (n + 1) / 2);
    want[0] = 1;
    want[1] = (datalog89_const)(n + 1);
    T_ASSERT(hash_store_has(hs, R_PATH, 2, want) == 1);
    T_EQ_SIZE(hash_store_scan_opens(hs), hash_store_scan_closes(hs));
    datalog89_eval_destroy(eval);
    hash_store_free(hs);
}

static void test_wide_fan_out(void)
{
    hash_store *hs;
    datalog89_eval_config config;
    datalog89_eval *eval;
    size_t n;
    size_t i;
    datalog89_const want[2];

    n = stress_n();
    hs = hash_store_new();
    config.store = hash_store_datalog89(hs);
    eval = NULL;
    T_STATUS(datalog89_eval_create(&config, &eval), DATALOG89_OK);
    add_tc(eval);
    for (i = 1; i <= n; ++i)
    {
        seed2(eval, R_EDGE, 1, (datalog89_const)(i + 1));
    }
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    T_EQ_SIZE(hash_store_count(hs, R_PATH, 2), n);
    want[0] = 1;
    want[1] = (datalog89_const)(n + 1);
    T_ASSERT(hash_store_has(hs, R_PATH, 2, want) == 1);
    T_EQ_SIZE(hash_store_scan_opens(hs), hash_store_scan_closes(hs));
    datalog89_eval_destroy(eval);
    hash_store_free(hs);
}

static void test_dense_cycle(void)
{
    hash_store *hs;
    datalog89_eval_config config;
    datalog89_eval *eval;
    size_t m;
    size_t i;
    size_t j;
    datalog89_const want[2];

    m = 30;
    hs = hash_store_new();
    config.store = hash_store_datalog89(hs);
    eval = NULL;
    T_STATUS(datalog89_eval_create(&config, &eval), DATALOG89_OK);
    add_tc(eval);
    for (i = 1; i <= m; ++i)
    {
        for (j = 1; j <= m; ++j)
        {
            seed2(eval, R_EDGE, (datalog89_const)i, (datalog89_const)j);
        }
    }
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    T_EQ_SIZE(hash_store_count(hs, R_PATH, 2), m * m);
    want[0] = 1;
    want[1] = 1;
    T_ASSERT(hash_store_has(hs, R_PATH, 2, want) == 1);
    want[0] = (datalog89_const)m;
    want[1] = (datalog89_const)m;
    T_ASSERT(hash_store_has(hs, R_PATH, 2, want) == 1);
    T_EQ_SIZE(hash_store_scan_opens(hs), hash_store_scan_closes(hs));
    datalog89_eval_destroy(eval);
    hash_store_free(hs);
}

static void test_irrelevant_tuples(void)
{
    hash_store *hs;
    datalog89_eval_config config;
    datalog89_eval *eval;
    datalog89_term head_terms[1];
    datalog89_term body_terms[4];
    datalog89_atom head;
    datalog89_atom body[2];
    datalog89_rule rule;
    size_t i;
    size_t irrelevant;
    datalog89_const want[1];

    irrelevant = stress_n() * 5;
    hs = hash_store_new();
    config.store = hash_store_datalog89(hs);
    eval = NULL;
    T_STATUS(datalog89_eval_create(&config, &eval), DATALOG89_OK);
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_const(&body_terms[1], C_A);
    mk_const(&body_terms[2], C_A);
    mk_const(&body_terms[3], C_B);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_LEFT, 2, &body_terms[0]);
    mk_atom(&body[1], R_EDGE, 2, &body_terms[2]);
    mk_rule(&rule, &head, 2, body);
    T_STATUS(datalog89_eval_add_rule(eval, &rule), DATALOG89_OK);
    for (i = 0; i < irrelevant; ++i)
    {
        seed2(eval, R_LEFT, (datalog89_const)(1000 + i),
              (datalog89_const)(2000 + i));
    }
    seed2(eval, R_LEFT, C_A, C_A);
    seed2(eval, R_EDGE, C_A, C_B);
    seed2(eval, R_EDGE, C_B, C_C);
    T_STATUS(datalog89_eval_run(eval), DATALOG89_OK);
    T_EQ_SIZE(hash_store_count(hs, R_P, 1), 1);
    want[0] = C_A;
    T_ASSERT(hash_store_has(hs, R_P, 1, want) == 1);
    T_EQ_SIZE(hash_store_scan_opens(hs), hash_store_scan_closes(hs));
    datalog89_eval_destroy(eval);
    hash_store_free(hs);
}

int main(void)
{
    test_long_chain();
    test_wide_fan_out();
    test_dense_cycle();
    test_irrelevant_tuples();
    if (datalog89_test_failures == 0)
    {
        printf("stress: %lu edges chain OK\n", (unsigned long)stress_n());
        return 0;
    }
    return 1;
}

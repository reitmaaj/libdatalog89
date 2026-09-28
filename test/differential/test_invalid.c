/* test_invalid.c - generated malformed rules must be rejected atomically.
 * DATALOG89_DIFF_N selects the number of cases (default 1000). */

#include <stdio.h>
#include <stdlib.h>

#include <datalog89.h>

#include "build.h"
#include "ref_store.h"
#include "symbols.h"

static void seed1(datalog89_eval *eval, datalog89_rel relation,
                  datalog89_const value)
{
    datalog89_const tuple[1];

    tuple[0] = value;
    if (datalog89_eval_add_fact(eval, relation, 1, tuple) != DATALOG89_OK)
    {
        fprintf(stderr, "seed1 failed\n");
    }
}

static void run_case(unsigned long seed, int *failed)
{
    ref_store *store;
    datalog89_eval_config config;
    datalog89_eval *eval;
    datalog89_term head_terms[2];
    datalog89_term body_terms[1];
    datalog89_atom head;
    datalog89_atom body[1];
    datalog89_rule rule;
    datalog89_const a[1];
    datalog89_rel fresh;
    int mode;
    int st;

    store = ref_store_new();
    config.store = ref_store_datalog89(store);
    eval = NULL;
    if (datalog89_eval_create(&config, &eval) != DATALOG89_OK)
    {
        *failed = 1;
        ref_store_free(store);
        return;
    }
    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_atom(&head, R_P, 1, head_terms);
    mk_atom(&body[0], R_Q, 1, body_terms);
    mk_rule(&rule, &head, 1, body);
    if (datalog89_eval_add_rule(eval, &rule) != DATALOG89_OK)
    {
        *failed = 1;
    }
    seed1(eval, R_Q, C_A);

    fresh = (datalog89_rel)(1000 + seed);
    mode = (int)(seed % 4);
    if (mode == 0)
    {
        head_terms[0].kind = 7;
        mk_atom(&head, fresh, 1, head_terms);
        mk_atom(&body[0], R_Q, 1, body_terms);
        mk_rule(&rule, &head, 1, body);
    }
    if (mode == 1)
    {
        mk_var(&head_terms[0], (datalog89_var)99);
        mk_atom(&head, fresh, 1, head_terms);
        mk_atom(&body[0], R_Q, 1, body_terms);
        mk_rule(&rule, &head, 1, body);
    }
    if (mode == 2)
    {
        mk_var(&head_terms[0], V_X);
        mk_var(&head_terms[1], V_X);
        mk_atom(&head, R_P, 2, head_terms);
        mk_atom(&body[0], R_Q, 1, body_terms);
        mk_rule(&rule, &head, 1, body);
    }
    if (mode == 3)
    {
        mk_var(&head_terms[0], V_X);
        mk_atom(&head, fresh, 1, head_terms);
        mk_rule(&rule, &head, 0, NULL);
    }
    st = datalog89_eval_add_rule(eval, &rule);
    if (st != DATALOG89_EPROGRAM)
    {
        fprintf(stderr, "seed %lu mode %d: add_rule => %d (expected %d)\n",
                seed, mode, st, DATALOG89_EPROGRAM);
        *failed = 1;
    }
    if (datalog89_eval_run(eval) != DATALOG89_OK)
    {
        fprintf(stderr, "seed %lu: run failed\n", seed);
        *failed = 1;
    }
    a[0] = C_A;
    if (ref_store_count(store, R_P, 1) != 1)
    {
        fprintf(stderr, "seed %lu: baseline rule state changed\n", seed);
        *failed = 1;
    }
    if (ref_store_has(store, R_P, 1, a) != 1)
    {
        fprintf(stderr, "seed %lu: baseline derivation missing\n", seed);
        *failed = 1;
    }
    if (ref_store_count(store, fresh, 1) != 0)
    {
        fprintf(stderr, "seed %lu: rejected rule had consequences\n", seed);
        *failed = 1;
    }
    datalog89_eval_destroy(eval);
    ref_store_free(store);
}

int main(void)
{
    const char *env;
    unsigned long total;
    unsigned long seed;
    int failed;

    env = getenv("DATALOG89_DIFF_N");
    total = 1000;
    if (env != NULL)
    {
        total = strtoul(env, NULL, 10);
    }
    failed = 0;
    for (seed = 1; seed <= total; ++seed)
    {
        run_case(seed, &failed);
    }
    if (failed != 0)
    {
        return 1;
    }
    printf("invalid-program corpus: %lu cases OK\n", total);
    return 0;
}

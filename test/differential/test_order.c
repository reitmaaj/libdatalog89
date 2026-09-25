/* test_order.c - generated programs must reach the same fixed point under
 * reversed rule insertion and reversed fact insertion. DL89_DIFF_N selects
 * the corpus size (default 10000). */

#include <stdio.h>
#include <stdlib.h>

#include <dl89.h>

#include "gen.h"
#include "ref_eval.h"
#include "ref_store.h"

static int seed_store(ref_store *store, const gen_case *c, int reverse)
{
    dl89_store s;
    size_t i;
    int inserted;

    s = ref_store_dl89(store);
    for (i = 0; i < c->fact_count; ++i)
    {
        size_t j;

        j = reverse ? c->fact_count - 1 - i : i;
        inserted = 0;
        if (s.ops->insert(s.ctx, c->fact_relations[j], c->fact_arities[j],
                          c->facts[j], &inserted) != 0)
        {
            return 1;
        }
    }
    return 0;
}

static int run_program(ref_store *store, const gen_case *c, int reverse)
{
    dl89_eval_config config;
    dl89_eval *eval;
    dl89_rule rules[GEN_MAX_RULES];
    size_t i;
    int st;

    for (i = 0; i < c->rule_count; ++i)
    {
        size_t j;

        j = reverse ? c->rule_count - 1 - i : i;
        rules[i] = c->rules[j].rule;
    }
    config.store = ref_store_dl89(store);
    eval = NULL;
    st = dl89_eval_create(&config, &eval);
    if (st != DL89_OK)
    {
        return st;
    }
    for (i = 0; i < c->rule_count; ++i)
    {
        st = dl89_eval_add_rule(eval, &rules[i]);
        if (st != DL89_OK)
        {
            dl89_eval_destroy(eval);
            return st;
        }
    }
    st = dl89_eval_run(eval);
    dl89_eval_destroy(eval);
    return st;
}

static void report(const gen_case *c, const ref_store *expected,
                   const ref_store *actual, unsigned long seed)
{
    fprintf(stderr, "order failure seed %lu\n", seed);
    gen_print(c, stderr);
    fprintf(stderr, "expected:\n");
    ref_store_dump(expected, stderr);
    fprintf(stderr, "actual:\n");
    ref_store_dump(actual, stderr);
}

int main(void)
{
    const char *env;
    unsigned long total;
    unsigned long seed;
    int failed;

    env = getenv("DL89_DIFF_N");
    total = 10000;
    if (env != NULL)
    {
        total = strtoul(env, NULL, 10);
    }
    failed = 0;
    for (seed = 1; seed <= total; ++seed)
    {
        gen_case c;
        ref_store *forward;
        ref_store *reversed;
        ref_store *oracle;
        dl89_rule rules[GEN_MAX_RULES];
        size_t i;
        int st;

        gen_build(&c, seed);
        forward = ref_store_new();
        reversed = ref_store_new();
        oracle = ref_store_new();
        if (seed_store(forward, &c, 0) != 0)
        {
            fprintf(stderr, "seed %lu: seed_store failed\n", seed);
            return 1;
        }
        if (seed_store(reversed, &c, 1) != 0)
        {
            fprintf(stderr, "seed %lu: seed_store failed\n", seed);
            return 1;
        }
        if (seed_store(oracle, &c, 0) != 0)
        {
            fprintf(stderr, "seed %lu: seed_store failed\n", seed);
            return 1;
        }
        st = run_program(forward, &c, 0);
        if (st != DL89_OK)
        {
            fprintf(stderr, "seed %lu: forward run => %d\n", seed, st);
            return 1;
        }
        st = run_program(reversed, &c, 1);
        if (st != DL89_OK)
        {
            fprintf(stderr, "seed %lu: reversed run => %d\n", seed, st);
            return 1;
        }
        for (i = 0; i < c.rule_count; ++i)
        {
            rules[i] = c.rules[i].rule;
        }
        if (ref_eval_run(ref_store_dl89(oracle), rules, c.rule_count) != 0)
        {
            fprintf(stderr, "seed %lu: reference evaluator failed\n", seed);
            return 1;
        }
        if (ref_store_equals(forward, oracle) == 0)
        {
            report(&c, oracle, forward, seed);
            failed = 1;
        }
        if (ref_store_equals(reversed, oracle) == 0)
        {
            report(&c, oracle, reversed, seed);
            failed = 1;
        }
        ref_store_free(forward);
        ref_store_free(reversed);
        ref_store_free(oracle);
        if (failed != 0)
        {
            return 1;
        }
    }
    printf("order differential: %lu programs OK\n", total);
    return 0;
}

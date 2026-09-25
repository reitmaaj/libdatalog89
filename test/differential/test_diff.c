/* test_diff.c - generated differential testing against the reference
 * evaluator. DL89_DIFF_N selects the corpus size (default 10000). */

#include <stdio.h>
#include <stdlib.h>

#include <dl89.h>

#include "gen.h"
#include "ref_eval.h"
#include "ref_store.h"

static int seed_store(ref_store *store, const gen_case *c)
{
    dl89_store s;
    size_t i;
    int inserted;

    s = ref_store_dl89(store);
    for (i = 0; i < c->fact_count; ++i)
    {
        inserted = 0;
        if (s.ops->insert(s.ctx, c->fact_relations[i], c->fact_arities[i],
                          c->facts[i], &inserted) != 0)
        {
            return 1;
        }
    }
    return 0;
}

static void report(const gen_case *c, const ref_store *expected,
                   const ref_store *actual)
{
    fprintf(stderr, "differential failure\n");
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
        ref_store *actual;
        ref_store *expected;
        dl89_eval_config config;
        dl89_eval *eval;
        dl89_rule rules[GEN_MAX_RULES];
        size_t i;
        int st;

        gen_build(&c, seed);
        actual = ref_store_new();
        expected = ref_store_new();
        if (seed_store(actual, &c) != 0)
        {
            fprintf(stderr, "seed %lu: seed_store failed\n", seed);
            return 1;
        }
        if (seed_store(expected, &c) != 0)
        {
            fprintf(stderr, "seed %lu: seed_store failed\n", seed);
            return 1;
        }
        for (i = 0; i < c.rule_count; ++i)
        {
            rules[i] = c.rules[i].rule;
        }
        config.store = ref_store_dl89(actual);
        eval = NULL;
        st = dl89_eval_create(&config, &eval);
        if (st != DL89_OK)
        {
            fprintf(stderr, "seed %lu: create => %d\n", seed, st);
            return 1;
        }
        for (i = 0; i < c.rule_count; ++i)
        {
            st = dl89_eval_add_rule(eval, &rules[i]);
            if (st != DL89_OK)
            {
                fprintf(stderr, "seed %lu: add_rule %lu => %d\n", seed,
                        (unsigned long)i, st);
                gen_print(&c, stderr);
                return 1;
            }
        }
        st = dl89_eval_run(eval);
        if (st != DL89_OK)
        {
            fprintf(stderr, "seed %lu: run => %d\n", seed, st);
            return 1;
        }
        if (ref_eval_run(ref_store_dl89(expected), rules, c.rule_count) != 0)
        {
            fprintf(stderr, "seed %lu: reference evaluator failed\n", seed);
            return 1;
        }
        if (ref_store_equals(actual, expected) == 0)
        {
            report(&c, expected, actual);
            failed = 1;
            dl89_eval_destroy(eval);
            ref_store_free(actual);
            ref_store_free(expected);
            break;
        }
        dl89_eval_destroy(eval);
        ref_store_free(actual);
        ref_store_free(expected);
    }
    if (failed != 0)
    {
        return 1;
    }
    printf("differential: %lu programs OK\n", total);
    return 0;
}

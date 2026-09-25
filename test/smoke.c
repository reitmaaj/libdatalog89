/* smoke.c - one end-to-end path: create, add fact, add rule, run, query. */

#include <dl89.h>

#include <test.h>

#include "ref_store.h"

int dl89_test_failures = 0;

int main(void)
{
    ref_store *store;
    dl89_store dl_store;
    dl89_eval_config config;
    dl89_eval *eval;
    dl89_term head_terms[1];
    dl89_term body_terms[1];
    dl89_atom body[1];
    dl89_rule rule;
    dl89_const fact[1];
    dl89_const check[1];

    store = ref_store_new();
    T_ASSERT(store != NULL);
    if (store == NULL)
    {
        return 1;
    }
    dl_store = ref_store_dl89(store);
    config.store = dl_store;
    eval = NULL;
    T_STATUS(dl89_eval_create(&config, &eval), DL89_OK);
    T_ASSERT(eval != NULL);
    if (eval == NULL)
    {
        ref_store_free(store);
        return 1;
    }

    fact[0] = 101;
    T_STATUS(dl89_eval_add_fact(eval, 9, 1, fact), DL89_OK);

    head_terms[0].kind = DL89_TERM_VAR;
    head_terms[0].u.variable = 1;
    body_terms[0].kind = DL89_TERM_VAR;
    body_terms[0].u.variable = 1;
    body[0].relation = 9;
    body[0].arity = 1;
    body[0].terms = body_terms;
    rule.head.relation = 10;
    rule.head.arity = 1;
    rule.head.terms = head_terms;
    rule.body_count = 1;
    rule.body = body;

    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
    T_STATUS(dl89_eval_run(eval), DL89_OK);

    check[0] = 101;
    T_ASSERT(ref_store_has(store, 10, 1, check) == 1);
    T_EQ_SIZE(ref_store_count(store, 10, 1), 1);
    T_EQ_SIZE(ref_store_scan_opens(store), ref_store_scan_closes(store));

    dl89_eval_destroy(eval);
    ref_store_free(store);
    if (dl89_test_failures == 0)
    {
        return 0;
    }
    return 1;
}

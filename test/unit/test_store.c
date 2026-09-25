/* test_store.c - store abstraction, enumeration order, binding patterns. */

#include <dl89.h>

#include <test.h>

#include "build.h"
#include "expect.h"
#include "fault_store.h"
#include "hash_store.h"
#include "ref_store.h"
#include "symbols.h"

int dl89_test_failures = 0;

static void seed2(dl89_eval *eval, dl89_rel relation, dl89_const a,
                  dl89_const b)
{
    dl89_const tuple[2];

    tuple[0] = a;
    tuple[1] = b;
    T_STATUS(dl89_eval_add_fact(eval, relation, 2, tuple), DL89_OK);
}

static void add_tc(dl89_eval *eval)
{
    dl89_term base_head[2];
    dl89_term base_body[2];
    dl89_term rec_head[2];
    dl89_term rec_body[4];
    dl89_atom base_head_atom;
    dl89_atom base_body_atom;
    dl89_atom rec_head_atom;
    dl89_atom rec_body_atoms[2];
    dl89_rule rules[2];

    mk_var(&base_head[0], V_X);
    mk_var(&base_head[1], V_Y);
    mk_var(&base_body[0], V_X);
    mk_var(&base_body[1], V_Y);
    mk_atom(&base_head_atom, R_PATH, 2, base_head);
    mk_atom(&base_body_atom, R_EDGE, 2, base_body);
    mk_rule(&rules[0], &base_head_atom, 1, &base_body_atom);

    mk_var(&rec_head[0], V_X);
    mk_var(&rec_head[1], V_Z);
    mk_var(&rec_body[0], V_X);
    mk_var(&rec_body[1], V_Y);
    mk_var(&rec_body[2], V_Y);
    mk_var(&rec_body[3], V_Z);
    mk_atom(&rec_head_atom, R_PATH, 2, rec_head);
    mk_atom(&rec_body_atoms[0], R_PATH, 2, &rec_body[0]);
    mk_atom(&rec_body_atoms[1], R_EDGE, 2, &rec_body[2]);
    mk_rule(&rules[1], &rec_head_atom, 2, rec_body_atoms);

    T_STATUS(dl89_eval_add_rule(eval, &rules[0]), DL89_OK);
    T_STATUS(dl89_eval_add_rule(eval, &rules[1]), DL89_OK);
}

static void add_same_rule(dl89_eval *eval)
{
    dl89_term head_terms[1];
    dl89_term body_terms[2];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;

    mk_var(&head_terms[0], V_X);
    mk_var(&body_terms[0], V_X);
    mk_var(&body_terms[1], V_X);
    mk_atom(&head, R_SAME, 1, head_terms);
    mk_atom(&body[0], R_EDGE, 2, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
}

static void add_const_head_rule(dl89_eval *eval)
{
    dl89_term head_terms[1];
    dl89_term body_terms[2];
    dl89_atom head;
    dl89_atom body[1];
    dl89_rule rule;

    mk_const(&head_terms[0], C_A);
    mk_var(&body_terms[0], V_X);
    mk_var(&body_terms[1], V_Y);
    mk_atom(&head, R_FLAG, 1, head_terms);
    mk_atom(&body[0], R_EDGE, 2, body_terms);
    mk_rule(&rule, &head, 1, body);
    T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
}

static void seed_program(dl89_eval *eval)
{
    seed2(eval, R_EDGE, C_A, C_B);
    seed2(eval, R_EDGE, C_B, C_C);
    seed2(eval, R_EDGE, C_C, C_D);
    seed2(eval, R_EDGE, C_E, C_E);
}

static int scan_all(const dl89_store *store, dl89_rel relation, size_t arity,
                    dl89_const *tuples, size_t max, size_t *out_count)
{
    dl89_scan *scan;
    unsigned char bound[8];
    dl89_const values[8];
    dl89_const tuple[8];
    size_t count;
    size_t p;
    int found;
    int rc;

    for (p = 0; p < 8; ++p)
    {
        bound[p] = 0;
        values[p] = 0;
    }
    scan = NULL;
    rc = store->ops->scan_open(store->ctx, relation, arity, values, bound,
                               &scan);
    if (rc != 0)
    {
        return 0;
    }
    count = 0;
    for (;;)
    {
        found = 0;
        rc = store->ops->scan_next(store->ctx, scan, tuple, &found);
        if (rc != 0)
        {
            store->ops->scan_close(store->ctx, scan);
            return 0;
        }
        if (found == 0)
        {
            break;
        }
        if (count < max)
        {
            for (p = 0; p < arity; ++p)
            {
                tuples[count * 8 + p] = tuple[p];
            }
        }
        count = count + 1;
    }
    store->ops->scan_close(store->ctx, scan);
    *out_count = count;
    return 1;
}

static int relation_matches(const dl89_store *a, const dl89_store *b,
                            dl89_rel relation, size_t arity)
{
    dl89_const tuples[256 * 8];
    size_t count_a;
    size_t count_b;
    size_t i;
    size_t p;
    dl89_const tuple[8];
    const hash_store *hb;

    hb = b->ctx;
    if (scan_all(a, relation, arity, tuples, 256, &count_a) == 0)
    {
        return 0;
    }
    count_b = hash_store_count(hb, relation, arity);
    if (count_a != count_b)
    {
        return 0;
    }
    for (i = 0; i < count_a; ++i)
    {
        for (p = 0; p < arity; ++p)
        {
            tuple[p] = tuples[i * 8 + p];
        }
        if (hash_store_has(hb, relation, arity, tuple) == 0)
        {
            return 0;
        }
    }
    return 1;
}

static void test_alternate_store(void)
{
    ref_store *rs;
    hash_store *hs;
    dl89_store rstore;
    dl89_store hstore;
    dl89_eval_config config;
    dl89_eval *eval;
    dl89_const a[1];

    rs = ref_store_new();
    hs = hash_store_new();
    rstore = ref_store_dl89(rs);
    hstore = hash_store_dl89(hs);
    config.store = rstore;
    eval = NULL;
    T_STATUS(dl89_eval_create(&config, &eval), DL89_OK);
    add_tc(eval);
    add_same_rule(eval);
    add_const_head_rule(eval);
    seed_program(eval);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    dl89_eval_destroy(eval);

    config.store = hstore;
    eval = NULL;
    T_STATUS(dl89_eval_create(&config, &eval), DL89_OK);
    add_tc(eval);
    add_same_rule(eval);
    add_const_head_rule(eval);
    seed_program(eval);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    dl89_eval_destroy(eval);

    T_ASSERT(relation_matches(&rstore, &hstore, R_PATH, 2) == 1);
    T_ASSERT(relation_matches(&rstore, &hstore, R_SAME, 1) == 1);
    T_ASSERT(relation_matches(&rstore, &hstore, R_FLAG, 1) == 1);
    a[0] = C_A;
    expect_tuple(rs, R_FLAG, 1, a);
    T_ASSERT(hash_store_has(hs, R_FLAG, 1, a) == 1);
    T_EQ_SIZE(ref_store_scan_opens(rs), ref_store_scan_closes(rs));
    T_EQ_SIZE(hash_store_scan_opens(hs), hash_store_scan_closes(hs));
    ref_store_free(rs);
    hash_store_free(hs);
}

static void run_order_program(ref_store *store, int order, unsigned long seed)
{
    dl89_eval_config config;
    dl89_eval *eval;

    config.store = ref_store_dl89(store);
    eval = NULL;
    T_STATUS(dl89_eval_create(&config, &eval), DL89_OK);
    add_tc(eval);
    add_same_rule(eval);
    add_const_head_rule(eval);
    seed_program(eval);
    ref_store_set_order(store, order);
    ref_store_set_seed(store, seed);
    T_STATUS(dl89_eval_run(eval), DL89_OK);
    dl89_eval_destroy(eval);
}

static void test_enumeration_order(void)
{
    ref_store *forward;
    ref_store *reverse;
    ref_store *random;
    int order;

    forward = ref_store_new();
    reverse = ref_store_new();
    random = ref_store_new();
    run_order_program(forward, REF_ORDER_FORWARD, 1);
    run_order_program(reverse, REF_ORDER_REVERSE, 1);
    run_order_program(random, REF_ORDER_RANDOM, 12345);
    T_ASSERT(ref_store_equals(forward, reverse) == 1);
    T_ASSERT(ref_store_equals(forward, random) == 1);
    for (order = 0; order < 3; ++order)
    {
        ref_store *which;

        which = forward;
        if (order == 1)
        {
            which = reverse;
        }
        if (order == 2)
        {
            which = random;
        }
        T_EQ_SIZE(ref_store_scan_opens(which), ref_store_scan_closes(which));
    }
    ref_store_free(forward);
    ref_store_free(reverse);
    ref_store_free(random);
}

static void seed_rel3(dl89_eval *eval)
{
    dl89_const tuple[3];

    tuple[0] = C_A;
    tuple[1] = C_B;
    tuple[2] = C_C;
    T_STATUS(dl89_eval_add_fact(eval, R_JOINED, 3, tuple), DL89_OK);
    tuple[0] = C_A;
    tuple[1] = C_B;
    tuple[2] = C_D;
    T_STATUS(dl89_eval_add_fact(eval, R_JOINED, 3, tuple), DL89_OK);
    tuple[0] = C_A;
    tuple[1] = C_E;
    tuple[2] = C_C;
    T_STATUS(dl89_eval_add_fact(eval, R_JOINED, 3, tuple), DL89_OK);
    tuple[0] = C_D;
    tuple[1] = C_B;
    tuple[2] = C_C;
    T_STATUS(dl89_eval_add_fact(eval, R_JOINED, 3, tuple), DL89_OK);
    tuple[0] = C_A;
    tuple[1] = C_B;
    tuple[2] = C_E;
    T_STATUS(dl89_eval_add_fact(eval, R_JOINED, 3, tuple), DL89_OK);
    tuple[0] = C_D;
    tuple[1] = C_E;
    tuple[2] = C_D;
    T_STATUS(dl89_eval_add_fact(eval, R_JOINED, 3, tuple), DL89_OK);
}

static void build_pattern_rule(dl89_rule *rule, dl89_term *head_terms,
                               dl89_term *body_terms, dl89_atom *head,
                               dl89_atom *body, int pattern)
{
    int p;

    for (p = 0; p < 3; ++p)
    {
        int bound;

        bound = (pattern >> p) & 1;
        if (bound != 0)
        {
            if (p == 0)
            {
                mk_const(&body_terms[p], C_A);
            }
            if (p == 1)
            {
                mk_const(&body_terms[p], C_B);
            }
            if (p == 2)
            {
                mk_const(&body_terms[p], C_C);
            }
            mk_const(&head_terms[p], body_terms[p].u.constant);
        }
        else
        {
            mk_var(&body_terms[p], (dl89_var)(p + 1));
            mk_var(&head_terms[p], (dl89_var)(p + 1));
        }
    }
    mk_atom(head, R_R, 3, head_terms);
    mk_atom(body, R_JOINED, 3, body_terms);
    mk_rule(rule, head, 1, body);
}

static void test_binding_patterns(void)
{
    static const size_t expected[8] = {6, 4, 4, 3, 3, 2, 2, 1};
    int pattern;

    for (pattern = 0; pattern < 8; ++pattern)
    {
        ref_store *rs;
        fault_store *fs;
        dl89_eval_config config;
        dl89_eval *eval;
        dl89_term head_terms[3];
        dl89_term body_terms[3];
        dl89_atom head;
        dl89_atom body[1];
        dl89_rule rule;

        rs = ref_store_new();
        fs = fault_store_new(rs);
        fault_store_set_validating(fs, 1);
        config.store = fault_store_dl89(fs);
        eval = NULL;
        T_STATUS(dl89_eval_create(&config, &eval), DL89_OK);
        build_pattern_rule(&rule, head_terms, body_terms, &head, body, pattern);
        T_STATUS(dl89_eval_add_rule(eval, &rule), DL89_OK);
        seed_rel3(eval);
        T_STATUS(dl89_eval_run(eval), DL89_OK);
        expect_count(rs, R_R, 3, expected[pattern]);
        T_EQ_SIZE(fault_store_arg_errors(fs), 0);
        T_EQ_SIZE(fault_store_live_scans(fs), 0);
        T_EQ_SIZE(fault_store_total_opens(fs), fault_store_total_closes(fs));
        dl89_eval_destroy(eval);
        fault_store_free(fs);
        ref_store_free(rs);
    }
}

int main(void)
{
    test_alternate_store();
    test_enumeration_order();
    test_binding_patterns();
    if (dl89_test_failures == 0)
    {
        return 0;
    }
    return 1;
}

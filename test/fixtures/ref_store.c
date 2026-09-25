/* ref_store.c - deliberately simple reference relational store.
 *
 * Membership is a flat array with linear lookup (set semantics). Scans
 * materialize the matching tuple indexes and may enumerate them forward,
 * backward, or in a deterministic pseudo-random order. The store is not
 * optimized; it exists to make semantics obvious and to serve as the
 * independent oracle store for differential tests.
 */

#include <stdlib.h>
#include <string.h>

#include "ref_store.h"

typedef struct
{
    dl89_rel relation;
    size_t arity;
    dl89_const *tuple;
} ref_tuple;

struct dl89_scan
{
    ref_store *store;
    size_t *indexes;
    size_t count;
    size_t pos;
};

struct ref_store
{
    ref_tuple *tuples;
    size_t count;
    size_t cap;
    int order;
    unsigned long seed;
    unsigned long scan_opens;
    unsigned long scan_closes;
    unsigned long insert_calls;
    unsigned long new_inserts;
};

static int tuple_equal(const ref_tuple *tuple, dl89_rel relation, size_t arity,
                       const dl89_const *values)
{
    if (tuple->relation != relation)
    {
        return 0;
    }
    if (tuple->arity != arity)
    {
        return 0;
    }
    if (arity == 0)
    {
        return 1;
    }
    return memcmp(tuple->tuple, values, arity * sizeof(dl89_const)) == 0;
}

static int tuple_matches(const ref_tuple *tuple, dl89_rel relation,
                         size_t arity, const dl89_const *values,
                         const unsigned char *bound)
{
    size_t p;

    if (tuple->relation != relation)
    {
        return 0;
    }
    if (tuple->arity != arity)
    {
        return 0;
    }
    if (arity == 0)
    {
        return 1;
    }
    for (p = 0; p < arity; ++p)
    {
        if (bound[p] != 0)
        {
            if (tuple->tuple[p] != values[p])
            {
                return 0;
            }
        }
    }
    return 1;
}

static int ref_store_insert(void *ctx, dl89_rel relation, size_t arity,
                            const dl89_const *tuple, int *inserted)
{
    ref_store *store;
    ref_tuple *grown;
    dl89_const *copy;
    size_t cap;
    size_t i;

    store = ctx;
    store->insert_calls = store->insert_calls + 1;
    for (i = 0; i < store->count; ++i)
    {
        if (tuple_equal(&store->tuples[i], relation, arity, tuple) != 0)
        {
            *inserted = 0;
            return 0;
        }
    }
    if (store->count == store->cap)
    {
        cap = store->cap;
        if (cap == 0)
        {
            cap = 8;
        }
        else
        {
            cap = cap * 2;
        }
        grown = realloc(store->tuples, cap * sizeof(*grown));
        if (grown == NULL)
        {
            return 1;
        }
        store->tuples = grown;
        store->cap = cap;
    }
    copy = NULL;
    if (arity > 0)
    {
        copy = malloc(arity * sizeof(*copy));
        if (copy == NULL)
        {
            return 1;
        }
        memcpy(copy, tuple, arity * sizeof(*copy));
    }
    store->tuples[store->count].relation = relation;
    store->tuples[store->count].arity = arity;
    store->tuples[store->count].tuple = copy;
    store->count = store->count + 1;
    store->new_inserts = store->new_inserts + 1;
    *inserted = 1;
    return 0;
}

static void order_indexes(struct dl89_scan *scan)
{
    size_t i;
    size_t j;
    size_t tmp;
    unsigned long state;

    if (scan->store->order == REF_ORDER_REVERSE)
    {
        for (i = 0; i < scan->count / 2; ++i)
        {
            j = scan->count - 1 - i;
            tmp = scan->indexes[i];
            scan->indexes[i] = scan->indexes[j];
            scan->indexes[j] = tmp;
        }
        return;
    }
    if (scan->store->order == REF_ORDER_RANDOM)
    {
        state = scan->store->seed;
        for (i = scan->count; i > 1; --i)
        {
            state = state * 1103515245UL + 12345UL;
            j = (size_t)(state % i);
            tmp = scan->indexes[i - 1];
            scan->indexes[i - 1] = scan->indexes[j];
            scan->indexes[j] = tmp;
        }
    }
}

static int ref_store_scan_open(void *ctx, dl89_rel relation, size_t arity,
                               const dl89_const *values,
                               const unsigned char *bound, dl89_scan **out)
{
    ref_store *store;
    struct dl89_scan *scan;
    size_t i;
    size_t n;
    size_t k;

    store = ctx;
    scan = malloc(sizeof(*scan));
    if (scan == NULL)
    {
        return 1;
    }
    scan->store = store;
    scan->indexes = NULL;
    scan->count = 0;
    scan->pos = 0;
    n = 0;
    for (i = 0; i < store->count; ++i)
    {
        if (tuple_matches(&store->tuples[i], relation, arity, values, bound) !=
            0)
        {
            n = n + 1;
        }
    }
    if (n > 0)
    {
        scan->indexes = malloc(n * sizeof(*scan->indexes));
        if (scan->indexes == NULL)
        {
            free(scan);
            return 1;
        }
    }
    k = 0;
    for (i = 0; i < store->count; ++i)
    {
        if (tuple_matches(&store->tuples[i], relation, arity, values, bound) !=
            0)
        {
            scan->indexes[k] = i;
            k = k + 1;
        }
    }
    scan->count = k;
    order_indexes(scan);
    store->scan_opens = store->scan_opens + 1;
    *out = scan;
    return 0;
}

static int ref_store_scan_next(void *ctx, dl89_scan *scan_ptr,
                               dl89_const *tuple, int *found)
{
    struct dl89_scan *scan;
    const ref_tuple *entry;

    (void)ctx;
    scan = scan_ptr;
    if (scan->pos >= scan->count)
    {
        *found = 0;
        return 0;
    }
    entry = &scan->store->tuples[scan->indexes[scan->pos]];
    if (entry->arity > 0)
    {
        memcpy(tuple, entry->tuple, entry->arity * sizeof(dl89_const));
    }
    scan->pos = scan->pos + 1;
    *found = 1;
    return 0;
}

static void ref_store_scan_close(void *ctx, dl89_scan *scan_ptr)
{
    struct dl89_scan *scan;

    (void)ctx;
    scan = scan_ptr;
    scan->store->scan_closes = scan->store->scan_closes + 1;
    free(scan->indexes);
    free(scan);
}

static const dl89_store_ops ref_store_ops = {
    ref_store_insert, ref_store_scan_open, ref_store_scan_next,
    ref_store_scan_close};

ref_store *ref_store_new(void)
{
    ref_store *store;

    store = malloc(sizeof(*store));
    if (store == NULL)
    {
        return NULL;
    }
    store->tuples = NULL;
    store->count = 0;
    store->cap = 0;
    store->order = REF_ORDER_FORWARD;
    store->seed = 1;
    store->scan_opens = 0;
    store->scan_closes = 0;
    store->insert_calls = 0;
    store->new_inserts = 0;
    return store;
}

void ref_store_free(ref_store *store)
{
    size_t i;

    if (store == NULL)
    {
        return;
    }
    for (i = 0; i < store->count; ++i)
    {
        free(store->tuples[i].tuple);
    }
    free(store->tuples);
    free(store);
}

void ref_store_set_order(ref_store *store, int order)
{
    store->order = order;
}

void ref_store_set_seed(ref_store *store, unsigned long seed)
{
    store->seed = seed;
}

dl89_store ref_store_dl89(ref_store *store)
{
    dl89_store result;

    result.ctx = store;
    result.ops = &ref_store_ops;
    return result;
}

int ref_store_has(const ref_store *store, dl89_rel relation, size_t arity,
                  const dl89_const *tuple)
{
    size_t i;

    for (i = 0; i < store->count; ++i)
    {
        if (tuple_equal(&store->tuples[i], relation, arity, tuple) != 0)
        {
            return 1;
        }
    }
    return 0;
}

size_t ref_store_count(const ref_store *store, dl89_rel relation, size_t arity)
{
    size_t i;
    size_t n;

    n = 0;
    for (i = 0; i < store->count; ++i)
    {
        if (store->tuples[i].relation == relation)
        {
            if (store->tuples[i].arity == arity)
            {
                n = n + 1;
            }
        }
    }
    return n;
}

size_t ref_store_total(const ref_store *store)
{
    return store->count;
}

int ref_store_equals(const ref_store *a, const ref_store *b)
{
    size_t i;

    if (a->count != b->count)
    {
        return 0;
    }
    for (i = 0; i < a->count; ++i)
    {
        if (ref_store_has(b, a->tuples[i].relation, a->tuples[i].arity,
                          a->tuples[i].tuple) == 0)
        {
            return 0;
        }
    }
    return 1;
}

void ref_store_dump(const ref_store *store, FILE *out)
{
    size_t i;
    size_t p;

    for (i = 0; i < store->count; ++i)
    {
        fprintf(out, "  r%lu/%lu (", (unsigned long)store->tuples[i].relation,
                (unsigned long)store->tuples[i].arity);
        for (p = 0; p < store->tuples[i].arity; ++p)
        {
            if (p > 0)
            {
                fprintf(out, ", ");
            }
            fprintf(out, "%lu", (unsigned long)store->tuples[i].tuple[p]);
        }
        fprintf(out, ")\n");
    }
}

unsigned long ref_store_scan_opens(const ref_store *store)
{
    return store->scan_opens;
}

unsigned long ref_store_scan_closes(const ref_store *store)
{
    return store->scan_closes;
}

unsigned long ref_store_insert_calls(const ref_store *store)
{
    return store->insert_calls;
}

unsigned long ref_store_new_inserts(const ref_store *store)
{
    return store->new_inserts;
}

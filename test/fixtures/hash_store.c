/* hash_store.c - indexed in-memory store (open-addressing membership plus
 * per-relation linked groups and lazily sorted per-position orders).
 *
 * A second, independently structured store used to prove the evaluator does
 * not depend on enumeration behavior. Bound scans use a binary-searched
 * per-position order so stress workloads stay tractable. */

#include <stdlib.h>
#include <string.h>

#include "hash_store.h"

typedef struct
{
    dl89_rel relation;
    size_t arity;
    dl89_const *tuple;
} hs_entry;

typedef struct
{
    dl89_rel relation;
    size_t arity;
    size_t head;
    size_t tail;
    size_t **orders;
    size_t *order_counts;
    size_t *order_caps;
    unsigned char *sorted;
} hs_group;

struct dl89_scan
{
    hash_store *store;
    size_t *indexes;
    size_t count;
    size_t pos;
};

struct hash_store
{
    hs_entry *entries;
    size_t count;
    size_t cap;
    size_t *next;
    size_t next_cap;
    hs_group *groups;
    size_t group_count;
    size_t group_cap;
    size_t *slots;
    size_t slot_cap;
    size_t slot_count;
    unsigned long scan_opens;
    unsigned long scan_closes;
};

static const hash_store *sort_store;
static size_t sort_pos;

static size_t hash_ulong(size_t h, unsigned long value)
{
    size_t i;

    for (i = 0; i < sizeof(value); ++i)
    {
        h = h ^ ((value >> (i * 8)) & 0xffUL);
        h = h * 16777619UL;
    }
    return h;
}

static size_t hash_key(dl89_rel relation, size_t arity, const dl89_const *tuple)
{
    size_t h;
    size_t i;

    h = 2166136261UL;
    h = hash_ulong(h, (unsigned long)relation);
    h = hash_ulong(h, (unsigned long)arity);
    for (i = 0; i < arity; ++i)
    {
        h = hash_ulong(h, (unsigned long)tuple[i]);
    }
    return h;
}

static int entry_equal(const hs_entry *entry, dl89_rel relation, size_t arity,
                       const dl89_const *tuple)
{
    if (entry->relation != relation)
    {
        return 0;
    }
    if (entry->arity != arity)
    {
        return 0;
    }
    if (arity == 0)
    {
        return 1;
    }
    return memcmp(entry->tuple, tuple, arity * sizeof(dl89_const)) == 0;
}

static int table_rehash(hash_store *store, size_t cap)
{
    size_t *slots;
    size_t i;

    slots = malloc(cap * sizeof(*slots));
    if (slots == NULL)
    {
        return 1;
    }
    for (i = 0; i < cap; ++i)
    {
        slots[i] = 0;
    }
    free(store->slots);
    store->slots = slots;
    store->slot_cap = cap;
    store->slot_count = 0;
    for (i = 0; i < store->count; ++i)
    {
        size_t pos;

        pos = hash_key(store->entries[i].relation, store->entries[i].arity,
                       store->entries[i].tuple) %
              store->slot_cap;
        while (store->slots[pos] != 0)
        {
            pos = pos + 1;
            if (pos == store->slot_cap)
            {
                pos = 0;
            }
        }
        store->slots[pos] = i + 1;
        store->slot_count = store->slot_count + 1;
    }
    return 0;
}

static int table_grow(hash_store *store)
{
    size_t cap;

    cap = store->slot_cap;
    if (cap == 0)
    {
        cap = 16;
    }
    else
    {
        cap = cap * 2;
    }
    if (cap < store->slot_cap)
    {
        return 1;
    }
    return table_rehash(store, cap);
}

static size_t table_find(const hash_store *store, dl89_rel relation,
                         size_t arity, const dl89_const *tuple)
{
    size_t pos;

    if (store->slot_cap == 0)
    {
        return 0;
    }
    pos = hash_key(relation, arity, tuple) % store->slot_cap;
    for (;;)
    {
        size_t slot;

        slot = store->slots[pos];
        if (slot == 0)
        {
            return 0;
        }
        if (entry_equal(&store->entries[slot - 1], relation, arity, tuple) != 0)
        {
            return slot;
        }
        pos = pos + 1;
        if (pos == store->slot_cap)
        {
            pos = 0;
        }
    }
}

static int table_reserve(hash_store *store)
{
    if ((store->slot_count + 1) * 4 > store->slot_cap * 3)
    {
        return table_grow(store);
    }
    return 0;
}

static int table_put(hash_store *store, size_t index)
{
    size_t pos;

    if (table_reserve(store) != 0)
    {
        return 1;
    }
    pos = hash_key(store->entries[index].relation, store->entries[index].arity,
                   store->entries[index].tuple) %
          store->slot_cap;
    while (store->slots[pos] != 0)
    {
        pos = pos + 1;
        if (pos == store->slot_cap)
        {
            pos = 0;
        }
    }
    store->slots[pos] = index + 1;
    store->slot_count = store->slot_count + 1;
    return 0;
}

static size_t group_find(const hash_store *store, dl89_rel relation,
                         size_t arity)
{
    size_t i;

    for (i = 0; i < store->group_count; ++i)
    {
        if (store->groups[i].relation == relation)
        {
            if (store->groups[i].arity == arity)
            {
                return i;
            }
        }
    }
    return (size_t)-1;
}

static void group_free_orders(hs_group *group)
{
    size_t p;

    if (group->orders != NULL)
    {
        for (p = 0; p < group->arity; ++p)
        {
            free(group->orders[p]);
        }
    }
    free(group->orders);
    free(group->order_counts);
    free(group->order_caps);
    free(group->sorted);
}

static int group_init_orders(hs_group *group, size_t arity)
{
    group->orders = NULL;
    group->order_counts = NULL;
    group->order_caps = NULL;
    group->sorted = NULL;
    if (arity == 0)
    {
        return 0;
    }
    group->orders = malloc(arity * sizeof(*group->orders));
    group->order_counts = malloc(arity * sizeof(*group->order_counts));
    group->order_caps = malloc(arity * sizeof(*group->order_caps));
    group->sorted = malloc(arity * sizeof(*group->sorted));
    if (group->orders == NULL)
    {
        group_free_orders(group);
        return 1;
    }
    if (group->order_counts == NULL)
    {
        group_free_orders(group);
        return 1;
    }
    if (group->order_caps == NULL)
    {
        group_free_orders(group);
        return 1;
    }
    if (group->sorted == NULL)
    {
        group_free_orders(group);
        return 1;
    }
    {
        size_t p;

        for (p = 0; p < arity; ++p)
        {
            group->orders[p] = NULL;
            group->order_counts[p] = 0;
            group->order_caps[p] = 0;
            group->sorted[p] = 1;
        }
    }
    return 0;
}

static int group_add(hash_store *store, dl89_rel relation, size_t arity,
                     size_t *out)
{
    hs_group *grown;
    size_t cap;

    if (store->group_count == store->group_cap)
    {
        cap = store->group_cap;
        if (cap == 0)
        {
            cap = 8;
        }
        else
        {
            cap = cap * 2;
        }
        grown = realloc(store->groups, cap * sizeof(*grown));
        if (grown == NULL)
        {
            return 1;
        }
        store->groups = grown;
        store->group_cap = cap;
    }
    store->groups[store->group_count].relation = relation;
    store->groups[store->group_count].arity = arity;
    store->groups[store->group_count].head = (size_t)-1;
    store->groups[store->group_count].tail = (size_t)-1;
    if (group_init_orders(&store->groups[store->group_count], arity) != 0)
    {
        return 1;
    }
    *out = store->group_count;
    store->group_count = store->group_count + 1;
    return 0;
}

static int group_find_or_add(hash_store *store, dl89_rel relation, size_t arity,
                             size_t *out)
{
    size_t g;

    g = group_find(store, relation, arity);
    if (g != (size_t)-1)
    {
        *out = g;
        return 0;
    }
    return group_add(store, relation, arity, out);
}

static int entries_grow(hash_store *store)
{
    hs_entry *grown;
    size_t *grown_next;
    size_t cap;

    if (store->count < store->cap)
    {
        return 0;
    }
    cap = store->cap;
    if (cap == 0)
    {
        cap = 8;
    }
    else
    {
        cap = cap * 2;
    }
    grown = realloc(store->entries, cap * sizeof(*grown));
    if (grown == NULL)
    {
        return 1;
    }
    store->entries = grown;
    grown_next = realloc(store->next, cap * sizeof(*grown_next));
    if (grown_next == NULL)
    {
        return 1;
    }
    store->next = grown_next;
    store->next_cap = cap;
    store->cap = cap;
    return 0;
}

static int order_ensure(hs_group *group, size_t p)
{
    size_t *grown;
    size_t cap;

    if (group->order_counts[p] < group->order_caps[p])
    {
        return 0;
    }
    cap = group->order_caps[p];
    if (cap == 0)
    {
        cap = 8;
    }
    else
    {
        cap = cap * 2;
    }
    grown = realloc(group->orders[p], cap * sizeof(*grown));
    if (grown == NULL)
    {
        return 1;
    }
    group->orders[p] = grown;
    group->order_caps[p] = cap;
    return 0;
}

static int order_ready(hash_store *store, size_t g)
{
    hs_group *group;
    size_t p;

    group = &store->groups[g];
    for (p = 0; p < group->arity; ++p)
    {
        if (order_ensure(group, p) != 0)
        {
            return 1;
        }
    }
    return 0;
}

static void order_push(hash_store *store, size_t g, size_t index)
{
    hs_group *group;
    size_t p;

    group = &store->groups[g];
    for (p = 0; p < group->arity; ++p)
    {
        group->orders[p][group->order_counts[p]] = index;
        group->order_counts[p] = group->order_counts[p] + 1;
        group->sorted[p] = 0;
    }
}

static int hash_store_insert(void *ctx, dl89_rel relation, size_t arity,
                             const dl89_const *tuple, int *inserted)
{
    hash_store *store;
    dl89_const *copy;
    size_t g;
    size_t index;

    store = ctx;
    if (table_find(store, relation, arity, tuple) != 0)
    {
        *inserted = 0;
        return 0;
    }
    if (entries_grow(store) != 0)
    {
        return 1;
    }
    if (group_find_or_add(store, relation, arity, &g) != 0)
    {
        return 1;
    }
    if (order_ready(store, g) != 0)
    {
        return 1;
    }
    if (table_reserve(store) != 0)
    {
        return 1;
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
    index = store->count;
    store->entries[index].relation = relation;
    store->entries[index].arity = arity;
    store->entries[index].tuple = copy;
    store->next[index] = (size_t)-1;
    if (store->groups[g].tail == (size_t)-1)
    {
        store->groups[g].head = index;
    }
    else
    {
        store->next[store->groups[g].tail] = index;
    }
    store->groups[g].tail = index;
    store->count = store->count + 1;
    order_push(store, g, index);
    if (table_put(store, index) != 0)
    {
        store->count = store->count - 1;
        free(copy);
        return 1;
    }
    *inserted = 1;
    return 0;
}

static int order_compare(const void *a, const void *b)
{
    size_t ia;
    size_t ib;
    dl89_const va;
    dl89_const vb;

    ia = *(const size_t *)a;
    ib = *(const size_t *)b;
    va = sort_store->entries[ia].tuple[sort_pos];
    vb = sort_store->entries[ib].tuple[sort_pos];
    if (va < vb)
    {
        return -1;
    }
    if (va > vb)
    {
        return 1;
    }
    return 0;
}

static void order_sort(hash_store *store, size_t g, size_t p)
{
    hs_group *group;

    group = &store->groups[g];
    if (group->sorted[p] != 0)
    {
        return;
    }
    sort_store = store;
    sort_pos = p;
    qsort(group->orders[p], group->order_counts[p], sizeof(size_t),
          order_compare);
    group->sorted[p] = 1;
}

static size_t order_lower(const hash_store *store, size_t g, size_t p,
                          dl89_const value)
{
    const hs_group *group;
    size_t lo;
    size_t hi;

    group = &store->groups[g];
    lo = 0;
    hi = group->order_counts[p];
    while (lo < hi)
    {
        size_t mid;

        mid = lo + (hi - lo) / 2;
        if (store->entries[group->orders[p][mid]].tuple[p] < value)
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid;
        }
    }
    return lo;
}

static int scan_matches(const hash_store *store, size_t index,
                        const dl89_const *values, const unsigned char *bound)
{
    const hs_entry *entry;
    size_t p;

    entry = &store->entries[index];
    if (entry->arity == 0)
    {
        return 1;
    }
    for (p = 0; p < entry->arity; ++p)
    {
        if (bound[p] != 0)
        {
            if (entry->tuple[p] != values[p])
            {
                return 0;
            }
        }
    }
    return 1;
}

static size_t first_bound(const unsigned char *bound, size_t arity)
{
    size_t p;

    for (p = 0; p < arity; ++p)
    {
        if (bound[p] != 0)
        {
            return p;
        }
    }
    return (size_t)-1;
}

static int scan_linked(const hash_store *store, size_t g,
                       const dl89_const *values, const unsigned char *bound,
                       struct dl89_scan *scan)
{
    const hs_group *group;
    size_t index;
    size_t n;
    size_t k;

    group = &store->groups[g];
    n = 0;
    index = group->head;
    while (index != (size_t)-1)
    {
        if (scan_matches(store, index, values, bound) != 0)
        {
            n = n + 1;
        }
        index = store->next[index];
    }
    if (n > 0)
    {
        scan->indexes = malloc(n * sizeof(*scan->indexes));
        if (scan->indexes == NULL)
        {
            return 1;
        }
    }
    k = 0;
    index = group->head;
    while (index != (size_t)-1)
    {
        if (scan_matches(store, index, values, bound) != 0)
        {
            scan->indexes[k] = index;
            k = k + 1;
        }
        index = store->next[index];
    }
    scan->count = k;
    return 0;
}

static int scan_bound(hash_store *store, size_t g, const dl89_const *values,
                      const unsigned char *bound, struct dl89_scan *scan)
{
    size_t p;
    size_t lo;
    size_t hi;
    size_t i;
    size_t n;
    size_t k;

    p = first_bound(bound, store->groups[g].arity);
    order_sort(store, g, p);
    lo = order_lower(store, g, p, values[p]);
    hi = store->groups[g].order_counts[p];
    n = 0;
    for (i = lo; i < hi; ++i)
    {
        size_t index;

        index = store->groups[g].orders[p][i];
        if (store->entries[index].tuple[p] != values[p])
        {
            break;
        }
        if (scan_matches(store, index, values, bound) != 0)
        {
            n = n + 1;
        }
    }
    if (n > 0)
    {
        scan->indexes = malloc(n * sizeof(*scan->indexes));
        if (scan->indexes == NULL)
        {
            return 1;
        }
    }
    k = 0;
    for (i = lo; i < hi; ++i)
    {
        size_t index;

        index = store->groups[g].orders[p][i];
        if (store->entries[index].tuple[p] != values[p])
        {
            break;
        }
        if (scan_matches(store, index, values, bound) != 0)
        {
            scan->indexes[k] = index;
            k = k + 1;
        }
    }
    scan->count = k;
    return 0;
}

static int hash_store_scan_open(void *ctx, dl89_rel relation, size_t arity,
                                const dl89_const *values,
                                const unsigned char *bound, dl89_scan **out)
{
    hash_store *store;
    struct dl89_scan *scan;
    size_t g;
    int rc;

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
    g = group_find(store, relation, arity);
    if (g == (size_t)-1)
    {
        store->scan_opens = store->scan_opens + 1;
        *out = scan;
        return 0;
    }
    if (first_bound(bound, arity) == (size_t)-1)
    {
        rc = scan_linked(store, g, values, bound, scan);
    }
    else
    {
        rc = scan_bound(store, g, values, bound, scan);
    }
    if (rc != 0)
    {
        free(scan->indexes);
        free(scan);
        return 1;
    }
    store->scan_opens = store->scan_opens + 1;
    *out = scan;
    return 0;
}

static int hash_store_scan_next(void *ctx, dl89_scan *scan_ptr,
                                dl89_const *tuple, int *found)
{
    struct dl89_scan *scan;
    const hs_entry *entry;

    (void)ctx;
    scan = scan_ptr;
    if (scan->pos >= scan->count)
    {
        *found = 0;
        return 0;
    }
    entry = &scan->store->entries[scan->indexes[scan->pos]];
    if (entry->arity > 0)
    {
        memcpy(tuple, entry->tuple, entry->arity * sizeof(dl89_const));
    }
    scan->pos = scan->pos + 1;
    *found = 1;
    return 0;
}

static void hash_store_scan_close(void *ctx, dl89_scan *scan_ptr)
{
    struct dl89_scan *scan;

    (void)ctx;
    scan = scan_ptr;
    scan->store->scan_closes = scan->store->scan_closes + 1;
    free(scan->indexes);
    free(scan);
}

static const dl89_store_ops hash_store_ops = {
    hash_store_insert, hash_store_scan_open, hash_store_scan_next,
    hash_store_scan_close};

hash_store *hash_store_new(void)
{
    hash_store *store;

    store = malloc(sizeof(*store));
    if (store == NULL)
    {
        return NULL;
    }
    store->entries = NULL;
    store->count = 0;
    store->cap = 0;
    store->next = NULL;
    store->next_cap = 0;
    store->groups = NULL;
    store->group_count = 0;
    store->group_cap = 0;
    store->slots = NULL;
    store->slot_cap = 0;
    store->slot_count = 0;
    store->scan_opens = 0;
    store->scan_closes = 0;
    return store;
}

void hash_store_free(hash_store *store)
{
    size_t i;

    if (store == NULL)
    {
        return;
    }
    for (i = 0; i < store->count; ++i)
    {
        free(store->entries[i].tuple);
    }
    for (i = 0; i < store->group_count; ++i)
    {
        group_free_orders(&store->groups[i]);
    }
    free(store->entries);
    free(store->next);
    free(store->groups);
    free(store->slots);
    free(store);
}

dl89_store hash_store_dl89(hash_store *store)
{
    dl89_store result;

    result.ctx = store;
    result.ops = &hash_store_ops;
    return result;
}

int hash_store_has(const hash_store *store, dl89_rel relation, size_t arity,
                   const dl89_const *tuple)
{
    if (table_find(store, relation, arity, tuple) != 0)
    {
        return 1;
    }
    return 0;
}

size_t hash_store_count(const hash_store *store, dl89_rel relation,
                        size_t arity)
{
    size_t g;
    size_t index;
    size_t n;

    g = group_find(store, relation, arity);
    if (g == (size_t)-1)
    {
        return 0;
    }
    n = 0;
    index = store->groups[g].head;
    while (index != (size_t)-1)
    {
        n = n + 1;
        index = store->next[index];
    }
    return n;
}

size_t hash_store_total(const hash_store *store)
{
    return store->count;
}

int hash_store_equals(const hash_store *a, const hash_store *b)
{
    size_t i;

    if (a->count != b->count)
    {
        return 0;
    }
    for (i = 0; i < a->count; ++i)
    {
        if (table_find(b, a->entries[i].relation, a->entries[i].arity,
                       a->entries[i].tuple) == 0)
        {
            return 0;
        }
    }
    return 1;
}

unsigned long hash_store_scan_opens(const hash_store *store)
{
    return store->scan_opens;
}

unsigned long hash_store_scan_closes(const hash_store *store)
{
    return store->scan_closes;
}

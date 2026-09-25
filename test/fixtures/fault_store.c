/* fault_store.c - fault-injecting, validating wrapper over a store.
 *
 * Tracks open scans (must return to zero), counts callbacks, validates
 * callback arguments, and can fail the nth scan_open, scan_next, or insert
 * once. A hook runs at the start of every scan_open for reentrancy tests.
 */

#include <stdlib.h>

#include "fault_store.h"

struct dl89_scan
{
    dl89_store inner;
    dl89_scan *inner_scan;
    size_t arity;
    int closed;
};

struct fault_store
{
    dl89_store inner;
    unsigned long open_calls;
    unsigned long next_calls;
    unsigned long insert_calls;
    unsigned long fail_open_at;
    unsigned long fail_next_at;
    unsigned long fail_insert_at;
    unsigned long live_scans;
    unsigned long total_opens;
    unsigned long total_closes;
    unsigned long arg_errors;
    void (*hook)(void *ctx);
    void *hook_ctx;
    int validating;
};

static void validate_open(fault_store *fs, size_t arity,
                          const dl89_const *values, const unsigned char *bound)
{
    size_t p;

    if (arity == 0)
    {
        return;
    }
    if (values == NULL)
    {
        fs->arg_errors = fs->arg_errors + 1;
        return;
    }
    if (bound == NULL)
    {
        fs->arg_errors = fs->arg_errors + 1;
        return;
    }
    for (p = 0; p < arity; ++p)
    {
        if (bound[p] != 0)
        {
            if (bound[p] != 1)
            {
                fs->arg_errors = fs->arg_errors + 1;
            }
        }
    }
}

static int fault_insert(void *ctx, dl89_rel relation, size_t arity,
                        const dl89_const *tuple, int *inserted)
{
    fault_store *fs;

    fs = ctx;
    fs->insert_calls = fs->insert_calls + 1;
    if (arity > 0)
    {
        if (tuple == NULL)
        {
            fs->arg_errors = fs->arg_errors + 1;
            return 1;
        }
    }
    if (fs->fail_insert_at != 0)
    {
        if (fs->insert_calls == fs->fail_insert_at)
        {
            return 1;
        }
    }
    return fs->inner.ops->insert(fs->inner.ctx, relation, arity, tuple,
                                 inserted);
}

static int fault_scan_open(void *ctx, dl89_rel relation, size_t arity,
                           const dl89_const *values, const unsigned char *bound,
                           dl89_scan **out)
{
    fault_store *fs;
    struct dl89_scan *scan;
    int rc;

    fs = ctx;
    fs->open_calls = fs->open_calls + 1;
    if (fs->validating != 0)
    {
        validate_open(fs, arity, values, bound);
    }
    if (fs->hook != NULL)
    {
        fs->hook(fs->hook_ctx);
    }
    if (fs->fail_open_at != 0)
    {
        if (fs->open_calls == fs->fail_open_at)
        {
            return 1;
        }
    }
    scan = malloc(sizeof(*scan));
    if (scan == NULL)
    {
        return 1;
    }
    scan->inner = fs->inner;
    scan->inner_scan = NULL;
    scan->arity = arity;
    scan->closed = 0;
    rc = fs->inner.ops->scan_open(fs->inner.ctx, relation, arity, values, bound,
                                  &scan->inner_scan);
    if (rc != 0)
    {
        free(scan);
        return rc;
    }
    fs->live_scans = fs->live_scans + 1;
    fs->total_opens = fs->total_opens + 1;
    *out = scan;
    return 0;
}

static int fault_scan_next(void *ctx, dl89_scan *scan_ptr, dl89_const *tuple,
                           int *found)
{
    fault_store *fs;
    struct dl89_scan *scan;

    fs = ctx;
    scan = scan_ptr;
    fs->next_calls = fs->next_calls + 1;
    if (fs->validating != 0)
    {
        if (scan->arity > 0)
        {
            if (tuple == NULL)
            {
                fs->arg_errors = fs->arg_errors + 1;
            }
        }
    }
    if (fs->fail_next_at != 0)
    {
        if (fs->next_calls == fs->fail_next_at)
        {
            return 1;
        }
    }
    return scan->inner.ops->scan_next(scan->inner.ctx, scan->inner_scan, tuple,
                                      found);
}

static void fault_scan_close(void *ctx, dl89_scan *scan_ptr)
{
    fault_store *fs;
    struct dl89_scan *scan;

    fs = ctx;
    scan = scan_ptr;
    if (scan->closed != 0)
    {
        fs->arg_errors = fs->arg_errors + 1;
        return;
    }
    scan->closed = 1;
    fs->total_closes = fs->total_closes + 1;
    fs->live_scans = fs->live_scans - 1;
    scan->inner.ops->scan_close(scan->inner.ctx, scan->inner_scan);
    free(scan);
}

static const dl89_store_ops fault_store_ops = {
    fault_insert, fault_scan_open, fault_scan_next, fault_scan_close};

fault_store *fault_store_new(ref_store *inner)
{
    fault_store *fs;

    fs = malloc(sizeof(*fs));
    if (fs == NULL)
    {
        return NULL;
    }
    fs->inner = ref_store_dl89(inner);
    fs->open_calls = 0;
    fs->next_calls = 0;
    fs->insert_calls = 0;
    fs->fail_open_at = 0;
    fs->fail_next_at = 0;
    fs->fail_insert_at = 0;
    fs->live_scans = 0;
    fs->total_opens = 0;
    fs->total_closes = 0;
    fs->arg_errors = 0;
    fs->hook = NULL;
    fs->hook_ctx = NULL;
    fs->validating = 0;
    return fs;
}

void fault_store_free(fault_store *fs)
{
    free(fs);
}

dl89_store fault_store_dl89(fault_store *fs)
{
    dl89_store result;

    result.ctx = fs;
    result.ops = &fault_store_ops;
    return result;
}

void fault_store_fail_scan_open(fault_store *fs, unsigned long n)
{
    fs->fail_open_at = n;
}

void fault_store_fail_scan_next(fault_store *fs, unsigned long n)
{
    fs->fail_next_at = n;
}

void fault_store_fail_insert(fault_store *fs, unsigned long n)
{
    fs->fail_insert_at = n;
}

void fault_store_set_hook(fault_store *fs, void (*hook)(void *ctx), void *ctx)
{
    fs->hook = hook;
    fs->hook_ctx = ctx;
}

void fault_store_set_validating(fault_store *fs, int on)
{
    fs->validating = on;
}

unsigned long fault_store_live_scans(const fault_store *fs)
{
    return fs->live_scans;
}

unsigned long fault_store_total_opens(const fault_store *fs)
{
    return fs->total_opens;
}

unsigned long fault_store_total_closes(const fault_store *fs)
{
    return fs->total_closes;
}

unsigned long fault_store_open_calls(const fault_store *fs)
{
    return fs->open_calls;
}

unsigned long fault_store_next_calls(const fault_store *fs)
{
    return fs->next_calls;
}

unsigned long fault_store_insert_calls(const fault_store *fs)
{
    return fs->insert_calls;
}

unsigned long fault_store_arg_errors(const fault_store *fs)
{
    return fs->arg_errors;
}

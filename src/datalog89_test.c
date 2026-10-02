/* datalog89_test.c - test-only plan introspection hooks.
 *
 * Compiled only into test programs; never archived into build/libdatalog89.a.
 * Every hook reads the cached immutable plan, allocates nothing, and
 * validates its arguments. */

#include "datalog89_test.h"
#include "datalog89_priv.h"
#include "datalog89_priv_plan.h"

size_t datalog89_test_plan_builds(const datalog89_eval *eval)
{
    if (eval == NULL)
    {
        return 0;
    }
    return eval->plan_builds;
}

size_t datalog89_test_plan_variant_count(const datalog89_eval *eval)
{
    if (eval == NULL)
    {
        return 0;
    }
    if (eval->plan == NULL)
    {
        return 0;
    }
    return eval->plan->nvariants;
}

datalog89_status
datalog89_test_plan_variant_info(const datalog89_eval *eval, size_t variant,
                                 datalog89_test_variant_info *out)
{
    const struct datalog89_priv_variant *v;

    if (eval == NULL)
    {
        return DATALOG89_EINVAL;
    }
    if (out == NULL)
    {
        return DATALOG89_EINVAL;
    }
    if (eval->plan == NULL)
    {
        return DATALOG89_EINVAL;
    }
    if (variant >= eval->plan->nvariants)
    {
        return DATALOG89_EINVAL;
    }
    v = &eval->plan->variants[variant];
    out->head = v->head;
    out->head_arity = v->head_arity;
    out->nsteps = v->nsteps;
    out->delta_pos = v->delta_pos;
    out->idb_delta = v->idb_delta;
    return DATALOG89_OK;
}

GREEN_PURE
static size_t add_bound(size_t n, const struct datalog89_priv_bind *cols,
                        size_t p)
{
    if (cols[p].kind == (unsigned char)DATALOG89_PRIV_BIND_SLOT)
    {
        return n + 1;
    }
    return n;
}

GREEN_PURE
static size_t add_const(size_t n, const struct datalog89_priv_bind *cols,
                        size_t p)
{
    if (cols[p].kind == (unsigned char)DATALOG89_PRIV_BIND_CONST)
    {
        return n + 1;
    }
    return n;
}

static size_t count_bound(const struct datalog89_priv_bind *cols, size_t arity)
{
    size_t p;
    size_t n;

    n = 0;
    for (p = 0; p < arity; ++p)
    {
        n = add_bound(n, cols, p);
    }
    return n;
}

static size_t count_const(const struct datalog89_priv_bind *cols, size_t arity)
{
    size_t p;
    size_t n;

    n = 0;
    for (p = 0; p < arity; ++p)
    {
        n = add_const(n, cols, p);
    }
    return n;
}

datalog89_status datalog89_test_plan_step_info(const datalog89_eval *eval,
                                               size_t variant, size_t step,
                                               datalog89_test_step_info *out)
{
    const struct datalog89_priv_variant *v;
    const struct datalog89_priv_step *s;

    if (eval == NULL)
    {
        return DATALOG89_EINVAL;
    }
    if (out == NULL)
    {
        return DATALOG89_EINVAL;
    }
    if (eval->plan == NULL)
    {
        return DATALOG89_EINVAL;
    }
    if (variant >= eval->plan->nvariants)
    {
        return DATALOG89_EINVAL;
    }
    v = &eval->plan->variants[variant];
    if (step >= v->nsteps)
    {
        return DATALOG89_EINVAL;
    }
    s = &eval->plan->steps[v->first_step + step];
    out->relation = s->relation;
    out->arity = s->arity;
    out->source = (datalog89_test_src)s->source;
    out->nbound = count_bound(&eval->plan->cols[s->col_off], s->arity);
    out->nconst = count_const(&eval->plan->cols[s->col_off], s->arity);
    return DATALOG89_OK;
}

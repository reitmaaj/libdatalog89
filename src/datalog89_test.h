#ifndef DATALOG89_TEST_HOOKS_H
#define DATALOG89_TEST_HOOKS_H

#include <stddef.h>

#include <datalog89.h>

/* Test-only plan introspection hooks. Compiled only into test programs,
 * never into build/libdatalog89.a. Every hook reads the cached immutable
 * plan, allocates nothing, and validates its arguments. */

typedef enum datalog89_test_src
{
    DATALOG89_TEST_SRC_FULL = 0,
    DATALOG89_TEST_SRC_DELTA = 1
} datalog89_test_src;

typedef struct
{
    datalog89_rel head;
    size_t head_arity;
    size_t nsteps;
    size_t delta_pos;
    int idb_delta;
} datalog89_test_variant_info;

typedef struct
{
    datalog89_rel relation;
    size_t arity;
    datalog89_test_src source;
    size_t nbound;
    size_t nconst;
} datalog89_test_step_info;

/* Number of successful plan builds since evaluator creation. */
size_t datalog89_test_plan_builds(const datalog89_eval *eval);

/* Number of variants in the cached plan; zero when no plan is built. */
size_t datalog89_test_plan_variant_count(const datalog89_eval *eval);

/* Copy one variant's summary; DATALOG89_EINVAL on NULL arguments, a missing
 * plan, or an out-of-range variant index. */
datalog89_status
datalog89_test_plan_variant_info(const datalog89_eval *eval, size_t variant,
                                 datalog89_test_variant_info *out);

/* Copy one step's summary; nbound counts pattern columns bound by earlier
 * steps, nconst counts constant pattern columns. DATALOG89_EINVAL on NULL
 * arguments, a missing plan, or out-of-range variant/step indices. */
datalog89_status datalog89_test_plan_step_info(const datalog89_eval *eval,
                                               size_t variant, size_t step,
                                               datalog89_test_step_info *out);

#endif

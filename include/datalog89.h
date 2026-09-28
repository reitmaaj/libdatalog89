#ifndef DATALOG89_H
#define DATALOG89_H

#include <stddef.h>

typedef unsigned long datalog89_const;
typedef unsigned long datalog89_rel;
typedef unsigned long datalog89_var;

/* Status of a fallible operation (CONVENTIONS.md section 14). DATALOG89_OK is
 * zero; every failure is negative. Store callbacks report success as 0 and
 * failure as nonzero; a nonzero callback result becomes DATALOG89_ESTORE. */
typedef enum datalog89_status
{
    DATALOG89_OK = 0,
    DATALOG89_EINVAL = -1,
    DATALOG89_ENOMEM = -2,
    DATALOG89_EPROGRAM = -3,
    DATALOG89_ESTORE = -4,
    DATALOG89_EBUSY = -5
} datalog89_status;

/* Stable ASCII token, e.g. "OK" or "EINVAL"; "UNKNOWN" for any other value.
 * Immutable static storage; allocation free; total. */
const char *datalog89_status_name(datalog89_status status);

/* Human-readable static text; wording is not part of the contract.
 * Immutable static storage; allocation free; total. */
const char *datalog89_status_message(datalog89_status status);

enum datalog89_term_kind
{
    DATALOG89_TERM_CONST = 1,
    DATALOG89_TERM_VAR = 2
};

typedef struct
{
    int kind;

    union
    {
        datalog89_const constant;
        datalog89_var variable;
    } u;
} datalog89_term;

typedef struct
{
    datalog89_rel relation;
    size_t arity;
    const datalog89_term *terms;
} datalog89_atom;

typedef struct
{
    datalog89_atom head;
    size_t body_count;
    const datalog89_atom *body;
} datalog89_rule;

typedef struct datalog89_scan datalog89_scan;

typedef struct
{
    int (*insert)(void *ctx, datalog89_rel relation, size_t arity,
                  const datalog89_const *tuple, int *inserted);

    int (*scan_open)(void *ctx, datalog89_rel relation, size_t arity,
                     const datalog89_const *values, const unsigned char *bound,
                     datalog89_scan **scan);

    int (*scan_next)(void *ctx, datalog89_scan *scan, datalog89_const *tuple,
                     int *found);

    void (*scan_close)(void *ctx, datalog89_scan *scan);
} datalog89_store_ops;

typedef struct
{
    void *ctx;
    const datalog89_store_ops *ops;
} datalog89_store;

typedef struct
{
    datalog89_store store;
} datalog89_eval_config;

typedef struct datalog89_eval datalog89_eval;

datalog89_status datalog89_eval_create(const datalog89_eval_config *config,
                                       datalog89_eval **out);

void datalog89_eval_destroy(datalog89_eval *eval);

datalog89_status datalog89_eval_add_rule(datalog89_eval *eval,
                                         const datalog89_rule *rule);

datalog89_status datalog89_eval_add_fact(datalog89_eval *eval,
                                         datalog89_rel relation, size_t arity,
                                         const datalog89_const *tuple);

datalog89_status datalog89_eval_run(datalog89_eval *eval);

#endif

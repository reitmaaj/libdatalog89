# libdatalog89 design

## 0001 — frozen public contract and internal architecture

### Frozen API decisions (section 27 of the specification)

- Nullary predicates (arity zero) are supported consistently.
- `datalog89_eval_destroy(NULL)` is a no-op.
- On `datalog89_eval_create` failure, `*out` is set to `NULL` (when `out` is not
  `NULL`).
- `datalog89_eval_add_rule` failure leaves evaluator state unchanged (atomic).
- `datalog89_eval_add_fact` during an active run returns `DATALOG89_EBUSY`.
- `datalog89_eval_add_rule` during an active run returns `DATALOG89_EBUSY`.
- Recursive `datalog89_eval_run` returns `DATALOG89_EBUSY`.
- Store callback failure surfaces as `DATALOG89_ESTORE`; no rollback guarantee.
- Allocation failure surfaces as `DATALOG89_ENOMEM`; `add_rule` is atomic, `run`
  may retain facts already inserted before the failure.
- The relation arity registry covers both rules and `datalog89_eval_add_fact`.
- After every return, all scans opened by `libdatalog89` are closed.
- `datalog89_store_ops.contains` is removed: the evaluator never needs it, because
  `insert` reports newness and scans cover lookups.
- `datalog89_eval_rule_count` is not part of the public surface.

### Public header

Single flat header `include/datalog89.h`:

    datalog89_const / datalog89_rel / datalog89_var = unsigned long
    enum datalog89_status      { OK, EINVAL, ENOMEM, EPROGRAM, ESTORE, EBUSY }
    enum datalog89_term_kind   { DATALOG89_TERM_CONST = 1, DATALOG89_TERM_VAR = 2 }
    datalog89_term, datalog89_atom, datalog89_rule
    datalog89_scan (opaque), datalog89_store_ops, datalog89_store
    datalog89_eval_config { datalog89_store store; }
    datalog89_eval (opaque)
    datalog89_eval_create / destroy / add_rule / add_fact / run

Required store callbacks: `insert`, `scan_open`, `scan_next`, `scan_close`.
`create` rejects a NULL config/out, NULL ops, or any missing required
callback with `DATALOG89_EINVAL`. At arity zero, NULL tuple/values/bound pointers
are valid.

### Module boundaries

- `src/datalog89_priv_mem.c` — default allocator behind the internal memory seam
  (`datalog89_priv_mem_alloc/realloc/free`). Fault tests link
  `test/fixtures/fault_mem.c` instead of this object, so allocation failure
  is injectable without changing the public header.
- `src/datalog89_eval.c` — evaluator lifetime, arity registry, `add_fact`,
  public `run` wrapper, busy flag, plan cache ownership.
- `src/datalog89_rule.c` — structural validation, deep copy/compile, atomic
  installation, plan-cache invalidation.
- `src/datalog89_plan.c` — program plan compilation: per-rule variant
  expansion, static binding patterns, plan build/release (see 0002-plan-design).
- `src/datalog89_join.c` — plan interpreter: pattern-driven scan filling,
  tuple unification, head instantiation, insertion.
- `src/datalog89_fixpoint.c` — least-fixed-point scheduling over precompiled
  variants.
- `src/datalog89_test.c` + `src/datalog89_test.h` — test-only plan
  introspection hooks, compiled only into test programs.

### Data model

`struct datalog89_eval` owns: the store descriptor, a growable vector of compiled
rules, a growable arity registry (`relation -> arity`), the cached immutable
program plan (`struct datalog89_priv_plan`, see 0002-plan-design) with its
build counter, and a `running` flag.

A compiled rule owns deep copies of all terms and atoms, a variable-id to
slot map, an environment (`env` values + `bound` flags per slot), scan
binding scratch (`values`, `bvalues`), a tuple scratch buffer sized to the
largest atom arity, and a log of slots bound while unifying one tuple.

The plan owns contiguous arrays of variants, steps, and static binding
patterns. Each pattern column is CONST (scan-bound constant), SLOT (bound by
an earlier step), EQ (bound by an earlier column of the same atom: free at
scan time, checked during unification), or FREE. The plan is built lazily at
the first run after program mutation, cached across runs, invalidated by
`add_rule`, and released by `destroy`.

Sparse identifiers are handled by linear lookup, never by dense indexing or
sentinels. `0` and `ULONG_MAX` are ordinary values.

### Validation

`add_rule` validates without mutating state: term kinds; nonzero arity
requires a terms array; a zero-body head must be ground; every head variable
must occur in the body; the same relation may not appear with different
arities inside one rule or against the registry; repeated variables are
valid. On success the rule is compiled into temporary storage, new registry
entries are staged, and only then are registry entries and the compiled rule
committed. Any failure rolls back staged registry entries and frees the
compiled rule. `add_fact` checks the registry before insertion and rolls back
a newly registered arity if the store insert fails.

### Evaluation

`run` sets the busy flag, builds the plan when no cached plan exists,
computes the fixed point, and clears the flag on every exit path. Each run
starts fresh. Round 0 evaluates each rule's no-delta seed variant over the
full store, seeding a delta table with newly inserted tuples. Later rounds
evaluate, for each variant whose delta position references a derivable
relation (IDB) and whose delta is nonempty, that variant: its delta step
iterates the previous round's delta while other steps scan the store. The
run stops when a round derives nothing new. This is standard semi-naive
evaluation over statically expanded variants, implemented in
`src/datalog89_fixpoint.c` with per-relation delta tables that grow through the
internal memory seam (so the allocation-failure campaign covers them). The
plan interpreter in `src/datalog89_join.c` walks each variant's steps with
precomputed binding patterns; the evaluator never rediscovers which body
positions are recursive or which scan positions are bound.

Every scan opened for a step is closed exactly once on all paths, including
`scan_open`/`scan_next` failures and nested-body errors; error unwinding
closes deeper scans first.

### Error mapping

- malformed API input (`NULL` pointers, missing callbacks, NULL terms with
  nonzero arity) -> `DATALOG89_EINVAL`
- invalid term kind, unsafe head variable, nonground fact, arity conflict ->
  `DATALOG89_EPROGRAM`
- allocation failure -> `DATALOG89_ENOMEM`
- any nonzero store callback result, or a successful `scan_open` that yields
  a NULL scan -> `DATALOG89_ESTORE`
- mutation or nested run while `running` -> `DATALOG89_EBUSY`

### Test-only architecture

`test/fixtures/` holds independent fixtures: `ref_store` (naive set with
forward/reverse/random enumeration), `hash_store` (a second indexed store
with open-addressing membership, per-relation groups, and lazily sorted
per-position orders for bound scans), `fault_store` (fault injection, scan
accounting, argument validation), `ref_eval` (independent worklist evaluator),
and `fault_mem` (memory seam fault injection). Production sources never share
code with fixtures or with the reference evaluator. Stress tests use direct
mathematical oracles (chain/fan-out/dense closure) because the reference
evaluator is optimized for small generated programs, not large closures.

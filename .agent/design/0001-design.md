# libdl89 design

## 0001 — frozen public contract and internal architecture

### Frozen API decisions (section 27 of the specification)

- Nullary predicates (arity zero) are supported consistently.
- `dl89_eval_destroy(NULL)` is a no-op.
- On `dl89_eval_create` failure, `*out` is set to `NULL` (when `out` is not
  `NULL`).
- `dl89_eval_add_rule` failure leaves evaluator state unchanged (atomic).
- `dl89_eval_add_fact` during an active run returns `DL89_EBUSY`.
- `dl89_eval_add_rule` during an active run returns `DL89_EBUSY`.
- Recursive `dl89_eval_run` returns `DL89_EBUSY`.
- Store callback failure surfaces as `DL89_ESTORE`; no rollback guarantee.
- Allocation failure surfaces as `DL89_ENOMEM`; `add_rule` is atomic, `run`
  may retain facts already inserted before the failure.
- The relation arity registry covers both rules and `dl89_eval_add_fact`.
- After every return, all scans opened by `libdl89` are closed.
- `dl89_store_ops.contains` is removed: the evaluator never needs it, because
  `insert` reports newness and scans cover lookups.
- `dl89_eval_rule_count` is not part of the public surface.

### Public header

Single flat header `include/dl89.h`:

    dl89_const / dl89_rel / dl89_var = unsigned long
    enum dl89_status      { OK, EINVAL, ENOMEM, EPROGRAM, ESTORE, EBUSY }
    enum dl89_term_kind   { DL89_TERM_CONST = 1, DL89_TERM_VAR = 2 }
    dl89_term, dl89_atom, dl89_rule
    dl89_scan (opaque), dl89_store_ops, dl89_store
    dl89_eval_config { dl89_store store; }
    dl89_eval (opaque)
    dl89_eval_create / destroy / add_rule / add_fact / run

Required store callbacks: `insert`, `scan_open`, `scan_next`, `scan_close`.
`create` rejects a NULL config/out, NULL ops, or any missing required
callback with `DL89_EINVAL`. At arity zero, NULL tuple/values/bound pointers
are valid.

### Module boundaries

- `src/dl89_priv_mem.c` — default allocator behind the internal memory seam
  (`dl89_priv_mem_alloc/realloc/free`). Fault tests link
  `test/fixtures/fault_mem.c` instead of this object, so allocation failure
  is injectable without changing the public header.
- `src/dl89_eval.c` — evaluator lifetime, arity registry, `add_fact`,
  public `run` wrapper, busy flag.
- `src/dl89_rule.c` — structural validation, deep copy/compile, atomic
  installation.
- `src/dl89_join.c` — body evaluation: binding construction, tuple
  unification, recursion over body atoms, head instantiation, insertion.
- `src/dl89_fixpoint.c` — least-fixed-point scheduling.

### Data model

`struct dl89_eval` owns: the store descriptor, a growable vector of compiled
rules, a growable arity registry (`relation -> arity`), and a `running` flag.

A compiled rule owns deep copies of all terms and atoms, a variable-id to
slot map, an environment (`env` values + `bound` flags per slot), scan
binding scratch (`values`, `bvalues`), a tuple scratch buffer sized to the
largest atom arity, and a log of slots bound while unifying one tuple.

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

`run` sets the busy flag, computes the fixed point, and clears the flag on
every exit path. Each run starts fresh. Round 0 evaluates every rule over the
full store, seeding a delta table with newly inserted tuples. Later rounds
evaluate, for each rule and each body position whose relation is derivable
(IDB), a variant whose scan at that position iterates the previous round's
delta while other positions scan the full store. The run stops when a round
derives nothing new. This is standard semi-naive evaluation, implemented in
`src/dl89_fixpoint.c` with per-relation delta tables that grow through the
internal memory seam (so the allocation-failure campaign covers them).

Every scan opened for an atom is closed exactly once on all paths, including
`scan_open`/`scan_next` failures and nested-body errors; error unwinding
closes deeper scans first.

### Error mapping

- malformed API input (`NULL` pointers, missing callbacks, NULL terms with
  nonzero arity) -> `DL89_EINVAL`
- invalid term kind, unsafe head variable, nonground fact, arity conflict ->
  `DL89_EPROGRAM`
- allocation failure -> `DL89_ENOMEM`
- any nonzero store callback result, or a successful `scan_open` that yields
  a NULL scan -> `DL89_ESTORE`
- mutation or nested run while `running` -> `DL89_EBUSY`

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

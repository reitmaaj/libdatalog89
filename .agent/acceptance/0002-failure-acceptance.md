# libdl89 acceptance tests — failures, resources, snapshots

## Must exhibit

- S01–S06: every backing-store failure observed by the library surfaces as
  `DL89_ESTORE`; all scans opened by the library are closed exactly once on
  every return path; after any failure `dl89_eval_destroy` produces no leak
  or invalid access. Facts inserted before a failure may remain; no rollback
  is required. Failures injected during a semi-naive delta round propagate
  identically, and a failed run followed by a successful run over the same
  store computes the same closure as a fresh evaluator.
- Allocation failure (section 16): for every allocation site reachable from
  `dl89_eval_create`, `dl89_eval_add_rule`, `dl89_eval_add_fact`, and
  `dl89_eval_run`, failing that allocation exactly once returns
  `DL89_ENOMEM`, leaves no leak, double free, or invalid access, and closes
  all open scans. A failed `add_rule` or `add_fact` installs nothing,
  including arity-registry entries; previously accepted rules remain intact
  and, after fault injection is disabled, still evaluate correctly. This
  includes registry-growth allocations and delta storage growth during a
  recursive run.
- Q01–Q03: `dl89_eval_add_rule`, `dl89_eval_add_fact`, and `dl89_eval_run`
  invoked from a store callback during an active run return `DL89_EBUSY`;
  the outer evaluation proceeds under the original rule snapshot and the
  rejected call has no consequence.
- Q05 (section 25): mutating caller rule buffers during active evaluation
  does not change the result; the compiled copy governs.
- Resource diagnostics (section 24): the deterministic and generated suites
  run under AddressSanitizer, UndefinedBehaviorSanitizer, and
  LeakSanitizer/Valgrind with zero invalid reads, invalid writes,
  use-after-free, double frees, leaks attributable to libdl89, and zero
  leaked scans. The generated differential corpus runs under the same
  instrumentation at a reduced size.
- Release gates: `just test`, `just sanitize`, `just green`, `just baseline`,
  and `just lint` all pass; `main` always passes all testing.

## Must reject

- `DL89_EINVAL` for NULL evaluator/config/output/callback inputs and for a
  NULL tuple with nonzero arity in `dl89_eval_add_fact`.
- `DL89_EPROGRAM` for invalid term kinds, unsafe head variables, nonground
  zero-body heads, and relation arity conflicts.
- `DL89_ESTORE` for any nonzero store callback result and for a successful
  `scan_open` that returns a NULL scan.
- `DL89_ENOMEM` for any failed internal allocation, without partial rule
  installation.
- `DL89_EBUSY` for any mutation or nested run during an active run.

## Not required

No requirement exists for rollback of inserted facts after a store or
allocation failure, resumability after a store error, thread safety,
concurrent evaluator use, or transactional behavior.

# libdatalog89 acceptance tests — compiled plan pipeline

This document supplements 0001-acceptance (core semantics), 0002-failure
(failures/resources), and 0003-status (error surface). It pins structure and
costs of the compiled execution plan. Structural pinning applies only to
test-build introspection (`datalog89_test_*` hooks, compiled solely into test
programs) and to scan-cost bounds observed through fixtures; the public API
never exposes the plan and the semantic suites remain order-agnostic.

## Must exhibit

- SCC: the predicate dependency graph is decomposed into SCCs scheduled in
  topological order. A relation with no rule deriving it belongs to no SCC
  (EDB). A recursive SCC contains every relation of its cycle and every rule
  whose head is one of them. A nonrecursive SCC's rules are each executed
  exactly once per run and participate in no delta round. Every rule of a
  recursive SCC is evaluated once against the full store before the first
  delta round.
- Variants: a rule whose body mentions the SCC's recursive relations at k
  positions compiles to exactly k semi-naive variants; other rules compile to
  exactly one. In variant i the step for body position i carries source
  DELTA, other recursive body atoms carry OLD, and EDB or earlier-SCC atoms
  carry FULL. The DELTA step is the first step of its variant.
- Join order: deterministic; within a variant, remaining steps follow
  most-bound-first with a constant-position tiebreak (a constant position
  counts as already bound). Two compilations of one logical program yield
  identical step sequences.
- Delta-first cost bound: for p(X,Z) :- q(X,Y), p(Y,Z) with EDB q, the full q
  relation is never rescanned per delta tuple; scan work is bounded by a
  constant multiple of the q scan plus matching lookups.
- Plan cache: the plan is built once per program snapshot. run rebuilds only
  after add_rule; add_fact does not invalidate it; destroy releases it; the
  plan is immutable for the duration of a run and its reported structure is
  unchanged afterward.
- Hooks: test-only introspection reports SCC count/classification, per-variant
  step order, source kinds, and plan build count. No datalog89_test_* symbol
  appears in build/libdatalog89.a. Every hook validates its arguments and
  returns DATALOG89_EINVAL for NULL outputs or out-of-range indices.
- Public surface: the public header, status codes, and store callback
  interface are unchanged by the plan pipeline; `just api-convention`,
  `just error-convention`, `just headers`, and `just green` all pass.

## Must reject / must avoid

- A run MUST NOT rescan a full recursive relation for every delta tuple
  (delta-first violation); the PL-12 scan bound is enforced by a fixture
  counting scan calls.
- A plan build MUST NOT leak or corrupt state when any of its allocations
  fails: DATALOG89_ENOMEM, all open scans closed, evaluator reusable on the
  next run after the fault is disabled.
- add_rule while a plan is cached MUST NOT reuse the stale plan; the next run
  MUST rebuild (PL-14) and yield the same closure as a fresh evaluator.
- The plan MUST NOT be observable through the public API or through the
  library archive's exported symbols.

## Deliberately not required

No requirement exists for cost-based optimization, statistics collection,
join-order optimality beyond the stated heuristic, relational-algebra ASTs,
arithmetic in the plan, generated C, JIT compilation, dynamic replanning, or
any new public API. Store-internal index selection remains the store's own
concern; the plan supplies only static binding patterns per scan.

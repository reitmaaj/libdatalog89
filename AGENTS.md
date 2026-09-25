# libdl89 — agentic workflow

`libdl89` is a small, embeddable Datalog evaluation core for strict ISO C89.
It accepts facts and rules through an opaque identifier model and computes the
least fixed point over a caller-supplied relational store. It is a
self-contained sibling git repository (no external dependency).

## Hard constraints

- **ISO C89 only**, clean under the green baseline for GCC and Clang in both
  C89 and C23 modes, plus the green clang-tidy semantic checks and the
  canonical Allman format (`.clang-format`). Verified via the sibling `green`
  driver (`just green` / `just check`).
- **Evaluation only.** `libdl89` owns rule validation, variable binding,
  unification with ground tuples, body joins, head instantiation, recursive
  evaluation, duplicate suppression through store insertion, and fixed-point
  scheduling. It owns nothing else: no syntax, symbols, SQL, persistence,
  transactions, modules, provenance, negation, aggregation, retraction,
  side effects, REPL, or application policy.
- **Opaque identifiers.** `dl89_const`, `dl89_rel`, `dl89_var` are
  `unsigned long`; every value including `0` and `ULONG_MAX` is an ordinary
  identifier. No sentinel values leak into the public model.
- **Abstract store.** The relational store is the only host interface:
  `insert`, `scan_open`, `scan_next`, `scan_close`. The store provides set
  semantics and reports whether an insertion was new.
- **Fixed snapshots.** The rule set is frozen for one `dl89_eval_run()`. Any
  attempt to add a rule, add a fact, or re-enter the evaluator during a run
  returns `DL89_EBUSY`.
- **Failure model.** `DL89_EINVAL` malformed input, `DL89_ENOMEM` allocation
  failure, `DL89_EPROGRAM` invalid rule or arity conflict, `DL89_ESTORE`
  store callback failure, `DL89_EBUSY` mutation during a run. Store failures
  carry no rollback guarantee. `add_rule` is atomic. Every scan opened by the
  library is closed before return.
- Four-space indentation; functional style; most functions short (2-7 lines).
- Public header is a single flat `include/dl89.h`.

## `.agent` directory

Keep concept, stories, design, testing (BDD scenarios), and acceptance
documents under `.agent/{concept,stories,design,testing,acceptance}` with
`NNNN-` names. The project must build and run without `.agent`.

## Test-driven development

Every behavior change follows: scenario in `.agent/testing/*.md` -> failing
test -> minimum code -> refactor. Acceptance tests under `.agent/acceptance/*.md`
cover must-exhibit and must-reject behavior. Coverage order: one end-to-end
smoke test first, then unit tests for every pure function and non-trivial
branch, then broader testing (differential, stress).

## `just` and `make`

Use `just` for all actions: `just build`, `just headers`, `just smoke`,
`just unit`, `just differential`, `just stress`, `just test`, `just sanitize`,
`just green`, `just check`, `just baseline`, `just lint`, `just format`,
`just doctor`, `just clean`. `make` delegates.

## Git

Keep `main` green. Use short-lived working branches. Do not push without
permission.

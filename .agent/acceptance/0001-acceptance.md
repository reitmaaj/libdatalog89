# libdatalog89 acceptance tests — core semantics

The library MUST exhibit the behavior under "must exhibit" and MUST reject,
avoid, refuse, fail safely, or report an error for the behavior under "must
reject". Every semantic test compares complete relation sets (both missing
and extra tuples fail).

## Must exhibit

- B01: a client including only `<datalog89.h>` compiles under
  `cc -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long` with GCC and Clang.
- B02: all production sources compile under the same strict C89 flags with
  zero diagnostics (the green baseline is stronger and is the gate of record).
- B04: a translation unit containing only `<datalog89.h>` and `main` compiles and
  links; no private header leaks through the public interface.
- A01: `datalog89_eval_create` on a valid store returns `DATALOG89_OK`, a non-NULL
  evaluator, and `datalog89_eval_destroy` leaves no leak.
- O01/O02/O03: `datalog89_eval_add_rule` deep-copies rules; caller buffers may be
  overwritten or reused; destroying an evaluator releases the copied program.
- V05/V06/V08/V12/V13: variable-free rules, ground facts, repeated variables,
  zero identifiers, and `ULONG_MAX` identifiers are accepted and behave
  exactly as ordinary identifiers.
- F01–F05: `datalog89_eval_add_fact` has set semantics, exact tuples, persistence
  across runs, and facts added between runs are used on the next run.
- E01–E10: unary copy, constant filtering, constants in heads,
  repeated-variable equality, two-way joins, failed joins, cross products,
  shared variables across three atoms, repeated body relations, and
  duplicate derivations produce exactly the specified relation sets.
- R01–R09: transitive closure, multi-round recursion, cycles, self-loops,
  mutual recursion, branching, recursion with equality constraints, and
  order independence (rules and tuples) produce exact closures and terminate.
- P01–P04: a second run is idempotent; new facts and new rules after closure
  yield full recomputation; successful runs are monotone.
- N01–N04: arity-zero predicates work as facts, bodies, derived predicates,
  and nullary mutual recursion; the store accepts NULL tuple pointers at
  arity zero.
- B-1 (section 13): every scan issued carries a logically valid binding set;
  final semantics are exact regardless of chosen join order.
- Q04: relational encodings of rules are inert data until the caller
  explicitly adds them and runs again.
- T01–T05: identifiers are opaque; sparse identifiers work; two independent
  store implementations agree; reversed and pseudo-random enumeration orders
  produce the same fixed point.
- G01: at least 10,000 generated valid programs evaluate to exactly the
  reference evaluator's least fixed point; failures print a complete
  C-independent reproduction with the seed.
- G01b: the generated corpus also reaches the same fixed point under reversed
  rule insertion order and reversed fact insertion order.
- C01–C04: bounded stress workloads (1000-edge chain, 1000-wide fan-out,
  dense cyclic graph, many irrelevant tuples) produce exact closures and
  terminate.

## Must reject

- A02/A03/A04: `datalog89_eval_create` with NULL config, NULL out, NULL ops, or a
  missing required callback returns `DATALOG89_EINVAL` and sets `*out = NULL`.
- V01/V02: an unknown term kind in the head or any body position returns
  `DATALOG89_EPROGRAM`; no rule is installed.
- V03/V04/V07: an unsafe head variable, a partially unsafe head, or a
  nonground zero-body rule returns `DATALOG89_EPROGRAM`.
- V09/V10/V11: relation arity conflicts inside a rule, across rules, or
  through `datalog89_eval_add_fact` return `DATALOG89_EPROGRAM`; previously accepted
  rules remain installed and usable.
- G02: generated malformed rules return `DATALOG89_EPROGRAM` and leave the
  evaluator state unchanged.
- Purity boundary (U-1): the suite must not interpret an impure caller store
  as a library failure; such behavior is outside the API preconditions.

## Deliberately not required

No test requires a specific join order, scan count, insert-attempt count,
iteration count, semi-naive evaluation, indexing, rule compilation, memory
complexity, tuple enumeration order, transaction rollback, persistence,
thread safety, concurrency, provenance, negation, aggregates, reflective
execution, or performance.

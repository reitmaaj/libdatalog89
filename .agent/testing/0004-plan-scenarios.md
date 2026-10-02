# libdatalog89 testing scenarios (compiled plan pipeline)

These scenarios govern the compiled execution plan: predicate dependency
graph, SCC decomposition, semi-naive variant expansion, join ordering,
immutable plan representation, plan caching, and the test-only introspection
hooks that observe them. They are white-box where noted: the pinned behavior
is observed through `datalog89_test_*` hooks compiled only into test builds,
never through the public API. Semantic suites remain join-order agnostic
(0001-core-scenarios B-1).

Constants follow 0001-core-scenarios: relations EDGE=3 PATH=4 P=9 Q=10
R=11; variables X=1 Y=2 Z=3.

## SCC decomposition

SCENARIO PL-1 SCC classification
    GIVEN path(X,Y) :- edge(X,Y) and path(X,Z) :- path(X,Y), edge(Y,Z)
    WHEN the plan is built
    THEN the plan reports exactly two SCCs in topological order, the first
         containing edge only (recursive=0) and the second containing path
         only (recursive=1)

SCENARIO PL-2 nonrecursive SCC runs each rule once
    GIVEN p(X) :- q(X), q(X) :- r(X), r(X) :- edge(X,A) with facts
    WHEN run completes
    THEN each rule is executed exactly once, no delta round fires, and the
         result equals the reference evaluator's closure

SCENARIO PL-3 mutual recursion shares one SCC
    GIVEN p(X) :- q(X), q(X) :- r(X), r(X) :- p(X)
    WHEN the plan is built
    THEN p, q, and r occupy a single recursive SCC and all three rules belong
         to it

SCENARIO PL-4 SCC topological ordering
    GIVEN r(X) :- p(X), q(X) plus rules deriving p and q
    WHEN the plan is built
    THEN the SCC of r is scheduled strictly after the SCCs of p and q

SCENARIO PL-5 recursive SCC seeding
    GIVEN a recursive SCC with several rules
    WHEN run starts
    THEN every rule of the SCC is evaluated once against the full store before
         any delta round

## Semi-naive variant expansion

SCENARIO PL-6 variant count
    GIVEN a rule whose body mentions the SCC's recursive relations at k
          positions
    WHEN the plan is built
    THEN the rule compiles to exactly k variants, one per recursive position;
         a nonrecursive rule compiles to exactly one variant; a rule with no
         recursive body atom compiles to exactly one variant

SCENARIO PL-7 variant source kinds
    GIVEN variant i of a recursive rule, delta at body position i
    WHEN the plan is inspected
    THEN step i carries source DELTA, every other recursive body atom carries
         OLD, and every EDB or earlier-SCC atom carries FULL

SCENARIO PL-8 delta atom leads the join
    GIVEN any variant with a DELTA step
    WHEN the plan is inspected
    THEN the DELTA step precedes every other step of that variant

## Join ordering

SCENARIO PL-9 most-bound-first ordering
    GIVEN a variant whose DELTA step binds some variables
    WHEN the plan orders the remaining steps
    THEN a step whose pattern binds more already-bound variables precedes one
         that binds fewer; a constant position counts as already bound

SCENARIO PL-10 constant-count tiebreak
    GIVEN two steps with equal bound-variable counts
    WHEN the plan orders them
    THEN the step with more constant positions precedes the other

SCENARIO PL-11 deterministic plan
    GIVEN one logical program compiled twice
    WHEN the two plans are compared step by step
    THEN they are identical

SCENARIO PL-12 recursive atom last is not quadratic
    GIVEN p(X,Z) :- q(X,Y), p(Y,Z) with q an EDB of n tuples and a delta of p
          of d tuples
    WHEN the plan runs
    THEN the DELTA step for p is first and the full q relation is never
         rescanned once per delta tuple; total scan work is bounded by a
         constant multiple of n plus the matching lookups

## Plan caching and immutability

SCENARIO PL-13 plan built once per program snapshot
    GIVEN a program and a first run
    WHEN run is called again without adding rules
    THEN the plan build count does not increase

SCENARIO PL-14 add_rule invalidates the plan
    GIVEN a plan built by a previous run
    WHEN a new rule is added and run is called
    THEN the plan build count increases by exactly one

SCENARIO PL-15 add_fact does not invalidate the plan
    GIVEN a plan built by a previous run
    WHEN only a fact is added and run is called
    THEN the plan build count does not increase

SCENARIO PL-16 plan immutable during a run
    GIVEN a plan built for a run
    WHEN the run completes (successfully or not)
    THEN every structural property the hooks reported before the run is
         unchanged afterward

SCENARIO PL-17 plan released on destroy
    GIVEN an evaluator with a built plan
    WHEN datalog89_eval_destroy is called
    THEN no evaluator-owned allocation remains (ASan/LSan/Valgrind)

## Explicit-frame evaluation

SCENARIO PL-18 long body joins
    GIVEN a rule with a body of at least 32 atoms forming a chain join
    WHEN run completes
    THEN the exact join results and evaluation terminates without relying on
         C recursion depth

## Test-only hooks

SCENARIO PL-19 hooks absent from the library archive
    GIVEN build/libdatalog89.a
    WHEN its exported symbols are inspected
    THEN no datalog89_test_* symbol appears; hooks link only into test
         programs

SCENARIO PL-20 hook argument validation
    GIVEN any introspection hook
    WHEN called with a NULL output or an out-of-range index
    THEN it returns DATALOG89_EINVAL without touching the output

# libdl89 testing scenarios (failures, snapshots, differential)

## Store failure propagation

SCENARIO S-1 scan_open failure
    GIVEN p(X) :- q(X) and a store whose first required scan_open fails
    WHEN run is called
    THEN DL89_ESTORE is returned and no scan remains open

SCENARIO S-2 scan_next failure
    GIVEN a store that fails the first scan_next, or after one successful
          tuple
    WHEN run is called
    THEN DL89_ESTORE is returned and every scan is closed exactly once

SCENARIO S-3 insert failure
    GIVEN a derivation that requires a new fact and a store that fails that
          insert
    WHEN run is called
    THEN DL89_ESTORE is returned and every scan is closed; facts inserted
         before the failure may remain

SCENARIO S-4 failure after nested joins
    GIVEN a three-atom rule and a store failing during the deepest scan
    WHEN run is called
    THEN DL89_ESTORE is returned and every opened scan is closed exactly once

SCENARIO S-5 destructible after failure
    GIVEN any injected store failure
    WHEN dl89_eval_destroy is called
    THEN there is no leak, double free, or invalid access

SCENARIO S-6 failure in a delta round
    GIVEN transitive-closure rules where the first delta-round scan is known
          to be the third scan_open
    WHEN that scan_open fails
    THEN DL89_ESTORE is returned and every opened scan is closed exactly once

SCENARIO S-7 rerun after failure
    GIVEN a run that failed with DL89_ESTORE
    WHEN the fault is disabled and the evaluator runs again over the same
         store
    THEN the closure equals that of a fresh evaluator and the fault store
         reports no leaked scans

SCENARIO S-8 add_fact store-failure rollback
    GIVEN a new relation whose first insert fails
    WHEN dl89_eval_add_fact returns DL89_ESTORE
    THEN the arity registry is unchanged and a rule using another arity for
         that relation is accepted

## Allocation failure

SCENARIO M-1 allocation sweep
    GIVEN the memory seam failing allocation number n, once, for every n
          reachable from create, add_rule, add_fact, and run
    WHEN the operation is attempted
    THEN DL89_ENOMEM is returned, no leak or invalid access occurs, all
         scans are closed, and (for add_rule/add_fact) the failing input is
         not partially installed and previously accepted rules still
         evaluate

SCENARIO M-2 recursive-run and registry-growth sweeps
    GIVEN a recursive program whose run allocates delta storage, and a rule
          that registers new relations
    WHEN every allocation site is failed once
    THEN the recursive run reports DL89_ENOMEM and reruns to the exact
         closure, and the rejected rule leaves the arity registry unchanged

## Snapshot and reentrancy

SCENARIO Q-1 add rule from a store callback
    GIVEN an active run whose store callback calls dl89_eval_add_rule
    WHEN the nested call executes
    THEN it returns DL89_EBUSY, the outer run completes under the original
         rule snapshot, and the rejected rule has no consequence

SCENARIO Q-2 add fact from a store callback
    GIVEN an active run whose store callback calls dl89_eval_add_fact
    WHEN the nested call executes
    THEN it returns DL89_EBUSY

SCENARIO Q-3 recursive run
    GIVEN an active run whose store callback calls dl89_eval_run
    WHEN the nested call executes
    THEN it returns DL89_EBUSY and the outer run remains valid

SCENARIO Q-4 derived rule data is inert
    GIVEN a program deriving tuples that encode a rule
    WHEN the run completes
    THEN those tuples cause no executable behavior unless the caller
         decodes them and explicitly adds rules and runs again

SCENARIO Q-5 rule buffers mutated during execution
    GIVEN caller rule buffers retained by the test
    WHEN a store callback mutates them during an active run
    THEN the result still follows the copied rule

## Store abstraction

SCENARIO T-1 opaque identifiers
    GIVEN arbitrary, sparse, zero, and ULONG_MAX identifiers
    WHEN evaluated
    THEN results depend only on identifier equality

SCENARIO T-2 alternate stores
    GIVEN the core semantic suite against a naive linear store and a
          different indexed store
    WHEN results are compared
    THEN relation sets match exactly

SCENARIO T-3 enumeration order
    GIVEN stores enumerating in insertion order, reverse order, and a fixed
          pseudo-random order
    WHEN evaluated
    THEN the fixed point is identical in all cases

SCENARIO U-1 purity boundary
    GIVEN a store whose membership changes independently during a run
    WHEN evaluating
    THEN the suite does not interpret this caller contract violation as a
         library failure

## Generated differential and stress

SCENARIO G-1 generated valid programs
    GIVEN deterministic seeds generating range-restricted programs over
          1..6 relations, arity 0..3, 1..8 constants, 1..4 variables, 0..8
          rules, 0..20 initial facts, body length 0..4
    WHEN each program is evaluated by the reference evaluator and by
         libdl89 over identical initial databases
    THEN the complete databases are equal; failures print a C-independent
         reproduction with the seed

SCENARIO G-2 generated invalid programs
    GIVEN generated rules with invalid term kinds, unsafe head variables,
          inconsistent arities, or nonground zero-body heads
    WHEN each is added
    THEN DL89_EPROGRAM is returned and previously accepted rules still
         evaluate to an unchanged database

SCENARIO G-3 generated order independence
    GIVEN the generated corpus under reversed rule insertion order and
          reversed fact insertion order
    WHEN each program is evaluated
    THEN both orders reach exactly the reference evaluator's fixed point

SCENARIO C-1 stress correctness
    GIVEN a 1000-edge chain, a 1000-successor fan-out, a dense cyclic graph,
          and relations with many irrelevant tuples
    WHEN evaluated
    THEN selected endpoints and the complete closure match an independent
         oracle and evaluation terminates

SCENARIO Z-1 resource diagnostics
    GIVEN the deterministic and generated suites
    WHEN run under ASan/UBSan/LSan or Valgrind
    THEN there are no invalid reads/writes, use-after-free, double frees,
         leaks attributable to libdl89, or leaked scans

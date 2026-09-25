# libdl89 testing scenarios (core)

All scenarios use the common symbolic fixture: constants A=101 B=102 C=103
D=104 E=105; relations PARENT=1 ANCESTOR=2 EDGE=3 PATH=4 SAME=5 LEFT=6
RIGHT=7 JOINED=8 P=9 Q=10 R=11 FLAG=12; variables X=1 Y=2 Z=3. Selected
tests repeat with 0 and ULONG_MAX identifiers.

## Construction and ownership

SCENARIO A-1 valid create and destroy
    GIVEN a store with all required callbacks
    WHEN dl89_eval_create is called and then dl89_eval_destroy
    THEN create returns DL89_OK with a non-NULL evaluator and destroy
         releases all evaluator-owned memory

SCENARIO A-2 invalid construction inputs
    GIVEN NULL config, NULL out, or a store missing any required callback
    WHEN dl89_eval_create is called
    THEN it returns DL89_EINVAL and sets *out to NULL

SCENARIO O-1 deep copy of rules
    GIVEN a stack rule p(X) :- q(X) and seed fact q(A)
    WHEN the rule is added and every caller byte is overwritten
    THEN running derives exactly p(A), proving add_rule copied the input

SCENARIO O-2 independent rule copies
    GIVEN one caller buffer reused for p(X) :- q(X) then r(X) :- p(X)
    WHEN both rules are added after buffer mutation
    THEN the first compiled rule is unaffected and p(A), r(A) derive

SCENARIO O-3 destroy releases copied program
    GIVEN many added rules
    WHEN the evaluator is destroyed
    THEN no evaluator-owned allocation remains (ASan/LSan/Valgrind)

## Rule validation

SCENARIO V-1 invalid term kinds
    GIVEN a head or any body term with an unknown kind
    WHEN the rule is added
    THEN DL89_EPROGRAM is returned and no rule is installed

SCENARIO V-2 unsafe head variables
    GIVEN p(X) :- q(Y), or p(X,Y) :- q(X)
    WHEN the rule is added
    THEN DL89_EPROGRAM is returned

SCENARIO V-3 variable-free rule
    GIVEN p(A) :- q(B)
    WHEN q(B) exists and the program runs
    THEN the add succeeds and p(A) is derived exactly once

SCENARIO V-4 ground fact and nonground fact
    GIVEN zero-body p(A) or zero-body p(X)
    WHEN each is added
    THEN p(A) is accepted and p(X) returns DL89_EPROGRAM

SCENARIO V-5 repeated variable
    GIVEN same(X) :- edge(X,X)
    WHEN added
    THEN the add succeeds; equality semantics are tested separately

SCENARIO V-6 arity conflicts
    GIVEN a rule containing p/1 and p/2, or a program with p/1 then p/2, or
          add_fact p/2 after p/1 (and the reverse)
    WHEN the conflicting input is added
    THEN DL89_EPROGRAM is returned and previously accepted state is unchanged

SCENARIO V-7 zero and maximum identifiers
    GIVEN rules, facts, and variables using 0 and ULONG_MAX
    WHEN added and run
    THEN DL89_OK and ordinary semantics, with no sentinel behavior

## Facts

SCENARIO F-1 single and duplicate facts
    GIVEN add_fact P(A) once and twice
    WHEN both calls run
    THEN both succeed and the store holds exactly one tuple

SCENARIO F-2 multi-column and persistent facts
    GIVEN EDGE(A,B) and an empty program
    WHEN run is called
    THEN the tuple is exact and unchanged

SCENARIO F-3 fact added between runs
    GIVEN p(X) :- q(X), run with empty q, then add q(A), then run again
    WHEN the second run completes
    THEN p contains exactly {(A)}

## Nonrecursive evaluation

SCENARIO E-1 unary copy
    GIVEN p(X) :- q(X) and q(A), q(B)
    WHEN run completes
    THEN p = {(A),(B)} and nothing else

SCENARIO E-2 constant filtering
    GIVEN p(X) :- edge(X,B) and edge(A,B), edge(A,C), edge(D,B)
    WHEN run completes
    THEN p = {(A),(D)}

SCENARIO E-3 constant in head
    GIVEN p(A) :- q(X) and q(B), q(C)
    WHEN run completes
    THEN p = {(A)} exactly once

SCENARIO E-4 repeated-variable equality
    GIVEN same(X) :- edge(X,X) and edge(A,A), edge(A,B), edge(B,B)
    WHEN run completes
    THEN same = {(A),(B)} and never a value from edge(A,B)

SCENARIO E-5 two-way join and failed join
    GIVEN joined(X,Z) :- left(X,Y), right(Y,Z)
    WHEN matching and non-matching inputs run
    THEN the exact join or the empty relation results

SCENARIO E-6 cross product
    GIVEN joined(X,Y) :- p(X), q(Y) with two tuples each
    WHEN run completes
    THEN all four ordered pairs exist and no others

SCENARIO E-7 shared variable across three atoms
    GIVEN r(X) :- p(X), q(X), edge(X,A)
    WHEN each predicate independently excludes one candidate
    THEN r equals the exact three-way intersection

SCENARIO E-8 repeated body relation
    GIVEN path(X,Z) :- edge(X,Y), edge(Y,Z)
    WHEN run completes
    THEN path is exactly the length-two edge composition

SCENARIO E-9 duplicate derivations
    GIVEN p(X) :- q(X) and p(X) :- r(X) with q(A), r(A)
    WHEN run completes
    THEN p = {(A)} with no observable duplicate

## Recursive fixed point

SCENARIO R-1 transitive closure
    GIVEN path(X,Y) :- edge(X,Y) and path(X,Z) :- path(X,Y), edge(Y,Z)
    WHEN the graph is a chain, cyclic, self-looping, branching, or dense
    THEN the exact mathematical closure results and evaluation terminates

SCENARIO R-2 recursion requires multiple rounds
    GIVEN a chain A->B->C->D->E
    WHEN run completes
    THEN path(A,E) is present

SCENARIO R-3 mutual recursion
    GIVEN p(X) :- q(X), q(X) :- r(X), r(X) :- p(X) with q(A)
    WHEN run completes
    THEN p, q, r all contain (A) and evaluation terminates

SCENARIO R-4 recursion with equality constraint
    GIVEN recursive path plus cycle(X) :- path(X,X)
    WHEN run completes
    THEN cycle contains exactly the nodes on directed cycles

SCENARIO R-5 order independence
    GIVEN a fixed logical program
    WHEN rules or initial facts are inserted in any permutation
    THEN the final databases are identical

## Fixed point, reruns, nullary

SCENARIO P-1 idempotent second run
    GIVEN a program at closure
    WHEN run is called again
    THEN DL89_OK and the database is exactly unchanged

SCENARIO P-2 new fact or rule after closure
    GIVEN a program at closure, then a new extensional fact or a new rule
    WHEN run is called again
    THEN the result equals full recomputation under all current input

SCENARIO P-3 monotonicity
    GIVEN successive successful runs without external mutation
    WHEN comparing databases
    THEN DB_before is a subset of DB_after

SCENARIO N-1 nullary predicates
    GIVEN ready() facts, bodies, derived nullary predicates, and nullary
          mutual recursion
    WHEN run completes
    THEN exactly one empty tuple behaves as documented and evaluation
         terminates

## Bound scans and exact oracle

SCENARIO B-1 binding patterns
    GIVEN rules forcing every bound pattern for arities one through three
    WHEN evaluation issues scans
    THEN every issued scan carries logically valid bindings and the final
         semantics match the reference evaluator (no particular join order or
         binding pattern is required)

SCENARIO X-1 exact database comparison
    GIVEN any semantic test
    WHEN comparing expected and actual relations
    THEN both directions are checked: missing and extra tuples both fail

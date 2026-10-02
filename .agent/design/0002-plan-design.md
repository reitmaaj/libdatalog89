# libdatalog89 design — compiled plan pipeline

## 0002 — immutable execution plan: SCCs, variants, join order

### Motivation

The current evaluator compiles individual rules (variable slotting in
`src/datalog89_rule.c`) but discovers evaluation structure at run time:
`src/datalog89_fixpoint.c` re-scans every rule and every body position on
every round to find IDB atoms, and `src/datalog89_join.c` always evaluates
body atoms left to right. Two defects follow:

- **Delta atom not first.** For p(X,Z) :- q(X,Y), p(Y,Z), the variant that
  iterates the delta of p still scans the full q relation for every delta
  tuple, because the join is body-ordered. Compile-time variant expansion
  with the DELTA step first removes this quadratic rescan.
- **Per-round IDB discovery.** Every round performs O(rules × body) linear
  `relation_is_idb` probes. Variants are static program structure and should
  be expanded once.

There is also no SCC decomposition: one global fixed-point loop drives the
whole program, and no rule can be proven "run exactly once".

### Target pipeline

```text
rules (compiled per rule, as today)
      │
      ▼
predicate dependency graph
      │
      ▼
SCC decomposition + topological ordering
      │
      ▼
semi-naive variant expansion
      │
      ▼
join ordering (static, deterministic)
      │
      ▼
static binding patterns per scan
      │
      ▼
immutable linear plan (contiguous arrays)
      │
      ▼
frame-based generic evaluator
```

The compiler answers every structural question — which SCC, which variant,
which atom leads, FULL/OLD/DELTA, which slots are bound where. The evaluator
answers only operational questions: next tuple, insert, did this SCC grow.
Nothing in the pipeline generates C, JITs, or synthesizes per-rule functions.

### Module boundaries

- `src/datalog89_plan.c` (new) — dependency graph, SCCs, variant expansion,
  join ordering, plan build and release, plan cache management.
- `src/datalog89_join.c` — becomes the plan interpreter: frame-based nested
  loop over a variant's steps, binding, unification, head emission.
- `src/datalog89_fixpoint.c` — per-SCC scheduling: nonrecursive SCCs execute
  once; recursive SCCs seed then iterate to a local fixed point.
- `src/datalog89_rule.c` — unchanged per-rule normalization and validation.
- `src/datalog89_test.c` + `src/datalog89_test.h` (new) — test-only
  introspection hooks. Compiled only into test programs, never into
  `build/libdatalog89.a`.

### Plan IR (C89, internal only)

```c
enum datalog89_priv_src { SRC_FULL, SRC_OLD, SRC_DELTA };

struct datalog89_priv_bind { size_t col; size_t slot; unsigned const_flag; };

struct datalog89_priv_step {
    datalog89_rel relation; size_t arity;
    enum datalog89_priv_src source;
    struct datalog89_priv_bind *bindings;   /* static per-column pattern */
};

struct datalog89_priv_variant {
    datalog89_rel head; size_t head_arity;
    size_t first_step, nsteps;
    size_t scc;                             /* index into plan->sccs */
    unsigned recursive;
};

struct datalog89_priv_scc {
    size_t first_variant, nvariants;
    unsigned recursive;
};

struct datalog89_priv_plan {
    struct datalog89_priv_scc *sccs;  size_t nsccs;
    struct datalog89_priv_variant *variants; size_t nvariants;
    struct datalog89_priv_step *steps;       /* contiguous; variants slice it */
    /* relation -> SCC map for EDB/IDB classification */
    datalog89_rel *idb_rels; size_t nidb;
};
```

All steps live in one contiguous array; variants point into slices. The plan
is immutable once built and is owned by `struct datalog89_eval`.

### Binding patterns

Given the join order, whether a variable slot is already bound at each step
is statically decidable. Each step therefore carries a static per-column
pattern: constant value, or bound slot to read, or free slot to bind. The
evaluator fills values without per-tuple kind/bound branching. This is the
abstract-store counterpart of index selection: the store still receives the
bound pattern in `scan_open` and chooses its own index. The store callback
interface is unchanged.

### Semi-naive convention

For a variant whose DELTA step is body position i:

- step i iterates the previous round's delta of its relation (source DELTA);
- other body atoms over relations recursive in the same SCC scan the store
  at the start of the round (source OLD);
- EDB and earlier-SCC atoms scan the store (source FULL).

A rule with k recursive body positions yields exactly k variants. Rules
without recursive body atoms yield one variant. Delta at position i is the
variant's first step (scenario PL-8).

### SCC scheduling

Tarjan's algorithm over the predicate dependency graph (head → body edges),
computed on the relation level, yields SCCs in topological order. Each SCC
owns the variants of every rule whose head is in it. Nonrecursive SCCs: all
body relations are EDB or in earlier SCCs, so every variant executes exactly
once, in any order within the SCC. Recursive SCCs: evaluate every variant
once against the full store (seed), then iterate variants over the local
delta until the SCC derives nothing new. Cross-SCC deltas are never carried
into another SCC's rounds.

### Plan cache

The plan is built lazily on the first `run` after program mutation and cached
in `struct datalog89_eval`. `add_rule` invalidates (frees) the cache;
`add_fact` does not (the plan is structural). `destroy` releases it. Plan
build failures free the partial plan, return `DATALOG89_ENOMEM`, and leave the
evaluator reusable.

### Join-order heuristic

Within a variant: DELTA step first, then remaining steps ordered by the
number of already-bound slots each step's pattern observes, with the number
of constant positions as tiebreak, then by body position as a final
deterministic tiebreak. No cost model, no statistics.

### Frame-based evaluator

The join evaluator uses an explicit frame array (one frame per step level)
with a cursor per open scan, instead of C recursion. Depth is bounded by the
plan, not by the C stack; a 32-atom body evaluates identically to a 3-atom
one (scenario PL-18).

### Test hooks

`src/datalog89_test.h` declares only `datalog89_test_*` functions: SCC count
and per-SCC classification, per-variant step order and source kinds, and
plan build count. They are implemented in `src/datalog89_test.c`, linked only
by test targets in the Justfile. The api-convention symbol audit (scenario
PL-19) proves the archive stays clean.

### Locked decisions

Phased delivery with main always green; most-bound-first join ordering with
constant-count tiebreak; plan cached and invalidated by `add_rule`; test-only
`datalog89_test_*` hooks for white-box plan tests.

### Non-goals

Cost-based optimization, statistics, relational-algebra ASTs, arithmetic in
the plan, generated C, JIT, dynamic replanning, negation/stratification,
aggregation, new public API, changes to the store callback interface.

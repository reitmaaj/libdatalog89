# libdl89 concept

## Purpose

`libdl89` implements a small embeddable Datalog evaluation core. It accepts
facts and rules and computes the least fixed point over a caller-supplied
relational store.

    eval : Program x Database -> least_fixed_point(Program, Database)

for finite monotone Datalog programs. The library knows nothing about
application syntax, reflection, SQLite, source files, symbol names, modules,
REPLs, or application policy.

## Semantic model

    Term   ::= Const(c) | Var(v)
    Atom   ::= Rel(r, Term*)
    Rule   ::= Atom | Atom :- Atom+
    DB     ::= set of ground atoms

A zero-body rule represents a fact. The evaluator derives ground facts until
no new fact can be inserted. No ordering among facts carries semantic meaning.

## Identifier model

    typedef unsigned long dl89_const;
    typedef unsigned long dl89_rel;
    typedef unsigned long dl89_var;

The caller assigns all meaning. `libdl89` never receives strings. Variables
only need uniqueness within one rule. Every `unsigned long` value, including
`0` and `ULONG_MAX`, is an ordinary identifier.

## Boundary

`libdl89` owns exactly: rule validation, variable binding, unification with
ground tuples, body joins, head instantiation, recursive evaluation,
duplicate suppression through store insertion, least-fixed-point scheduling,
and delta evaluation if implemented.

`libdl89` explicitly does not own: text syntax, parser, lexer, AST, pretty
printing, constant/relation names, symbol interning, source diagnostics,
SQLite schemas, transactions, persistence, modules, imports, namespaces,
reflective representation, clause/atom IDs, list encodings, rule discovery,
program loading, dynamic rule installation semantics, REPL/CLI/server/TUI,
authorization, provenance, explanations, negation, aggregation, retraction,
updates, truth maintenance, foreign side effects, and I/O predicates.

## Layering

    application UI / language
              |
    application program model (reflective encoding, snapshots)
              |
           dl89_rule[]
              |
    libdl89 (bindings, joins, fixed point)
              |
            dl89_store
              |
    application relation store (symbols, tuples, SQLite, indexes)

The architectural rule: `libdl89` evaluates a fixed Datalog program against
an abstract relational database; the caller decides how that program and
database are represented, persisted, named, reflected, mutated, and
presented.

## Reflective modification

Self-extension is ordinary iteration of `lfp(Pn, DBn)` by the caller: decode
derived rule data, add rules, run another phase. The evaluator's rule set
never mutates midway through a fixed-point computation.

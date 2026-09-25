# libdl89 stakeholders

## STORY-1 embedder derives closure facts

AS an embedder with my own symbol table and relational store
I WANT to load a finite Datalog program and run it to its least fixed point
SO THAT derived facts appear in my store without my implementing joins,
unification, or fixed-point iteration.

## STORY-2 store adapter author keeps storage authority

AS a store adapter author (SQLite, hash sets, B-trees, LMDB, virtual
relations)
I WANT the library to use only insert and bound-pattern scans
SO THAT I keep control of schemas, indexes, transactions, persistence, and
performance while the library stays storage-neutral.

## STORY-3 application author reflects programs as data

AS an application author whose programs are relational data
I WANT rules to be copied at load time and the rule set frozen during a run
SO THAT evaluation never observes my representation changing underneath it
and I can decode derived facts into new program snapshots between runs.

## STORY-4 tester proves exact closure

AS a tester
I WANT an independent reference evaluator and a fault-injecting store
SO THAT I can prove the library computes exactly the least fixed point and
propagates every store or allocation failure without leaking scans or state.

## STORY-5 future language runtime embeds a small core

AS a future self-modifying language runtime
I WANT a tiny, auditable, dependency-free C89 evaluator core
SO THAT I can embed it in a larger system without coupling it to my language,
storage engine, or transaction policy.

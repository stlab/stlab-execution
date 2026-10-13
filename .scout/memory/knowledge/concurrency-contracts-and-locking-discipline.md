---
title: "Concurrency Contracts and Locking Discipline"
entity_type: pattern
confidence: 0.80
created: 2026-10-13T00:39:55Z
last_accessed: 2026-10-13T00:39:55Z
last_reinforced: 2026-10-13T00:39:55Z
access_count: 1
sources:
  - "CLAUDE.md:1-50"
  - "src/concurrency/detail/main_task_queue.hpp:25-55"
  - "src/concurrency/core_shutdown.cpp:30-80"
relationships:
  - type: relates_to
    target: "Architecture Overview"
  - type: relates_to
    target: "Task Type Erasure and Relocation ABI"
tags: ["concurrency-contracts", "locking-rules", "synchronization-invariants", "mutex-discipline", "lock-scoping"]
tier: knowledge
source: inferred
---

# Concurrency Contracts and Locking Discipline

Strict concurrency and lock-scoping guidelines govern all internal synchronization within `stlab-execution`:

Key rules and invariants:
1. Narrowest lock scope: Mutexes must protect only the minimal shared state invariant. Callbacks, capture destruction, task bodies, user code, and blocking waits must NEVER be executed inside a mutex critical section.
2. Mandatory mutex documentation: Every mutex declaration in source code must include a comment detailing the exact state and synchronization invariant it protects, including any associated condition variable coordination.
3. Queue push/pop safety: Callable construction, copy/move construction, and moved-from object destruction are prohibited from submitting work to queues, because queue implementations may hold locks during element relocation or destruction.
4. Mutex vs Atomic: Protect a single primitive with `std::atomic` only if compound invariants, wait queues, or object lifetime guarantees are not required.

Aliases / also known as: concurrency_contracts, locking_rules, synchronization_invariants, mutex_discipline, lock_scoping

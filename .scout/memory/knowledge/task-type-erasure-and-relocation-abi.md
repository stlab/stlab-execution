---
title: "Task Type Erasure and Relocation ABI"
entity_type: concept
confidence: 0.80
created: 2026-10-13T00:38:55Z
last_accessed: 2026-10-13T00:38:55Z
last_reinforced: 2026-10-13T00:38:55Z
access_count: 1
sources:
  - "include/stlab/concurrency/task.hpp:12-32"
  - "include/stlab/concurrency/task.hpp:63-80"
  - "include/stlab/concurrency/task.hpp:160-200"
relationships:
  - type: relates_to
    target: "Architecture Overview"
tags: ["task", "task-t", "stlab-v2-task-concept", "type-erasure", "move-only-task"]
tier: knowledge
source: inferred
---

# Task Type Erasure and Relocation ABI

`stlab::task<Signature>` provides a move-only type-erased callable wrapper (similar to `std::function`, but non-copyable). It is explicitly designed for move-only and single-shot targets in executor queues.

Key invariants and contracts:
1. Non-throwing relocation across ABI: Small callables with non-throwing move constructors are stored in a small-buffer optimization (SBO) buffer (`stlab_v2_task_storage_size`), while larger or throwing callables are heap-allocated. Relocation across the C ABI is guaranteed non-throwing via `stlab_v2_task_concept` operation tables.
2. Queued-target lifecycle requirement: Callable construction, copy/move construction, and destruction of moved-from callables must NOT submit work to executors or timers, because queues may perform relocation/destruction while holding internal locks.
3. Invocation & destruction: Task invocation and destruction of executed targets ARE permitted to submit work while admission is open.
4. Empty tasks: An empty task compares equal to `nullptr`. Invoking an empty task throws `std::bad_function_call` for throwing signatures, or calls `std::terminate()` for `noexcept` signatures.

Aliases / also known as: task, task_t, stlab_v2_task_concept, type_erasure, move_only_task

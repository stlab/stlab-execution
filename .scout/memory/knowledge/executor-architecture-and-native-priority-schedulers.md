---
title: "Executor Architecture and Native Priority Schedulers"
entity_type: concept
confidence: 0.75
created: 2026-10-13T00:39:05Z
last_accessed: 2026-10-13T00:39:05Z
last_reinforced: 2026-10-13T00:39:05Z
access_count: 1
sources:
  - "include/stlab/concurrency/default_executor.hpp:50-115"
  - "src/concurrency/executor_abi.cpp:630-760"
relationships:
  - type: depends_on
    target: "Task Type Erasure and Relocation ABI"
  - type: relates_to
    target: "Process Lifecycle and Pre-Exit Protocol"
tags: ["default-executor", "executor-abi", "priority-task-system", "thread-pool", "stlab-v2-default-executor-submit"]
tier: knowledge
source: inferred
---

# Executor Architecture and Native Priority Schedulers

Default thread-pool executors submit tasks through an ABI-stable C interface (`stlab_v2_default_executor_submit`, `stlab_v2_high_executor_submit`, `stlab_v2_low_executor_submit`) defined in `include/stlab/concurrency/default_executor.hpp`.

Key mechanics:
1. Entry points take a `task_abi_guard` pointer (`current_task_storage_abi_guard::value`), a vtable pointer (`stlab_v2_task_concept`), an invocation procedure (`stlab_v2_task_proc`), and a relocation source pointer.
2. Backends: In `src/concurrency/executor_abi.cpp`, native priority scheduling is sharded. Backends are selected at build time:
   - macOS/Apple: Apple libdispatch (GCD) global concurrent queues (`DISPATCH_QUEUE_PRIORITY_*`).
   - Windows: Windows thread pool APIs (`TrySubmitThreadpoolCallback`).
   - Portable: Native portable worker threads (`priority_task_system`).
   - Threadless Emscripten: `cooperative_executor.cpp` using the browser/Node event loop.
3. Blocking-wait compensation: Portable executor provides `stlab_v2_notify_default_executor_before_waiting()` (`stlab_v2_default_executor_supports_blocking()`). Calling threads notify the scheduler before blocking, allowing the thread pool to spawn a temporary worker thread to maintain concurrency and avoid deadlocks. On OS-managed pools (Windows/libdispatch), this is a no-op.

Aliases / also known as: default_executor, executor_abi, priority_task_system, thread_pool, stlab_v2_default_executor_submit

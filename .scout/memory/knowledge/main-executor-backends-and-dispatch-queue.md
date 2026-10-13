---
title: "Main Executor Backends and Dispatch Queue"
entity_type: concept
confidence: 0.75
created: 2026-10-13T00:39:25Z
last_accessed: 2026-10-13T00:39:25Z
last_reinforced: 2026-10-13T00:39:25Z
access_count: 1
sources:
  - "cmake/ExecutionPlatform.cmake:52-67"
  - "src/concurrency/detail/main_task_queue.hpp:20-55"
  - "src/concurrency/main_executor_none.cpp:1-25"
relationships:
  - type: relates_to
    target: "Architecture Overview"
  - type: relates_to
    target: "Process Lifecycle and Pre-Exit Protocol"
tags: ["main-executor", "main-task-queue", "main-queue", "portable-main-executor"]
tier: knowledge
source: inferred
---

# Main Executor Backends and Dispatch Queue

Main-queue execution and task systems are orthogonal platform selections in CMake (`cmake/ExecutionPlatform.cmake` and `cmake/ExecutionConfig.cmake`).

Key mechanics:
1. Selection independent of task system: `STLAB_MAIN_EXECUTOR` can be configured as `qt5`, `qt6`, `libdispatch`, `emscripten`, `portable`, or `none`. On Windows and desktop Linux without Qt or GCD, it defaults to `none`.
2. `main_task_queue`: When `portable` or Qt/libdispatch main execution is active, `main_task_queue` (`src/concurrency/detail/main_task_queue.hpp`) manages FIFO task queuing using a mutex and condition variable.
3. Teardown independence: Main executor admission remains open even during `shutdown_core()`. Core teardown drains default/high/low executors and cancels timers, but does not close main-queue admission, allowing main tasks to coordinate shutdown or UI teardown.
4. Main thread fence: Producers joined during shutdown must not synchronously depend on main-queue progress, or deadlocks will occur if `pre_exit()` was called from the main thread.

Aliases / also known as: main_executor, main_task_queue, main_queue, portable_main_executor

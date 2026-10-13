---
title: "Threadless Emscripten Cooperative Execution and Shutdown"
entity_type: concept
confidence: 0.75
created: 2026-10-13T00:39:36Z
last_accessed: 2026-10-13T00:39:36Z
last_reinforced: 2026-10-13T00:39:36Z
access_count: 1
sources:
  - "src/pre_exit.cpp:25-58"
  - "src/concurrency/detail/cooperative_executor.hpp:20-60"
  - "src/concurrency/cooperative_executor.cpp:1-50"
relationships:
  - type: relates_to
    target: "Process Lifecycle and Pre-Exit Protocol"
  - type: relates_to
    target: "Executor Architecture and Native Priority Schedulers"
tags: ["emscripten", "cooperative-executor", "threadless-wasm", "webassembly-execution", "wasm-pre-exit"]
tier: knowledge
source: inferred
---

# Threadless Emscripten Cooperative Execution and Shutdown

Threadless Emscripten (WebAssembly without pthreads) operates in a single-threaded environment with run-to-completion semantics, requiring specialized execution handling:

Key architectural distinctions:
1. `cooperative_executor`: Implemented in `src/concurrency/cooperative_executor.cpp` and `detail/cooperative_executor.hpp`. Instead of background threads, tasks yield control back to the host event loop via microtasks or `setTimeout`.
2. Blocking prohibition: Blocking operations (e.g. `wait()`, `std::condition_variable::wait()`, sleeping) are strictly impossible and unsupported.
3. Pre-exit behavior: `pre_exit()` cannot block to join threads. Instead, it initiates asynchronous retirement (`phase::deferred` in `src/pre_exit.cpp`). Remaining pre-exit handlers and ordinary main tasks are deferred until queued cooperative executor tasks and descendant tasks drain completely.
4. Completion fence: An ordinary main task queued *after* `pre_exit()` acts as the final completion fence; final process exit must wait for that fence to execute.

Aliases / also known as: emscripten, cooperative_executor, threadless_wasm, webassembly_execution, wasm_pre_exit

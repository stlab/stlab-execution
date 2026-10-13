---
title: "Process Lifecycle and Pre-Exit Protocol"
entity_type: decision
confidence: 0.80
created: 2026-10-13T00:38:46Z
last_accessed: 2026-10-13T00:40:29Z
last_reinforced: 2026-10-13T00:38:46Z
access_count: 2
sources:
  - "include/stlab/pre_exit.hpp:11-32"
  - "include/stlab/pre_exit.hpp:59-74"
  - "src/pre_exit.cpp:38-90"
  - "src/concurrency/core_shutdown.cpp:48-102"
relationships:
  - type: relates_to
    target: "Architecture Overview"
tags: ["pre-exit", "at-pre-exit", "process-shutdown", "stlab-pre-exit", "core-shutdown"]
tier: knowledge
source: inferred
---

# Process Lifecycle and Pre-Exit Protocol

`stlab::pre_exit()` manages process lifecycle by running registered handlers in reverse registration order (LIFO). Registration via `stlab::at_pre_exit()` accepts `void (*)() noexcept` functions.

Crucial synchronization and ordering invariants:
1. `pre_exit()` must be invoked exactly once before normal termination (prior to `exit()` or leaving `main()`).
2. Core teardown handler (`shutdown_core()` in `src/concurrency/core_shutdown.cpp`) is lazily registered upon first use of default executors or system timers.
3. Applications that need running worker tasks or timer callbacks to finish must register unblocking handlers *after* first core use, so LIFO execution runs application teardown before core teardown.
4. Core teardown cancels pending timers, waits for committed timer callbacks, then drains initialized default executors.
5. In threaded builds, default/high/low executor tasks and timer callbacks (including executed-target destructors) are strictly prohibited from calling `pre_exit()`. Violations terminate immediately via `check_pre_exit_context()`.
6. On threadless Emscripten, `pre_exit()` does not block; it initiates asynchronous cooperative retirement. A main task submitted after `pre_exit()` serves as the completion fence.

Aliases / also known as: pre_exit, at_pre_exit, process_shutdown, stlab_pre_exit, core_shutdown

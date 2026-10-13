---
title: "Architecture Overview"
entity_type: concept
confidence: 0.80
created: 2026-10-13T00:38:31Z
last_accessed: 2026-10-13T00:38:31Z
last_reinforced: 2026-10-13T00:38:31Z
access_count: 1
sources:
  - "README.md:1-40"
  - "include/stlab/concurrency/task.hpp:12-32"
  - "include/stlab/pre_exit.hpp:11-32"
tags: ["architecture-overview", "stlab-execution", "execution-runtime", "concurrency-runtime", "stlab-concurrency"]
tier: knowledge
source: inferred
---

# Architecture Overview

stlab-execution is the standalone C++17/20 execution runtime extracted from STLab, providing move-only tasks, priority executors, asynchronous timers, and process lifecycle coordination (`pre_exit()`). It preserves canonical stlab namespace and include paths (`<stlab/concurrency/*.hpp>`, `<stlab/pre_exit.hpp>`), but deliberately excludes futures, channels, serial queues, and coroutine policies (which reside in higher-level libraries). Public C++ interfaces are header-only inline templates, while compiled sources implement process-wide schedulers, thread pools, and lifecycle state.

First-to-read headers and entry points:
- `include/stlab/concurrency/task.hpp`: non-copyable type-erased callable wrapper with SBO and noexcept relocation across the C ABI.
- `include/stlab/concurrency/default_executor.hpp`: default, high, and low priority thread-pool executors.
- `include/stlab/concurrency/system_timer.hpp`: non-early asynchronous timer service.
- `include/stlab/pre_exit.hpp`: process-wide LIFO pre-exit handler registry and drain barrier.
- `src/concurrency/executor_abi.cpp`: ABI implementation for native priority queues, threadpool backends, and portable blocking compensation.

Aliases / also known as: architecture_overview, stlab_execution, execution_runtime, concurrency_runtime, stlab_concurrency

---
title: "ABI Stability, Version Namespaces, and Shared Library Exports"
entity_type: decision
confidence: 0.80
created: 2026-10-13T00:39:46Z
last_accessed: 2026-10-13T00:39:46Z
last_reinforced: 2026-10-13T00:39:46Z
access_count: 1
sources:
  - "include/stlab/concurrency/task.hpp:60-80"
  - "include/stlab/concurrency/default_executor.hpp:50-80"
  - "src/execution.def:1-30"
  - "CMakeLists.txt:1-60"
relationships:
  - type: relates_to
    target: "Architecture Overview"
  - type: relates_to
    target: "Task Type Erasure and Relocation ABI"
tags: ["abi-stability", "version-namespaces", "execution-def", "task-abi-guard", "shared-library-abi"]
tier: knowledge
source: inferred
---

# ABI Stability, Version Namespaces, and Shared Library Exports

`stlab-execution` employs versioned symbols and ABI namespaces to prevent symbol collisions and preserve binary compatibility across component updates:

Key rules and constraints:
1. Version namespaces: C++ interfaces live inside `STLAB_EXECUTION_VERSION_NAMESPACE_BEGIN()` and `END()`, which expand to versioned inline namespaces (`inline namespace v2`). Internal implementation helpers reside in `execution_detail`.
2. Linker ABI Guard: Task submission across the C boundary requires passing `task_abi_guard` pointing to `execution_detail::current_task_storage_abi_guard::value`. Any mismatch in SBO buffer layout, alignment, or calling convention causes immediate assertion failure and termination.
3. C Export Vector: Core functions are exposed as C entry points (`stlab_pre_exit`, `stlab_at_pre_exit`, `stlab_v2_default_executor_submit`, etc.) and explicitly tracked in `src/execution.def` on Windows DLL builds. ABI changes require defining new versioned entry points, not weakening layout assertions.
4. Header generation: Platform capabilities are generated into `stlab/execution/config.hpp` via `config.hpp.in` and CMake. Never edit `config.hpp` directly in the build tree.

Aliases / also known as: abi_stability, version_namespaces, execution_def, task_abi_guard, shared_library_abi

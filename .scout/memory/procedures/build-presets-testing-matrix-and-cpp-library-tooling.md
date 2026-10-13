---
title: "Build Presets, Testing Matrix, and cpp-library Tooling"
entity_type: procedure
confidence: 0.80
created: 2026-10-13T00:40:05Z
last_accessed: 2026-10-13T00:40:05Z
last_reinforced: 2026-10-13T00:40:05Z
access_count: 1
sources:
  - "CMakePresets.json:1-100"
  - "README.md:30-85"
  - "CLAUDE.md:20-60"
relationships:
  - type: relates_to
    target: "Architecture Overview"
tags: ["build-system", "cmake-presets", "cpp-library", "test-infrastructure", "ctest-matrix"]
tier: procedures
source: inferred
---

# Build Presets, Testing Matrix, and cpp-library Tooling

`stlab-execution` uses standard CMake presets and pins `cpp-library` for build orchestration, packaging, and CI workflows:

Key infrastructure rules:
1. Pinned dependency `cpp-library`: Uses `cpp-library` pinned via CPM (e.g. v5.6.0 on branch `prepare-v1-0-0-release`). Files marked auto-generated (CI workflows, `.clang-format`, etc.) must not be edited directly; they are updated via `cmake --preset=init && cmake --build --preset=init`.
2. Standard presets:
   - `default`: Library-only release build.
   - `test`: Complete debug build and test suite (`ctest --preset=test`).
   - `clang-tidy`: Static analysis with project `.clang-tidy`.
   - `docs`: API reference generation via Doxygen.
3. Test layout: Core tests use doctest (`execution.test.task`, `execution.test.executor`, `execution.test.system_timer`, etc.). Lifecycle and failure scenarios run as isolated CTest processes with explicit timeouts to test termination behavior safely.
4. Language standards: Defaults to C++20 for compilation, but all public interfaces must strictly retain compatibility with C++17 consumers.

Aliases / also known as: build_system, cmake_presets, cpp_library, test_infrastructure, ctest_matrix

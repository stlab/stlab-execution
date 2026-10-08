# Repository scope

`stlab-execution` is the standalone C++17/20 execution runtime extracted from
STLab: move-only tasks, executors, timers, and process lifecycle. It preserves
`stlab::` API names and canonical `<stlab/concurrency/*.hpp>` and
`<stlab/pre_exit.hpp>` includes. Execution does not depend on STLab; futures,
channels, serial queues, and coroutine policy belong to the higher-level library.
Header-adjacent contracts are canonical; consult `README.md` for configuration
and consumption details and `CLAUDE.md` for concurrency instructions.

## Build, test, and analysis

Run from the repository root with CMake >= 3.24, Ninja, and a configured compiler
environment. On Windows, use the same x64 Visual Studio developer environment
for configure, build, and test. The build defaults to C++20; public interfaces
must remain usable by C++17 consumers.

```powershell
# Library-only Release build
cmake --preset=default
cmake --build --preset=default

# Debug build and complete test suite
cmake --preset=test
cmake --build --preset=test
ctest --preset=test

# Discover tests and run one CTest entry
ctest --preset=test -N
ctest --preset=test -R "^execution\.test\.task$"

# Build one doctest executable and run one case by its exact name
cmake --build --preset=test --target execution.test.task
.\build\test\test\execution.test.task.exe --test-case="empty task throws bad_function_call"

# Static analysis using the repository's .clang-tidy
cmake --preset=clang-tidy
cmake --build --preset=clang-tidy
ctest --preset=clang-tidy

# Format changed C++ files using .clang-format
clang-format -i <changed-file>

# API reference (requires Doxygen)
cmake --preset=docs
cmake --build --preset=docs
```

On non-Windows hosts, the doctest binary is `build/test/test/execution.test.task`
without `.exe`. Other doctest targets are `execution.test.executor`,
`execution.test.executor_abi`, `execution.test.waiter_state`, and
`execution.test.system_timer`. Lifecycle and failure scenarios run as separate
CTest processes, often with command-line scenario arguments and explicit timeouts.
Use `ctest -N` and `test/CMakeLists.txt` to select the relevant entry.

Variants are separate build directories, not extra presets:

```powershell
cmake --preset=test -B build\test-cpp17 -DCMAKE_CXX_STANDARD=17
cmake --build build\test-cpp17
ctest --test-dir build\test-cpp17 --output-on-failure
```

Use the same configure/build/test pattern for `-DBUILD_SHARED_LIBS=ON`,
`-DSTLAB_TASK_SYSTEM=portable`, `-DSTLAB_MAIN_EXECUTOR=portable`, or
`-DSTLAB_SANITIZER=address`. `-DSTLAB_EXECUTION_PACKAGE_TESTS=ON` adds the
`execution.package.roundtrip` installed-consumer check for native builds.
All test configurations also compile public headers independently.

Emscripten requires an activated SDK, Node >= 18.3.0, the toolchain
`cmake\Platform\Emscripten-Execution.cmake`, and an explicit
`-DSTLAB_EMSCRIPTEN_PTHREADS=ON` or `OFF`; see the README commands.
Threadless tests are standalone Node scenarios rather than blocking doctest cases.
Preserve `NODE_JS_FLAGS` as a CMake list in nested configuration tests.

## Architecture

- Public headers provide callable wrappers and adapters; compiled sources own
  process-wide scheduler and lifecycle state. Link `stlab::execution` for source
  consumers and after `find_package(stlab-execution CONFIG REQUIRED)`.
- `task.hpp` implements move-only type erasure with small-buffer storage and
  nonthrowing relocation. Executor adapters submit `task<void() noexcept>` through
  versioned C entry points using an operation table, invocation pointer, storage
  source, and linker ABI guard. `src/concurrency/executor_abi.cpp` owns native
  priority scheduling: sharded queues backed by libdispatch, Windows thread
  pools, or portable workers with blocking-wait compensation. Threadless
  Emscripten uses `cooperative_executor.cpp` and the host event loop instead.
- Main execution and timers use separate backend source files selected by
  `CMakeLists.txt`. Task-system and main-executor selection are independent;
  `cmake/ExecutionPlatform.cmake` discovers defaults and
  `cmake/ExecutionConfig.cmake` validates combinations and generates
  `stlab/execution/config.hpp`. Native main execution uses libdispatch, Qt, or
  an opt-in portable queue; it can be unavailable (`none`), including on Windows.
- `system_timer.hpp` normalizes durations and translates resource statuses from
  the nonthrowing C ABI into C++ exceptions. Backend timer services share
  validation/delay helpers in `src/concurrency/detail/timer_common.hpp` and
  lifecycle coordination with `core_shutdown.cpp`.
- `pre_exit.cpp` owns the process-wide LIFO handler stack. First timer or
  default-executor use registers one core teardown handler. Native teardown
  cancels pending timers and destroys their captures, waits for committed timer
  callbacks, then drains initialized default executors. Main-queue admission
  remains open. Threadless Emscripten defers remaining handlers and ordinary main
  dispatch until cooperative executor work and its descendants finish.

## Compatibility and concurrency contracts

- Keep C++ wrappers in `STLAB_EXECUTION_VERSION_NAMESPACE_BEGIN()` / `END()` and
  implementation helpers in `execution_detail`. The stable scheduling ABI and
  pre-exit API use `stlab::v2`; preserve their versioned symbols. ABI-incompatible
  operation-table changes require a new versioned type, not weakened layout
  assertions. Coordinate shared-library exports in `src/execution.def` when
  changing exported entry points. Changes to extraction/version namespaces
  require rebuilding C++ consumers even when source compatibility is preserved.
- Use generated function-like selectors such as `STLAB_TASK_SYSTEM(PORTABLE)` and
  `STLAB_MAIN_EXECUTOR(NONE)`. Change `config.hpp.in` and its CMake generator,
  never the generated build-tree header. `BUILD_SHARED_LIBS` determines runtime
  linkage; `STLAB_EXECUTION_SHARED` is derived, not a user-selectable CMake option.
- Queued callable construction, copy/move construction, and moved-from
  destruction must not submit work: queue operations may perform them while
  locked. Task bodies and executed-target cleanup may submit work while admission
  is open. Preserve exactly-once ownership, invocation/cancellation, and capture
  destruction through relocation and shutdown.
- Give mutex locks the narrowest scope preserving the complete synchronization
  invariant. Keep unrelated work, callbacks, capture destruction, and blocking
  operations outside critical sections whenever safe. Comment each mutex
  declaration with the state/invariant it protects, including condition-variable
  and object-lifetime coordination. Consider an atomic for a single member only
  when compound operations, waits, and lifetime guarantees do not require a mutex.
- Call `pre_exit()` exactly once before normal process exit. On threaded systems,
  default/high/low tasks and timer callbacks, including executed-target cleanup,
  must not call it. Main tasks may initiate shutdown. Producers joined during
  shutdown must not depend synchronously on main-queue progress. Application
  handlers that unblock core work must register after first core use so LIFO
  ordering runs them before core teardown.
- Threadless Emscripten cannot make progress through blocking waits. `pre_exit()`
  initiates asynchronous retirement; a subsequently submitted ordinary main task
  is the completion fence, not return from `pre_exit()`.
- Timers are asynchronous even for nonpositive delays; positive delays round
  upward and must not execute early. Submission resource failure leaves the task
  unconsumed. Contract violations use assertions plus termination where required
  even in release builds; resource errors use the explicit timer status protocol.

## Build infrastructure ownership

cpp-library is pinned to an exact release commit and supplies build/install/docs
helpers and template generation. Files marked auto-generated, including the CI
workflow and `.clang-format`, must be changed through their owning cpp-library
templates/generator, not edited directly. Regenerate with `cmake --preset=init`
and `cmake --build --preset=init`. Keep dependency provenance in `[DEPENDENCY]`
comments; do not customize the external CPM bootstrap.

For local toolkit work, configure with
`-DCPM_cpp-library_SOURCE:PATH=<toolkit-checkout>`. Keep local paths out of
production dependency declarations. Execution is unreleased; the README's 1.0.0
fetch example is prospective, and `CPP_LIBRARY_VERSION=1.0.0` is only a standalone
install-validation override, not release evidence.

The generated CI runs generic native and clang-tidy presets.
`.github/matrix.json` is not consumed by that workflow; shared, C++17, portable,
ASan, Qt, Emscripten, and package variants need separate validation.

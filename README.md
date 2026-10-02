# stlab-execution

C++17/20 task execution, executors, timers, and process lifecycle, extracted from
STLab under the Boost Software License 1.0. Existing `stlab::` API names and
`stlab/concurrency/*.hpp` and `stlab/pre_exit.hpp` include paths are preserved.
No futures, channels, serial queues, or STLab utility library are required.

## Build and test

Use CMake 3.24 or newer and Ninja, with the compiler environment configured:

```powershell
cmake --preset=test
cmake --build --preset=test
ctest --preset=test

cmake --preset=test-cpp17
cmake --build --preset=test-cpp17
ctest --preset=test-cpp17
```

`test-shared` and `test-portable-shared` exercise the shared runtime with native
and portable task backends. The toolkit dependency is pinned to its exact commit;
for local development, pass `-DCPM_cpp-library_SOURCE:PATH=<toolkit-checkout>` during
configuration. With no release tag, the toolkit's development version is `0.0.0`.
`-DCPP_LIBRARY_VERSION=1.0.0` may be used for standalone install validation.

## Consume

Link `stlab::execution` from `add_subdirectory` or CPM. Installed consumers use
`find_package(stlab-execution CONFIG REQUIRED)` and the same target name.
`STLAB_EXECUTION_INSTALL` defaults to ON for standalone builds and OFF as a
subproject. The generated `stlab/execution/config.hpp` belongs to this library,
with execution-specific version macros and an `execution_v*` inline namespace.

`STLAB_EXECUTION_SHARED` selects a shared runtime. `STLAB_CORE_SHARED` is the
legacy input spelling and effective compatibility macro; explicitly conflicting
inputs are rejected. Without the new setting, legacy ON selects shared, as does
`BUILD_SHARED_LIBS=ON` on non-Windows platforms; otherwise the runtime is static.
The parent project's `BUILD_SHARED_LIBS` is not changed.

Backend options retain STLab's defaults and validation: `STLAB_THREAD_SYSTEM`,
`STLAB_TASK_SYSTEM`, `STLAB_MAIN_EXECUTOR`, `STLAB_TASK_POOL_MAXIMUM`,
`STLAB_NO_STD_COROUTINES`, and `STLAB_EMSCRIPTEN_PTHREADS`.
`STLAB_SANITIZER=address` retains runtime address-sanitizer instrumentation.

Call `stlab::pre_exit()` exactly once before normal process exit when using
runtime services. Scheduling, timer retirement, pre-exit ordering, and versioned
C ABI entry points retain their STLab contracts; see adjacent header documentation.

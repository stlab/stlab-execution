# stlab-execution

C++17/20 task execution, executors, timers, and process lifecycle, extracted from
STLab under the Boost Software License 1.0. Existing `stlab::` API names and
`stlab/concurrency/*.hpp` and `stlab/pre_exit.hpp` include paths are preserved.
No futures, channels, serial queues, or STLab utility library are required.

**Publication status:** this is an unreleased extraction under review. The execution
and toolkit development commits are available on public review branches, not approved
release pins. Hosted CI can now run; its results remain separate from local validation.
Release publication still follows the order below.

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

`cmake --preset=test-packages` followed by `ctest --preset=test-packages`
verifies standalone installed C++17/20 consumers and independently compiles each
applicable public header. `test-packages-shared` and
`test-packages-portable-shared` select the corresponding shared backends.
The verifier uses a fresh `install`-preset child with `BUILD_TESTING=OFF` and
the standalone validation version override above, never an STLab package.
Commands and package evidence remain under
`build/<preset>/package-test/execution-packages`; Windows fixtures deploy
`TARGET_RUNTIME_DLLS` before execution. Use the same compiler environment and
toolkit `:PATH` override as other native presets.

Matching configure/build/test presets also cover `test-portable`,
`test-portable-main`, and `test-asan`. On Windows, run all three CMake commands
from the same x64 Visual Studio developer environment. The ASan preset instruments
both the runtime and its contract/lifecycle executables; allocator interception
tests run only in non-ASan static Windows configurations.

With an activated Emscripten SDK and Node 16.16 or newer, use `test-emscripten`
for pthreads or `test-emscripten-threadless` for cooperative event-loop execution.
The threadless suite uses standalone Node scenarios rather than a blocking
doctest/std::future harness. Each process has a timeout. ABI rejection tests
require an unresolved task-storage guard diagnostic, not merely a failed link.
The deliberately incompatible target is excluded from the default build.

Nested configuration tests preserve the selected `NODE_JS_FLAGS` list and check
the child cache and actual emulator executable/flag order. For a focused regression
with two Node options (repeat with `test-emscripten` for pthreads):

```bash
cmake --preset=test-emscripten-threadless "-DNODE_JS_FLAGS:STRING=--no-warnings;--stack-trace-limit=20"
ctest --preset=test-emscripten-threadless -R "shared_reconfiguration|emscripten_configuration"
```

## Consume

Windows test and package consumers deploy runtime DLLs with an empty-list-safe
helper compatible with CMake 3.24. Static consumers require no DLL copy.

Link `stlab::execution` from `add_subdirectory` or CPM. Installed consumers use
`find_package(stlab-execution CONFIG REQUIRED)` and the same target name.

Current local source consumption (generic developer paths):

```cmake
set(CPM_cpp-library_SOURCE "/path/to/cpp-library" CACHE PATH "")
set(CPM_stlab-execution_SOURCE "/path/to/stlab-execution" CACHE PATH "")
CPMAddPackage(
  NAME stlab-execution
  SOURCE_DIR "${CPM_stlab-execution_SOURCE}")
target_link_libraries(app PRIVATE stlab::execution)
```

From a developer shell, equivalent typed overrides are
`-DCPM_cpp-library_SOURCE:PATH=<toolkit-checkout>` and
`-DCPM_stlab-execution_SOURCE:PATH=<execution-checkout>`.
Use them to validate local changes instead of the remote development commits; do not
substitute developer paths into production dependency declarations.

**Proposed future release example only — 1.0.0 is not published:**

```cmake
CPMAddPackage("gh:stlab/stlab-execution@1.0.0")
target_link_libraries(app PRIVATE stlab::execution)
```

Do not use that fetch until an authorized release exists. Its approved version
must then replace both this example and STLab's dependency/minimum install
requirement together, without guessing a floor now.

For a locally installed execution package:

```cmake
find_package(stlab-execution CONFIG REQUIRED)
target_link_libraries(app PRIVATE stlab::execution)
```

Set `CMAKE_PREFIX_PATH` to the install prefix. Source and installed consumers
include the same canonical headers, for example
`<stlab/concurrency/default_executor.hpp>` and `<stlab/pre_exit.hpp>`.
STLab users keep linking `stlab::stlab`; its public execution dependency supplies
these headers and the single runtime transitively. Execution never requires STLab.
The legacy `stlab-core` / `stlab::stlab-core` INTERFACE targets belong to STLab,
not to the standalone execution package.

**Rebuild all consumers.** Independent execution inline namespaces and library
filenames change the C++ ABI. Existing public `stlab::` names, include paths,
task-storage guards, and the v2 C scheduling ABI are preserved; this is source
compatibility, not compatibility with old prebuilt C++ binaries.

`STLAB_EXECUTION_INSTALL` defaults to ON for standalone builds and OFF as a
subproject. The generated `stlab/execution/config.hpp` belongs to this library,
with execution-specific version macros and an `execution_v*` inline namespace.
STLab's version/namespace and coroutine policy are independent. Its own
`STLAB_INSTALL` controls only STLab-owned artifacts; enabling either package's
installation does not silently enable the other. Header file sets do not overlap.

`STLAB_EXECUTION_SHARED` selects a shared runtime. `STLAB_CORE_SHARED` is the
legacy input spelling and effective compatibility macro. Differing supplied values
are rejected on a fresh configuration. On reconfiguration, a spelling changed
since the last resolved configuration wins over its unchanged cached counterpart,
even if that counterpart is redundantly supplied again; both caches are reconciled.
Without the new setting, legacy ON selects shared, as does
`BUILD_SHARED_LIBS=ON` on non-Windows platforms; otherwise the runtime is static.
The parent project's `BUILD_SHARED_LIBS` is not changed.

Backend options retain STLab's defaults and validation: `STLAB_THREAD_SYSTEM`,
`STLAB_TASK_SYSTEM`, `STLAB_MAIN_EXECUTOR`, `STLAB_TASK_POOL_MAXIMUM`,
and `STLAB_EMSCRIPTEN_PTHREADS`.
`STLAB_SANITIZER=address` retains runtime address-sanitizer instrumentation.

Coroutine configuration (`STLAB_NO_STD_COROUTINES` and `STLAB_STD_COROUTINES()`)
belongs exclusively to STLab. Execution neither resolves that option nor defines
that macro; its C++17 public interface is independent of STLab's coroutine policy
and either library's build standard.

Call `stlab::pre_exit()` exactly once before normal process exit when using
runtime services. Scheduling, timer retirement, pre-exit ordering, and versioned
C ABI entry points retain their STLab contracts; see adjacent header documentation.

## Documentation and platform CI

Header-adjacent contracts are canonical. `cmake --preset=docs` and
`cmake --build --preset=docs` generate the standalone API reference in
`build/docs/html`; Doxygen is required. There is no published execution
documentation site yet. The directory groups and main page are owned here,
without importing STLab's higher-level API sources.

CI covers Linux GCC/Clang, C++17/20, macOS native/portable and both TSan+UBSan
variants, Windows static/native and canonical shared native/portable, portable
main, Qt6/Qt5 main, both Emscripten runtimes, and Linux/Windows package consumers
(including Windows DLL deployment). Job configuration is not passing runtime
evidence; macOS/Qt/race checks remain pending until run.

Publication is a separate authorized operation: publish the toolkit first,
replace this repository's toolkit SHA with that actual release and verify without
overrides; publish execution next; then update STLab's pin and matching installed
dependency requirement and verify STLab without overrides. No release or minimum
version is inferred from local validation's `CPP_LIBRARY_VERSION` override.

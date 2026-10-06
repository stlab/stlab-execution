cmake_minimum_required(VERSION 3.24)

include(cmake/CPM.cmake)
# [DEPENDENCY] https://github.com/stlab/cpp-library/releases/tag/v5.5.0
CPMAddPackage(
  NAME cpp-library
  VERSION 5.5.0
  GITHUB_REPOSITORY stlab/cpp-library
  GIT_TAG b5273fdb8d0c356579883d8419fecb267804d0f2)
include("${cpp-library_SOURCE_DIR}/cpp-library.cmake")
cpp_library_enable_dependency_tracking()

set(CMAKE_CXX_STANDARD 20 CACHE STRING "The C++ standard to use")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
project(execution LANGUAGES CXX)
include(CTest)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ExecutionPlatform)
include(ExecutionConfig)

execution_configure_platform()

cpp_library_setup(
  DESCRIPTION "Portable task execution, timers, and process lifecycle"
  NAMESPACE stlab
  REQUIRES_CPP_VERSION 17
  INSTALL_OPTION STLAB_EXECUTION_INSTALL
  HEADERS
    concurrency/task.hpp
    concurrency/executor_base.hpp
    concurrency/default_executor.hpp
    concurrency/main_executor.hpp
    concurrency/immediate_executor.hpp
    concurrency/system_timer.hpp
    concurrency/set_current_thread_name.hpp
    pre_exit.hpp
    concurrency/detail/libdispatch_executor_group.hpp
  SOURCES
    pre_exit.cpp
    concurrency/executor_abi.cpp
    concurrency/core_shutdown.cpp)

get_target_property(execution_library_type execution TYPE)
if(execution_library_type STREQUAL "SHARED_LIBRARY")
  set(STLAB_EXECUTION_SHARED ON)
else()
  set(STLAB_EXECUTION_SHARED OFF)
endif()
execution_generate_config_file()
target_sources(execution PUBLIC
  FILE_SET headers TYPE HEADERS
  BASE_DIRS "${PROJECT_BINARY_DIR}/include"
  FILES "${PROJECT_BINARY_DIR}/include/stlab/execution/config.hpp")
target_include_directories(execution PUBLIC
  "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>")
target_compile_definitions(execution PRIVATE STLAB_CORE_BUILD)
target_compile_definitions(execution PUBLIC $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX>)
target_compile_options(execution PUBLIC
  $<$<CXX_COMPILER_ID:MSVC>:/EHsc>
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:AppleClang>>:-fexceptions>)
if(NOT WIN32)
  set_target_properties(execution PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

if(STLAB_TASK_SYSTEM STREQUAL "emscripten")
  target_sources(execution PRIVATE src/concurrency/cooperative_executor.cpp)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  target_sources(execution PRIVATE src/concurrency/system_timer_emscripten.cpp)
elseif(STLAB_TASK_SYSTEM STREQUAL "windows")
  target_sources(execution PRIVATE src/concurrency/system_timer_windows.cpp)
elseif(STLAB_TASK_SYSTEM STREQUAL "libdispatch")
  target_sources(execution PRIVATE src/concurrency/system_timer_libdispatch.cpp)
elseif(STLAB_TASK_SYSTEM STREQUAL "portable")
  target_sources(execution PRIVATE src/concurrency/system_timer_portable.cpp)
else()
  message(FATAL_ERROR "Unsupported native timer task system: ${STLAB_TASK_SYSTEM}")
endif()
if(STLAB_MAIN_EXECUTOR STREQUAL "portable")
  target_sources(execution PRIVATE src/concurrency/main_executor_portable.cpp)
elseif(STLAB_MAIN_EXECUTOR STREQUAL "libdispatch")
  target_sources(execution PRIVATE src/concurrency/main_executor_libdispatch.cpp)
elseif(STLAB_MAIN_EXECUTOR STREQUAL "qt5" OR STLAB_MAIN_EXECUTOR STREQUAL "qt6")
  target_sources(execution PRIVATE src/concurrency/main_executor_qt.cpp)
elseif(STLAB_MAIN_EXECUTOR STREQUAL "emscripten")
  target_sources(execution PRIVATE src/concurrency/main_executor_emscripten.cpp)
else()
  target_sources(execution PRIVATE src/concurrency/main_executor_none.cpp)
endif()

if(STLAB_SANITIZER STREQUAL "address")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(execution PRIVATE /fsanitize=address)
    target_link_options(execution PRIVATE /INCREMENTAL:NO)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(execution PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(execution PUBLIC -fsanitize=address)
  endif()
endif()
if(STLAB_EXECUTION_SHARED AND WIN32)
  target_sources(execution PRIVATE src/execution.def)
endif()

if(NOT STLAB_THREAD_SYSTEM STREQUAL "none")
  target_link_libraries(execution PUBLIC Threads::Threads)
endif()
if(STLAB_TASK_SYSTEM STREQUAL "libdispatch" OR STLAB_MAIN_EXECUTOR STREQUAL "libdispatch")
  target_link_libraries(execution PUBLIC libdispatch::libdispatch)
  if(STLAB_EXECUTION_INSTALL)
    # Export the resolver as a package-local config without changing consumers' module paths.
    cpp_library_map_dependency(libdispatch::libdispatch
      [[libdispatch CONFIG PATHS "${CMAKE_CURRENT_LIST_DIR}" NO_DEFAULT_PATH]])
    install(FILES cmake/Findlibdispatch.cmake
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/stlab-execution"
      RENAME libdispatchConfig.cmake)
  endif()
endif()
if(STLAB_MAIN_EXECUTOR STREQUAL "qt5")
  target_link_libraries(execution PUBLIC Qt5::Core)
elseif(STLAB_MAIN_EXECUTOR STREQUAL "qt6")
  target_link_libraries(execution PUBLIC Qt6::Core)
endif()

message(STATUS "execution: Thread System: ${STLAB_THREAD_SYSTEM}")
message(STATUS "execution: Task System: ${STLAB_TASK_SYSTEM}")
message(STATUS "execution: Main Executor: ${STLAB_MAIN_EXECUTOR}")

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
  add_subdirectory(test)
endif()

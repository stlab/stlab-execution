# Distributed under the Boost Software License, Version 1.0.
# Backend settings and validation retained from STLab.

# Discovers dependencies and validates the selected execution backends.
# Macro scope keeps the discovered imported targets and configuration available to the caller.
macro(execution_configure_platform)
  # [DEPENDENCY] https://github.com/swiftlang/swift-corelibs-libdispatch
  find_package(libdispatch)
  # [DEPENDENCY] https://code.qt.io/cgit/qt/qtbase.git/
  find_package(Qt5 QUIET COMPONENTS Core)
  # [DEPENDENCY] https://code.qt.io/cgit/qt/qtbase.git/
  find_package(Qt6 QUIET COMPONENTS Core)
  find_package(Threads)

  if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    option(STLAB_EMSCRIPTEN_PTHREADS "Build Emscripten targets with pthread support." ON)
    include(CheckCXXSourceCompiles)
    check_cxx_source_compiles("
      #ifndef __EMSCRIPTEN_PTHREADS__
      #error Pthread support is not enabled
      #endif
      int main() {}
    " STLAB_EMSCRIPTEN_HAS_PTHREADS)
    if(STLAB_EMSCRIPTEN_PTHREADS AND NOT STLAB_EMSCRIPTEN_HAS_PTHREADS)
      message(FATAL_ERROR "STLAB_EMSCRIPTEN_PTHREADS=ON requires a toolchain with -pthread.")
    elseif(NOT STLAB_EMSCRIPTEN_PTHREADS AND STLAB_EMSCRIPTEN_HAS_PTHREADS)
      message(FATAL_ERROR "STLAB_EMSCRIPTEN_PTHREADS=OFF requires a toolchain without -pthread.")
    endif()
  endif()

  execution_detect_thread_system(STLAB_DEFAULT_THREAD_SYSTEM)
  set(STLAB_THREAD_SYSTEM ${STLAB_DEFAULT_THREAD_SYSTEM} CACHE STRING
    "Thread system to use (win32|pthread|pthread-emscripten|pthread-apple|none)")
  set(STLAB_TASK_POOL_MAXIMUM 0 CACHE STRING
    "Define the maximum number threads in the task pool. Default of zero implies a pool size of std::thread::hardware_concurrency. Non-zero implies STLAB_TASK_SYSTEM=portable.")
  if(NOT STLAB_TASK_POOL_MAXIMUM MATCHES "^[0-9]+$")
    message(FATAL_ERROR "STLAB_TASK_POOL_MAXIMUM must be a non-negative, decimal integer.")
  endif()
  option(STLAB_MINIMAL_TASK_POOL "Deprecated: Use STLAB_TASK_POOL_MAXIMUM=1." OFF)
  if(STLAB_MINIMAL_TASK_POOL)
    message(WARNING "STLAB_MINIMAL_TASK_POOL is deprecated. Use STLAB_TASK_POOL_MAXIMUM=1, instead.")
    if(STLAB_TASK_POOL_MAXIMUM EQUAL 0)
      set(STLAB_TASK_POOL_MAXIMUM 1)
    elseif(STLAB_TASK_POOL_MAXIMUM GREATER 1)
      message(FATAL_ERROR "STLAB_MINIMAL_TASK_POOL is deprecated and incompatible with STLAB_TASK_POOL_MAXIMUM>1. Use STLAB_TASK_POOL_MAXIMUM to set the desired pool size.")
    endif()
  endif()
  if(STLAB_TASK_POOL_MAXIMUM GREATER 0)
    if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten" AND NOT STLAB_EMSCRIPTEN_PTHREADS)
      message(FATAL_ERROR "Threadless Emscripten does not support STLAB_TASK_POOL_MAXIMUM>0.")
    endif()
    set(STLAB_TASK_SYSTEM "portable" CACHE STRING
      "Task system to use (portable|libdispatch|windows)." FORCE)
  else()
    execution_detect_task_system(STLAB_DEFAULT_TASK_SYSTEM)
  endif()
  set(STLAB_TASK_SYSTEM ${STLAB_DEFAULT_TASK_SYSTEM} CACHE STRING
    "Task system to use (portable|libdispatch|windows|emscripten).")
  set_property(CACHE STLAB_TASK_SYSTEM PROPERTY STRINGS portable libdispatch windows emscripten)
  if(NOT STLAB_TASK_SYSTEM MATCHES "^(portable|libdispatch|windows|emscripten)$")
    message(FATAL_ERROR "Unknown STLAB_TASK_SYSTEM: ${STLAB_TASK_SYSTEM}.")
  endif()

  execution_detect_main_executor(STLAB_DEFAULT_MAIN_EXECUTOR)
  set(STLAB_MAIN_EXECUTOR ${STLAB_DEFAULT_MAIN_EXECUTOR} CACHE STRING
    "Main executor to use (qt5|qt6|libdispatch|emscripten|portable|none).")
  set_property(CACHE STLAB_MAIN_EXECUTOR PROPERTY STRINGS qt5 qt6 libdispatch emscripten portable none)
  if(NOT STLAB_MAIN_EXECUTOR MATCHES "^(qt5|qt6|libdispatch|emscripten|portable|none)$")
    message(FATAL_ERROR "STLAB_MAIN_EXECUTOR must be one of qt5|qt6|libdispatch|emscripten|portable|none (got \"${STLAB_MAIN_EXECUTOR}\").")
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    if(NOT STLAB_EMSCRIPTEN_PTHREADS)
      if(NOT STLAB_THREAD_SYSTEM STREQUAL "none" OR
         NOT STLAB_TASK_SYSTEM STREQUAL "emscripten" OR
         NOT STLAB_MAIN_EXECUTOR STREQUAL "emscripten")
        message(FATAL_ERROR
          "Threadless Emscripten requires STLAB_THREAD_SYSTEM=none, STLAB_TASK_SYSTEM=emscripten, and STLAB_MAIN_EXECUTOR=emscripten.")
      endif()
    elseif(STLAB_THREAD_SYSTEM STREQUAL "none" OR STLAB_TASK_SYSTEM STREQUAL "emscripten")
      message(FATAL_ERROR "Pthread-enabled Emscripten requires a threaded task system.")
    endif()
  elseif(STLAB_TASK_SYSTEM STREQUAL "emscripten")
    message(FATAL_ERROR "STLAB_TASK_SYSTEM=emscripten is only supported on threadless Emscripten.")
  endif()
  if(NOT STLAB_THREAD_SYSTEM STREQUAL "none" AND NOT Threads_FOUND)
    message(SEND_ERROR "STLAB_THREAD_SYSTEM is not \"none\", but a thread system was not found.")
  endif()
  if(STLAB_TASK_SYSTEM STREQUAL "libdispatch" AND NOT libdispatch_FOUND)
    message(SEND_ERROR "STLAB_TASK_SYSTEM is set to \"libdispatch\", but a libdispatch installation was not found.")
  endif()
  if(STLAB_MAIN_EXECUTOR STREQUAL "libdispatch" AND NOT libdispatch_FOUND)
    message(SEND_ERROR "STLAB_MAIN_EXECUTOR is set to \"libdispatch\", but a libdispatch installation was not found.")
  endif()
  if(STLAB_MAIN_EXECUTOR STREQUAL "qt5" AND NOT Qt5Core_FOUND)
    message(SEND_ERROR "STLAB_MAIN_EXECUTOR is set to \"qt5\", but a Qt5 installation was not found.")
  endif()
  if(STLAB_MAIN_EXECUTOR STREQUAL "qt6" AND NOT Qt6Core_FOUND)
    message(SEND_ERROR "STLAB_MAIN_EXECUTOR is set to \"qt6\", but a Qt6 installation was not found.")
  endif()
endmacro()

# Generates the execution-owned feature, scheduler, export, and version macros.
function(execution_generate_config_file)
  if(STLAB_THREAD_SYSTEM STREQUAL "win32")
    set(STLAB_THREADS_WIN32 TRUE)
  elseif(STLAB_THREAD_SYSTEM STREQUAL "pthread")
    set(STLAB_THREADS_PTHREAD TRUE)
  elseif(STLAB_THREAD_SYSTEM STREQUAL "pthread-emscripten")
    set(STLAB_THREADS_PTHREAD_EMSCRIPTEN TRUE)
  elseif(STLAB_THREAD_SYSTEM STREQUAL "pthread-apple")
    set(STLAB_THREADS_PTHREAD_APPLE TRUE)
  else()
    set(STLAB_THREADS_NONE TRUE)
  endif()
  if(STLAB_TASK_SYSTEM STREQUAL "portable")
    set(STLAB_TASK_SYSTEM_PORTABLE TRUE)
  elseif(STLAB_TASK_SYSTEM STREQUAL "libdispatch")
    set(STLAB_TASK_SYSTEM_LIBDISPATCH TRUE)
  elseif(STLAB_TASK_SYSTEM STREQUAL "emscripten")
    set(STLAB_TASK_SYSTEM_EMSCRIPTEN TRUE)
  elseif(STLAB_TASK_SYSTEM STREQUAL "windows")
    set(STLAB_TASK_SYSTEM_WINDOWS TRUE)
  endif()
  if(STLAB_MAIN_EXECUTOR STREQUAL "libdispatch")
    set(STLAB_MAIN_EXECUTOR_LIBDISPATCH TRUE)
  elseif(STLAB_MAIN_EXECUTOR STREQUAL "emscripten")
    set(STLAB_MAIN_EXECUTOR_EMSCRIPTEN TRUE)
  elseif(STLAB_MAIN_EXECUTOR STREQUAL "qt5")
    set(STLAB_MAIN_EXECUTOR_QT5 TRUE)
  elseif(STLAB_MAIN_EXECUTOR STREQUAL "qt6")
    set(STLAB_MAIN_EXECUTOR_QT6 TRUE)
  elseif(STLAB_MAIN_EXECUTOR STREQUAL "portable")
    set(STLAB_MAIN_EXECUTOR_PORTABLE TRUE)
  elseif(STLAB_MAIN_EXECUTOR STREQUAL "none")
    set(STLAB_MAIN_EXECUTOR_NONE TRUE)
  endif()
  configure_file(
    "${PROJECT_SOURCE_DIR}/include/stlab/execution/config.hpp.in"
    "${PROJECT_BINARY_DIR}/include/stlab/execution/config.hpp" @ONLY)
endfunction()

# Exercise non-Windows compatibility defaults without requiring a cross compiler.
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/ExecutionConfig.cmake")
set(WIN32 FALSE)
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(STLAB_CORE_SHARED OFF CACHE BOOL "" FORCE)

execution_resolve_library_type()
if(NOT execution_library_type STREQUAL "SHARED" OR NOT STLAB_CORE_SHARED)
  message(FATAL_ERROR "Non-Windows BUILD_SHARED_LIBS=ON must select shared execution.")
endif()

# Settings produced by configuration must remain compatible on the next configure.
unset(STLAB_CORE_SHARED)
execution_resolve_library_type()
if(NOT execution_library_type STREQUAL "SHARED" OR NOT STLAB_CORE_SHARED OR
   NOT BUILD_SHARED_LIBS)
  message(FATAL_ERROR "Reconfiguration must preserve shared execution and the parent setting.")
endif()

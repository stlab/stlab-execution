# Check real configure caches, the generated public macro, and the compiled target type.
cmake_minimum_required(VERSION 3.24)
if(NOT DEFINED TEST_BINARY_DIR OR NOT DEFINED CPM_cpp-library_SOURCE)
  message(FATAL_ERROR "TEST_BINARY_DIR and CPM_cpp-library_SOURCE are required.")
endif()
get_filename_component(source_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/check_node_configuration.cmake")
file(REMOVE_RECURSE "${TEST_BINARY_DIR}")
file(MAKE_DIRECTORY "${TEST_BINARY_DIR}/client")
file(WRITE "${TEST_BINARY_DIR}/client/CMakeLists.txt"
  "cmake_minimum_required(VERSION 3.24)\n"
  "project(execution_client LANGUAGES CXX)\n"
  "set(BUILD_SHARED_LIBS \${CLIENT_SHARED_LIBS})\n"
  "add_subdirectory(\"${source_dir}\" execution)\n"
  "if(NOT BUILD_SHARED_LIBS STREQUAL CLIENT_SHARED_LIBS)\n"
  "  message(FATAL_ERROR \"Execution changed the client's BUILD_SHARED_LIBS\")\n"
  "endif()\n")

# Configures one fresh or existing build directory and checks its public selection.
function(check_selection case expected selection)
  set(binary_dir "${TEST_BINARY_DIR}/${case}")
  file(MAKE_DIRECTORY "${binary_dir}/.cmake/api/v1/query")
  file(WRITE "${binary_dir}/.cmake/api/v1/query/codemodel-v2" "")
  set(platform_options)
  if(TEST_TOOLCHAIN)
    # Keep the semicolon-separated flags in one argument when this list is expanded.
    string(REPLACE ";" "\\;" node_flags_argument "${NODE_JS_FLAGS}")
    list(APPEND platform_options "-DCMAKE_TOOLCHAIN_FILE:FILEPATH=${TEST_TOOLCHAIN}"
      "-DEM_CONFIG_EXECUTABLE:FILEPATH=${EM_CONFIG_EXECUTABLE}"
      "-DNODE_JS_EXECUTABLE:FILEPATH=${NODE_JS_EXECUTABLE}"
      "-DNODE_JS_FLAGS:STRING=${node_flags_argument}"
      "-DCMAKE_PROJECT_INCLUDE:FILEPATH=${CMAKE_CURRENT_LIST_DIR}/record_node_emulator.cmake"
      "-DSTLAB_EMSCRIPTEN_PTHREADS:BOOL=${STLAB_EMSCRIPTEN_PTHREADS}")
  endif()
  set(shared_options)
  set(configure_source "${source_dir}")
  set(execution_binary_dir "${binary_dir}")
  if(ARGN STREQUAL "PARENT")
    set(configure_source "${TEST_BINARY_DIR}/client")
    set(execution_binary_dir "${binary_dir}/execution")
    list(APPEND shared_options -UBUILD_SHARED_LIBS "-DCLIENT_SHARED_LIBS=${selection}")
  elseif(selection STREQUAL "UNSET")
    list(APPEND shared_options -UBUILD_SHARED_LIBS)
  else()
    list(APPEND shared_options "-DBUILD_SHARED_LIBS:BOOL=${selection}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${configure_source}" -B "${binary_dir}" -G Ninja
      "-DCPM_cpp-library_SOURCE:PATH=${CPM_cpp-library_SOURCE}"
      -DBUILD_TESTING=OFF ${shared_options} ${platform_options}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "${case}: configure exit ${result}\n${output}${error}")
  endif()

  file(READ "${binary_dir}/CMakeCache.txt" cache)
  if(TEST_SYSTEM_NAME STREQUAL "Emscripten")
    check_node_configuration("${binary_dir}" "${NODE_JS_EXECUTABLE}" "${NODE_JS_FLAGS}")
  endif()
  if(selection STREQUAL "UNSET" OR ARGN STREQUAL "PARENT")
    if(cache MATCHES "BUILD_SHARED_LIBS:")
      message(FATAL_ERROR "${case}: unset BUILD_SHARED_LIBS was added to the cache.")
    endif()
  elseif(NOT cache MATCHES "BUILD_SHARED_LIBS:BOOL=${selection}[\r\n]")
    message(FATAL_ERROR "${case}: client BUILD_SHARED_LIBS was changed.")
  endif()
  if(expected)
    set(macro_value 1)
    set(target_type SHARED_LIBRARY)
  else()
    set(macro_value 0)
    set(target_type STATIC_LIBRARY)
  endif()
  file(READ "${execution_binary_dir}/include/stlab/execution/config.hpp" config)
  if(NOT config MATCHES "#define STLAB_EXECUTION_SHARED\\(\\) ${macro_value}[\r\n]" OR
     NOT config MATCHES "#define STLAB_CORE_SHARED\\(\\) STLAB_EXECUTION_SHARED\\(\\)")
    message(FATAL_ERROR "${case}: public shared macros do not match ${expected}.")
  endif()

  file(GLOB indexes "${binary_dir}/.cmake/api/v1/reply/index-*.json")
  list(SORT indexes)
  list(GET indexes -1 index)
  file(READ "${index}" index_json)
  string(JSON model_file GET "${index_json}" reply codemodel-v2 jsonFile)
  file(READ "${binary_dir}/.cmake/api/v1/reply/${model_file}" model)
  string(JSON target_count LENGTH "${model}" configurations 0 targets)
  math(EXPR target_last "${target_count} - 1")
  set(found OFF)
  foreach(i RANGE ${target_last})
    string(JSON name GET "${model}" configurations 0 targets ${i} name)
    if(name STREQUAL "execution")
      string(JSON target_file GET "${model}" configurations 0 targets ${i} jsonFile)
      file(READ "${binary_dir}/.cmake/api/v1/reply/${target_file}" target)
      string(JSON type GET "${target}" type)
      if(NOT type STREQUAL target_type)
        message(FATAL_ERROR "${case}: execution target is ${type}, expected ${target_type}.")
      endif()
      set(found ON)
    endif()
  endforeach()
  if(NOT found)
    message(FATAL_ERROR "${case}: execution target missing from codemodel.")
  endif()
  message(STATUS "${case}: ${expected}, ${target_type}, macros agree (exit 0)")
endfunction()

# Emscripten's toolchain shadows BUILD_SHARED_LIBS with a normal OFF variable.
if(TEST_SYSTEM_NAME STREQUAL "Emscripten")
  set(shared_expected OFF)
else()
  set(shared_expected ON)
endif()
check_selection(default OFF UNSET)
check_selection(explicit-static OFF OFF)
check_selection(explicit-shared ${shared_expected} ON)
check_selection(reconfigure OFF OFF)
check_selection(reconfigure ${shared_expected} ON)
check_selection(reconfigure OFF OFF)
check_selection(reconfigure ${shared_expected} ON)
check_selection(reconfigure OFF UNSET)
if(NOT TEST_SYSTEM_NAME STREQUAL "Emscripten")
  check_selection(parent-static OFF OFF PARENT)
  check_selection(parent-shared ON ON PARENT)
endif()

# Distributed under the Boost Software License, Version 1.0.

cmake_minimum_required(VERSION 3.24)
if(NOT DEFINED TEST_BINARY_DIR OR NOT DEFINED TOOLKIT_SOURCE OR NOT DEFINED TEST_GENERATOR)
  message(FATAL_ERROR "TEST_BINARY_DIR, TOOLKIT_SOURCE, and TEST_GENERATOR are required.")
endif()
get_filename_component(source_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(fixture "${TEST_BINARY_DIR}/fixture")
file(MAKE_DIRECTORY "${fixture}")
file(WRITE "${fixture}/CMakeLists.txt"
  "cmake_minimum_required(VERSION 3.24)\n"
  "project(pool_configuration LANGUAGES CXX)\n"
  "set(BUILD_TESTING OFF CACHE BOOL \"\" FORCE)\n"
  "add_subdirectory(\"${source_dir}\" execution)\n"
  "add_executable(pool_configuration main.cpp)\n"
  "target_link_libraries(pool_configuration PRIVATE stlab::execution)\n"
  "target_compile_definitions(pool_configuration PRIVATE EXPECTED_MAXIMUM=\${EXPECTED_MAXIMUM})\n")
file(WRITE "${fixture}/main.cpp"
  "#include <stlab/execution/config.hpp>\n"
  "static_assert(STLAB_TASK_POOL_MAXIMUM() == EXPECTED_MAXIMUM);\n"
  "int main() {}\n")
set(configure_args -G "${TEST_GENERATOR}"
  "-DCMAKE_BUILD_TYPE:STRING=${TEST_CONFIG}"
  "-DCMAKE_CXX_COMPILER:FILEPATH=${TEST_COMPILER}"
  "-DCMAKE_MAKE_PROGRAM:FILEPATH=${TEST_MAKE_PROGRAM}")
if(TEST_GENERATOR_PLATFORM)
  list(APPEND configure_args -A "${TEST_GENERATOR_PLATFORM}")
endif()
if(TEST_GENERATOR_TOOLSET)
  list(APPEND configure_args -T "${TEST_GENERATOR_TOOLSET}")
endif()

foreach(input IN ITEMS 010 00017 000)
  if(input STREQUAL "010")
    set(expected 10)
  elseif(input STREQUAL "00017")
    set(expected 17)
  else()
    set(expected 0)
  endif()
  set(binary_dir "${TEST_BINARY_DIR}/${input}")
  execute_process(COMMAND "${CMAKE_COMMAND}" -S "${fixture}" -B "${binary_dir}" ${configure_args}
    "-DCPM_cpp-library_SOURCE:PATH=${TOOLKIT_SOURCE}"
    "-DSTLAB_TASK_POOL_MAXIMUM=${input}" "-DEXPECTED_MAXIMUM=${expected}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "${input}: configure failed\n${output}${error}")
  endif()
  execute_process(COMMAND "${CMAKE_COMMAND}" --build "${binary_dir}" --config "${TEST_CONFIG}"
    --target pool_configuration
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "${input}: generated pool maximum is not decimal ${expected}\n${output}${error}")
  endif()
endforeach()

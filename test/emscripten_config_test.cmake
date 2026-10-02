# Each incompatible configuration must fail with the intended diagnostic, not a toolchain error.
# Require nested toolchain checks to inherit the explicitly selected SDK.
include("${CMAKE_CURRENT_LIST_DIR}/check_node_configuration.cmake")
get_filename_component(em_config_dir "${em_config}" DIRECTORY)
cmake_path(CONVERT "$ENV{PATH}" TO_CMAKE_PATH_LIST search_path NORMALIZE)
list(REMOVE_ITEM search_path "${em_config_dir}")
cmake_path(CONVERT "${search_path}" TO_NATIVE_PATH_LIST search_path)
set(ENV{PATH} "${search_path}")
foreach(scenario IN ITEMS task main threads pool compiler)
  if(scenario STREQUAL "task")
    set(option -DSTLAB_TASK_SYSTEM=portable)
    set(diagnostic "Threadless Emscripten requires")
  elseif(scenario STREQUAL "main")
    set(option -DSTLAB_MAIN_EXECUTOR=none)
    set(diagnostic "Threadless Emscripten requires")
  elseif(scenario STREQUAL "threads")
    set(option -DSTLAB_THREAD_SYSTEM=pthread-emscripten)
    set(diagnostic "Threadless Emscripten requires")
  elseif(scenario STREQUAL "pool")
    set(option -DSTLAB_TASK_POOL_MAXIMUM=1)
    set(diagnostic "Threadless Emscripten does not support")
  else()
    set(option -DCMAKE_CXX_FLAGS=-pthread)
    set(diagnostic "requires a toolchain without -pthread")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --preset=test-emscripten-threadless
      -B "${binary_dir}/${scenario}" "-DEM_CONFIG_EXECUTABLE:FILEPATH=${em_config}"
      "-DCPM_cpp-library_SOURCE:PATH=${cpp_library_source}"
      "-DNODE_JS_EXECUTABLE:FILEPATH=${node}"
      "-DNODE_JS_FLAGS:STRING=${NODE_JS_FLAGS}"
      "-DCMAKE_PROJECT_INCLUDE:FILEPATH=${CMAKE_CURRENT_LIST_DIR}/record_node_emulator.cmake"
      -DBUILD_TESTING=OFF ${option}
    WORKING_DIRECTORY "${source_dir}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
    TIMEOUT 90)
  if(result STREQUAL "0" OR NOT "${output}${error}" MATCHES "${diagnostic}")
    message(FATAL_ERROR "${scenario}: wrong configuration result: ${result}\n${output}${error}")
  endif()
  check_node_configuration("${binary_dir}/${scenario}" "${node}" "${NODE_JS_FLAGS}")
endforeach()

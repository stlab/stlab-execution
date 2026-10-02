file(MAKE_DIRECTORY "${TEST_BINARY_DIR}/sdk/cmake/Modules/Platform")
file(WRITE "${TEST_BINARY_DIR}/sdk/cmake/Modules/Platform/Emscripten.cmake" "")
if(CMAKE_HOST_WIN32)
  set(em_config "${TEST_BINARY_DIR}/em-config.cmd")
  set(node "${TEST_BINARY_DIR}/node.cmd")
  file(WRITE "${em_config}" "@echo off\r\necho ${TEST_BINARY_DIR}/sdk\r\n")
else()
  set(em_config "${TEST_BINARY_DIR}/em-config")
  set(node "${TEST_BINARY_DIR}/node")
  file(WRITE "${em_config}" "#!/bin/sh\nprintf '%s\\n' '${TEST_BINARY_DIR}/sdk'\n")
  file(CHMOD "${em_config}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
endif()

foreach(version IN ITEMS 16.16.0 18.2.0 18.3.0 22.0.0)
  if(CMAKE_HOST_WIN32)
    file(WRITE "${node}" "@echo off\r\necho v${version}\r\n")
  else()
    file(WRITE "${node}" "#!/bin/sh\nprintf '%s\\n' 'v${version}'\n")
    file(CHMOD "${node}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
  endif()
  execute_process(COMMAND "${CMAKE_COMMAND}"
    "-DEM_CONFIG_EXECUTABLE:FILEPATH=${em_config}"
    "-DNODE_JS_EXECUTABLE:FILEPATH=${node}"
    "-DNODE_JS_FLAGS:STRING=--no-warnings;--stack-trace-limit=20"
    "-DTEST_TOOLCHAIN:FILEPATH=${TEST_TOOLCHAIN}"
    -P "${CMAKE_CURRENT_LIST_DIR}/node_minimum_child.cmake"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(version VERSION_LESS "18.3.0")
    if(result STREQUAL "0" OR NOT "${output}${error}" MATCHES "Unsupported node")
      message(FATAL_ERROR "Node ${version} must be rejected:\n${output}${error}")
    endif()
  elseif(NOT result STREQUAL "0")
    message(FATAL_ERROR "Node ${version} must be accepted:\n${output}${error}")
  endif()
endforeach()

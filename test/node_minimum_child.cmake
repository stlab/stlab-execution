include("${TEST_TOOLCHAIN}")
if(NOT CMAKE_CROSSCOMPILING_EMULATOR STREQUAL "${NODE_JS_EXECUTABLE};${NODE_JS_FLAGS}")
  message(FATAL_ERROR "The selected Node executable and flags must be preserved.")
endif()

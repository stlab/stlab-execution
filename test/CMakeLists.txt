get_target_property(execution_alias stlab::execution ALIASED_TARGET)
if(NOT execution_alias STREQUAL "execution")
  message(FATAL_ERROR "The build target must be stlab::execution, matching the installed target.")
endif()
add_executable(execution.test.header_smoke header_smoke.cpp)
target_link_libraries(execution.test.header_smoke PRIVATE stlab::execution)
if(WIN32 AND STLAB_EXECUTION_SHARED)
  add_custom_command(TARGET execution.test.header_smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:execution.test.header_smoke>
      $<TARGET_FILE_DIR:execution.test.header_smoke>
    COMMAND_EXPAND_LISTS)
endif()
add_test(NAME execution.test.header_smoke COMMAND execution.test.header_smoke)
option(STLAB_EXECUTION_PACKAGE_TESTS "Verify native installed execution consumers" OFF)
if(STLAB_EXECUTION_PACKAGE_TESTS AND NOT CMAKE_CROSSCOMPILING)
  add_test(NAME execution.package.roundtrip
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTION_SOURCE:PATH=${PROJECT_SOURCE_DIR}"
      "-DTOOLKIT_SOURCE:PATH=${cpp-library_SOURCE_DIR}"
      "-DVERIFY_PARENT:PATH=${PROJECT_BINARY_DIR}/package-test"
      "-DVERIFY_ROOT:PATH=${PROJECT_BINARY_DIR}/package-test/execution-packages"
      "-DSTLAB_EXECUTION_SHARED:BOOL=${STLAB_EXECUTION_SHARED}"
      "-DSTLAB_TASK_SYSTEM=${STLAB_TASK_SYSTEM}"
      "-DTEST_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
      "-DTEST_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_package.cmake")
  set_tests_properties(execution.package.roundtrip PROPERTIES TIMEOUT 600 LABELS package)
endif()
add_test(NAME execution.config.shared_selection
  COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/shared_selection.cmake")
add_test(NAME execution.config.shared_reconfiguration
  COMMAND ${CMAKE_COMMAND}
    "-DTEST_BINARY_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/shared_reconfiguration"
    "-DTEST_PARENT_BINARY_DIR:PATH=${PROJECT_BINARY_DIR}"
    "-DCPM_cpp-library_SOURCE:PATH=${cpp-library_SOURCE_DIR}"
    "-DTEST_TOOLCHAIN:FILEPATH=${CMAKE_TOOLCHAIN_FILE}"
    "-DTEST_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
    "-DEM_CONFIG_EXECUTABLE:FILEPATH=${EM_CONFIG_EXECUTABLE}"
    "-DNODE_JS_EXECUTABLE:FILEPATH=${NODE_JS_EXECUTABLE}"
    "-DNODE_JS_FLAGS:STRING=${NODE_JS_FLAGS}"
    "-DSTLAB_EMSCRIPTEN_PTHREADS:BOOL=${STLAB_EMSCRIPTEN_PTHREADS}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/shared_reconfiguration.cmake")
set_tests_properties(execution.test.header_smoke execution.config.shared_selection
  PROPERTIES TIMEOUT 60)
set_tests_properties(execution.config.shared_reconfiguration PROPERTIES TIMEOUT 450)

# Each public header must compile without relying on another include preceding it.
foreach(header IN ITEMS
    concurrency/task.hpp
    concurrency/executor_base.hpp
    concurrency/default_executor.hpp
    concurrency/main_executor.hpp
    concurrency/immediate_executor.hpp
    concurrency/system_timer.hpp
    concurrency/set_current_thread_name.hpp
    pre_exit.hpp
    execution/config.hpp)
  string(REPLACE "/" "." header_target "${header}")
  set(source "${CMAKE_CURRENT_BINARY_DIR}/${header_target}.cpp")
  file(WRITE "${source}" "#include <stlab/${header}>\n")
  add_library(execution.header.${header_target} OBJECT "${source}")
  target_link_libraries(execution.header.${header_target} PRIVATE stlab::execution)
endforeach()

# Matches the runtime's sanitizer instrumentation, including MSVC STL annotations.
function(execution_test_sanitizer target)
  if(STLAB_SANITIZER STREQUAL "address")
    if(MSVC)
      target_compile_options(${target} PRIVATE /fsanitize=address /Zi)
      target_link_options(${target} PRIVATE /INCREMENTAL:NO /DEBUG)
    else()
      target_compile_options(${target} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=address)
    endif()
  endif()
endfunction()

# Links a standalone test and deploys its runtime DLLs beside the executable.
function(execution_test_target target)
  add_executable(${target} ${ARGN})
  target_link_libraries(${target} PRIVATE stlab::execution)
  if(WIN32 AND STLAB_EXECUTION_SHARED)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:${target}> $<TARGET_FILE_DIR:${target}>
      COMMAND_EXPAND_LISTS)
  endif()
  execution_test_sanitizer(${target})
endfunction()

execution_test_sanitizer(execution.test.header_smoke)

# Registers a standalone process with a bounded lifecycle.
function(execution_process_test target timeout)
  execution_test_target(${target} ${ARGN})
  add_test(NAME ${target} COMMAND ${target})
  set_tests_properties(${target} PROPERTIES TIMEOUT ${timeout})
endfunction()

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  execution_process_test(execution.test.emscripten_timer 60 emscripten_timer_test.cpp)
  foreach(scenario IN ITEMS abi_guard negative_abi_delay cancel_capture_reentry)
    add_test(NAME execution.test.emscripten_timer_${scenario}
      COMMAND ${CMAKE_COMMAND} "-Demulator=${CMAKE_CROSSCOMPILING_EMULATOR}"
        "-Dexecutable=$<TARGET_FILE:execution.test.emscripten_timer>" "-Dscenario=${scenario}"
        "-Dexpected=EXPECTED_STLAB_TERMINATE|system timer task storage ABI mismatch|delay_ns >= 0|Scheduling a timer after pre_exit"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_emscripten_terminate.cmake")
    set_tests_properties(execution.test.emscripten_timer_${scenario} PROPERTIES TIMEOUT 60)
  endforeach()
  add_test(NAME execution.test.emscripten_timer_cancel_pending
    COMMAND execution.test.emscripten_timer cancel_pending)
  set_tests_properties(execution.test.emscripten_timer_cancel_pending PROPERTIES TIMEOUT 60)
  if(STLAB_EMSCRIPTEN_PTHREADS)
    add_test(NAME execution.test.emscripten_timer_shutdown_worker
      COMMAND execution.test.emscripten_timer shutdown_worker)
    set_tests_properties(execution.test.emscripten_timer_shutdown_worker PROPERTIES TIMEOUT 60)
  endif()
endif()

if(NOT STLAB_MAIN_EXECUTOR STREQUAL "none")
  set(main_scenarios order shutdown_order)
  if(NOT STLAB_THREAD_SYSTEM STREQUAL "none")
    list(APPEND main_scenarios concurrent)
  endif()
  if(STLAB_MAIN_EXECUTOR STREQUAL "portable")
    list(APPEND main_scenarios pre_exit pre_exit_before_submit)
  endif()
  foreach(scenario IN LISTS main_scenarios)
    execution_process_test(execution.test.main_executor_${scenario} 60
      main_executor_${scenario}_test.cpp main_executor_test_host.hpp)
  endforeach()
endif()

execution_test_target(execution.test.task_abi_mismatch task_abi_mismatch_test.cpp)
set_target_properties(execution.test.task_abi_mismatch PROPERTIES EXCLUDE_FROM_ALL TRUE)
add_test(NAME execution.test.task_abi_mismatch_link
  COMMAND ${CMAKE_COMMAND} "-Dbuild_dir=${PROJECT_BINARY_DIR}" "-Dconfig=$<CONFIG>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_task_abi_mismatch.cmake")
set_tests_properties(execution.test.task_abi_mismatch_link PROPERTIES TIMEOUT 60)

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten" AND NOT STLAB_EMSCRIPTEN_PTHREADS)
  execution_test_target(execution.test.cooperative_shutdown cooperative_shutdown_test.cpp)
  foreach(scenario IN ITEMS pending current empty normal_fifo outside timer_only)
    add_test(NAME execution.test.cooperative_shutdown_${scenario}
      COMMAND execution.test.cooperative_shutdown ${scenario})
    set_tests_properties(execution.test.cooperative_shutdown_${scenario} PROPERTIES TIMEOUT 60)
  endforeach()
  foreach(scenario IN ITEMS late_default late_high late_low shutdown_first repeat_pre_exit)
    add_test(NAME execution.test.cooperative_shutdown_${scenario}
      COMMAND ${CMAKE_COMMAND} "-Demulator=${CMAKE_CROSSCOMPILING_EMULATOR}"
        "-Dexecutable=$<TARGET_FILE:execution.test.cooperative_shutdown>" "-Dscenario=${scenario}"
        "-Dexpected=EXPECTED_STLAB_TERMINATE|core service use after pre_exit|pre_exit invoked more than once"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_emscripten_terminate.cmake")
    set_tests_properties(execution.test.cooperative_shutdown_${scenario} PROPERTIES TIMEOUT 60)
  endforeach()
  add_test(NAME execution.test.emscripten_configuration
    COMMAND ${CMAKE_COMMAND} "-Dsource_dir:PATH=${PROJECT_SOURCE_DIR}"
      "-Dbinary_dir:PATH=${PROJECT_BINARY_DIR}/configuration-tests"
      "-DTEST_PARENT_BINARY_DIR:PATH=${PROJECT_BINARY_DIR}"
      "-Dem_config:FILEPATH=${EM_CONFIG_EXECUTABLE}"
      "-Dcpp_library_source:PATH=${cpp-library_SOURCE_DIR}"
      "-Dnode:FILEPATH=${NODE_JS_EXECUTABLE}"
      "-DNODE_JS_FLAGS:STRING=${NODE_JS_FLAGS}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/emscripten_config_test.cmake")
  set_tests_properties(execution.test.emscripten_configuration PROPERTIES TIMEOUT 450)
  # Node's event loop cannot progress while a doctest case blocks on std::future.
  return()
endif()

CPMAddPackage("gh:doctest/doctest@2.5.2")
foreach(component IN ITEMS task executor executor_abi waiter_state system_timer)
  if(component STREQUAL "task" OR component STREQUAL "executor")
    set(source ${component}_contract_test.cpp)
  else()
    set(source ${component}_test.cpp)
  endif()
  execution_process_test(execution.test.${component} 60 ${source} main.cpp)
  target_link_libraries(execution.test.${component} PRIVATE doctest::doctest)
endforeach()

if(WIN32 AND STLAB_EXECUTION_SHARED)
  execution_process_test(execution.test.core_shared_smoke 60
    core_shared_smoke_test.cpp core_shared_smoke_main.cpp)
  target_link_libraries(execution.test.core_shared_smoke PRIVATE doctest::doctest)
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  execution_process_test(execution.test.executor_priority_shutdown 10 executor_priority_shutdown.cpp)
  execution_process_test(execution.test.core_shutdown_first 60 core_shutdown_first.cpp)
  set(timer_scenarios lifecycle shutdown_first executor_shutdown application_shutdown)
  if(WIN32 AND NOT STLAB_EXECUTION_SHARED AND NOT STLAB_SANITIZER STREQUAL "address")
    list(APPEND timer_scenarios resource_failure)
  endif()
  foreach(scenario IN LISTS timer_scenarios)
    execution_process_test(execution.test.system_timer_${scenario} 10 system_timer_${scenario}.cpp)
  endforeach()
endif()

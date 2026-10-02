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
add_test(NAME execution.config.shared_selection
  COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/shared_selection.cmake")

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

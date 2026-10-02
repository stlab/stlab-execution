# Observe the real child toolchain variable after project() has loaded it.
file(WRITE "${CMAKE_BINARY_DIR}/node-emulator.txt" "${CMAKE_CROSSCOMPILING_EMULATOR}")

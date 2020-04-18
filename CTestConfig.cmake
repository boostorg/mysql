set(CTEST_MEMORYCHECK_SUPPRESSIONS_FILE ${CMAKE_SOURCE_DIR}/test/common/valgrind_suppressions.txt)
set(CTEST_MEMORYCHECK_COMMAND_OPTIONS "--error-exitcode=1")
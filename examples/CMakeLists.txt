
find_package(Boost REQUIRED COMPONENTS coroutine)

function (_mysql_add_example EXECUTABLE_NAME CPPFILE)
	add_executable(
		${EXECUTABLE_NAME}
		${CPPFILE}
	)
	target_link_libraries(
		${EXECUTABLE_NAME}
		PRIVATE
		mysql_asio
		Boost::coroutine
	)
	_mysql_set_target_warnings(${EXECUTABLE_NAME})
endfunction()

set(MYSQL_EXAMPLES
  query_sync
  query_async_callbacks
  query_async_coroutines
  query_async_futures
  metadata
  prepared_statements
)

# We will autogenerate run_examples.bat and run_examples.sh, which will
# run in Windows and UNIX all given examples. This prevents code duplication.
set(BASH_CONTENT "")
set(BATCH_CONTENT "")
set(SETUP_SCRIPT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/db_setup.sql)

foreach(EXAMPLE_NAME ${MYSQL_EXAMPLES})
    set(EXECUTABLE_NAME "example_${EXAMPLE_NAME}")
    _mysql_add_example(${EXECUTABLE_NAME} "${EXAMPLE_NAME}.cpp")
    set(BASH_CONTENT "${BASH_CONTENT}./${EXECUTABLE_NAME} example_user example_password\n")
    set(BATCH_CONTENT "${BATCH_CONTENT}${EXECUTABLE_NAME} example_user example_password\n")
endforeach()

configure_file(
	run_examples.bat.in
	run_examples.bat
	@ONLY
)

configure_file(
	run_examples.sh.in
	run_examples.sh
	@ONLY
)

# Add them as automated tests
add_test(
	NAME examples
	COMMAND ${CMAKE_CURRENT_BINARY_DIR}/run_examples.${_MYSQL_SHELL_EXT}
)


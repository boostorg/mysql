
find_package(Boost REQUIRED COMPONENTS coroutine)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

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
	_mysql_common_target_settings(${EXECUTABLE_NAME})
endfunction()

set(MYSQL_EXAMPLES
  query_sync
  query_async_callbacks
  query_async_coroutines
  query_async_futures
  metadata
  prepared_statements
  unix_socket
)

# Examples setup
add_test(
	NAME mysql_examples_setup
	COMMAND
		${Python3_EXECUTABLE} 
		${CMAKE_SOURCE_DIR}/test/common/run_sql.py 
		${CMAKE_CURRENT_SOURCE_DIR}/db_setup.sql
)
set_tests_properties(mysql_examples_setup PROPERTIES FIXTURES_SETUP mysql_examples_fixture)

foreach(EXAMPLE_NAME ${MYSQL_EXAMPLES})
    set(EXECUTABLE_NAME "example_${EXAMPLE_NAME}")
    _mysql_add_example(${EXECUTABLE_NAME} "${EXAMPLE_NAME}.cpp")
    set(TEST_NAME "mysql_${EXECUTABLE_NAME}")
	add_test(
		NAME ${TEST_NAME}
		COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${EXECUTABLE_NAME} example_user example_password
	)
	set_tests_properties(${TEST_NAME} PROPERTIES FIXTURES_REQUIRED mysql_examples_fixture)
endforeach()

set_tests_properties(
	mysql_example_query_async_coroutines
	PROPERTIES LABELS "NoMemcheck"
)

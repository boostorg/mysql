
find_package(Boost REQUIRED COMPONENTS coroutine)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

if (BOOST_MYSQL_VALGRIND_TESTS)
	find_package(Mysqlvalgrind REQUIRED)
endif()

function (_mysql_build_example EXECUTABLE_NAME CPPFILE)
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

function (_mysql_test_example EXECUTABLE_NAME)
	set(EXECUTABLE_PATH ${CMAKE_CURRENT_BINARY_DIR}/${EXECUTABLE_NAME})
    set(TEST_NAME "mysql_${EXECUTABLE_NAME}")
	add_test(
		NAME ${TEST_NAME}
		COMMAND ${EXECUTABLE_PATH} example_user example_password
	)
	set_tests_properties(${TEST_NAME} PROPERTIES FIXTURES_REQUIRED mysql_examples_fixture)
endfunction()

function (_mysql_memcheck_example EXECUTABLE_NAME)
	set(EXECUTABLE_PATH ${CMAKE_CURRENT_BINARY_DIR}/${EXECUTABLE_NAME})
    set(TEST_NAME "mysql_${EXECUTABLE_NAME}_memcheck")
	Mysqlvalgrind_AddMemcheckTest(
		NAME ${TEST_NAME}
		COMMAND ${EXECUTABLE_PATH} example_user example_password
	)
	set_tests_properties(${TEST_NAME} PROPERTIES FIXTURES_REQUIRED mysql_examples_fixture)
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

set(MYSQL_EXAMPLES_NOMEMCHECK
  query_async_coroutines
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

    _mysql_build_example(${EXECUTABLE_NAME} "${EXAMPLE_NAME}.cpp")
    
	# Don't run examples twice. Some of them would require re-running setup
    if (BOOST_MYSQL_VALGRIND_TESTS AND NOT ${EXAMPLE_NAME} IN_LIST MYSQL_EXAMPLES_NOMEMCHECK)
    	_mysql_memcheck_example(${EXECUTABLE_NAME})
    else()
    	_mysql_test_example(${EXECUTABLE_NAME})
    endif()
endforeach()

#
# Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

find_package(Boost REQUIRED COMPONENTS coroutine)

# Compile the example
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

# Run it as a test
function (_mysql_test_example EXECUTABLE_NAME)
    set(EXECUTABLE_PATH ${CMAKE_CURRENT_BINARY_DIR}/${EXECUTABLE_NAME})
    set(TEST_NAME "mysql_${EXECUTABLE_NAME}")
    add_test(
        NAME ${TEST_NAME}
        COMMAND ${EXECUTABLE_PATH} example_user example_password
    )
    set_tests_properties(${TEST_NAME} PROPERTIES FIXTURES_REQUIRED mysql_examples_fixture)
endfunction()

# Run it as a test using Valgrind
function (_mysql_memcheck_example EXECUTABLE_NAME)
    set(EXECUTABLE_PATH ${CMAKE_CURRENT_BINARY_DIR}/${EXECUTABLE_NAME})
    set(TEST_NAME "mysql_${EXECUTABLE_NAME}_memcheck")
    Mysqlvalgrind_AddMemcheckTest(
        NAME ${TEST_NAME}
        COMMAND ${EXECUTABLE_PATH} example_user example_password
    )
    set_tests_properties(${TEST_NAME} PROPERTIES FIXTURES_REQUIRED mysql_examples_fixture)
endfunction()

# The list of all the examples we have
set(MYSQL_EXAMPLES
    query_sync
    query_async_callbacks
    query_async_coroutines
    query_async_futures
    metadata
    prepared_statements
    unix_socket
)

# The examples we do NOT want to ever memcheck
set(MYSQL_EXAMPLES_NOMEMCHECK
    query_async_coroutines
)

# Run setup
_mysql_sql_setup_fixture(
    TEST_NAME mysql_examples_setup
    FIXTURE_NAME mysql_examples_fixture
    SQL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/db_setup.sql
)

# Build and run examples
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

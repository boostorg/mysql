#
# Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

find_package(Boost REQUIRED COMPONENTS coroutine)

# Compile the example
function (build_example EXECUTABLE_NAME CPPFILE)
    add_executable(
        ${EXECUTABLE_NAME}
        ${CPPFILE}
    )
    target_link_libraries(
        ${EXECUTABLE_NAME}
        PRIVATE
        Boost::mysql
        Boost::coroutine
    )
    common_target_settings(${EXECUTABLE_NAME})
endfunction()

# Run it as a test
function (test_example EXECUTABLE_NAME)
    set(EXECUTABLE_PATH ${CMAKE_CURRENT_BINARY_DIR}/${EXECUTABLE_NAME})
    set(TEST_NAME ${EXECUTABLE_NAME})
    add_test(
        NAME ${TEST_NAME}
        COMMAND ${EXECUTABLE_PATH} example_user example_password
    )
endfunction()

# Run it as a test using Valgrind
function (memcheck_example EXECUTABLE_NAME)
    set(EXECUTABLE_PATH ${CMAKE_CURRENT_BINARY_DIR}/${EXECUTABLE_NAME})
    set(TEST_NAME "${EXECUTABLE_NAME}_memcheck")
    add_memcheck_test(
        NAME ${TEST_NAME}
        COMMAND ${EXECUTABLE_PATH} example_user example_password
    )
endfunction()

# The list of all the examples we have
set(BOOST_MYSQL_EXAMPLES
    tutorial
    value
    query_sync
    query_async_callbacks
    query_async_coroutines
    query_async_coroutinescpp20
    query_async_futures
    metadata
    prepared_statements
    default_completion_tokens
    unix_socket
    ssl
)

# The examples we do NOT want to ever memcheck
set(BOOST_MYSQL_EXAMPLES_NOMEMCHECK
    query_async_coroutines
)

# Build and run examples
foreach(EXAMPLE_NAME ${BOOST_MYSQL_EXAMPLES})
    set(EXECUTABLE_NAME "boost_mysql_example_${EXAMPLE_NAME}")

    build_example(${EXECUTABLE_NAME} "${EXAMPLE_NAME}.cpp")
    
    # Don't run examples twice. Some of them would require re-running setup
    if (BOOST_MYSQL_VALGRIND_TESTS AND NOT ${EXAMPLE_NAME} IN_LIST BOOST_MYSQL_EXAMPLES_NOMEMCHECK)
        memcheck_example(${EXECUTABLE_NAME})
    else()
        test_example(${EXECUTABLE_NAME})
    endif()
endforeach()

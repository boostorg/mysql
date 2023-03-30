#
# Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# Note: examples count as integration tests. This is only processed
# when we're the main project and BOOST_MYSQL_INTEGRATION_TESTS is on
find_package(Boost ${BOOST_MYSQL_VERSION} REQUIRED COMPONENTS context)

# Get the MySQL hostname to use for examples
if(DEFINED ENV{BOOST_MYSQL_SERVER_HOST})
    set(SERVER_HOST $ENV{BOOST_MYSQL_SERVER_HOST})
else()
    set(SERVER_HOST "localhost")
endif()

add_library(boost_mysql_examples_common INTERFACE)
target_link_libraries(
    boost_mysql_examples_common
    INTERFACE
    Boost::disable_autolinking
    Boost::mysql
    boost_mysql_asio
)

# Build and run an example
function (add_example EXAMPLE_NAME)
    set(EXECUTABLE_NAME "boost_mysql_example_${EXAMPLE_NAME}")
    add_executable(${EXECUTABLE_NAME} "${EXAMPLE_NAME}.cpp")
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE boost_mysql_examples_common)
    common_target_settings(${EXECUTABLE_NAME})

    if (BOOST_MYSQL_VALGRIND_TESTS)
        add_memcheck_test(
            NAME "${EXECUTABLE_NAME}_memcheck"
            TARGET ${EXECUTABLE_NAME}
            ARGUMENTS ${ARGN}
        )
    else()
        add_test(
            NAME ${EXECUTABLE_NAME}
            COMMAND ${EXECUTABLE_NAME} ${ARGN}
        )
    endif()
endfunction()

# Coroutines needs Boost.Context and shouldn't be memchecked
function (add_example_coroutines)
    set(EXECUTABLE_NAME boost_mysql_example_async_coroutines)
    add_executable(${EXECUTABLE_NAME} async_coroutines.cpp)
    target_link_libraries(
        ${EXECUTABLE_NAME}
        PRIVATE
        boost_mysql_examples_common
        Boost::context
    )
    common_target_settings(${EXECUTABLE_NAME})
    add_test(
        NAME ${EXECUTABLE_NAME}
        COMMAND ${EXECUTABLE_NAME} example_user example_password ${SERVER_HOST}
    )
endfunction()

# The stored procedures example must be run several times through a Python script
function (add_example_stored_procedures)
    set(EXECUTABLE_NAME boost_mysql_example_stored_procedures)
    add_executable(${EXECUTABLE_NAME} stored_procedures.cpp)
    target_link_libraries(
        ${EXECUTABLE_NAME}
        PRIVATE
        boost_mysql_examples_common
    )
    common_target_settings(${EXECUTABLE_NAME})
    add_test(
        NAME ${EXECUTABLE_NAME}
        COMMAND
            python
            ${CMAKE_CURRENT_SOURCE_DIR}/private/run_stored_procedures.py
            $<TARGET_FILE:${EXECUTABLE_NAME}>
            ${SERVER_HOST}
    )
endfunction()

# Build and run all the examples
add_example(snippets                    example_user example_password ${SERVER_HOST})
add_example(tutorial                    example_user example_password ${SERVER_HOST})
add_example(text_queries                example_user example_password ${SERVER_HOST})
add_example(prepared_statements         example_user example_password ${SERVER_HOST})
add_example(async_callbacks             example_user example_password ${SERVER_HOST})
add_example_coroutines()
add_example(async_coroutinescpp20       example_user example_password ${SERVER_HOST})
add_example(async_futures               example_user example_password ${SERVER_HOST})
add_example(default_completion_tokens   example_user example_password ${SERVER_HOST})
add_example(metadata                    example_user example_password ${SERVER_HOST})
add_example(ssl                         example_user example_password ${SERVER_HOST})
add_example(timeouts                    example_user example_password ${SERVER_HOST})
add_example(source_script               example_user example_password ${SERVER_HOST} ${CMAKE_CURRENT_SOURCE_DIR}/private/test_script.sql)
add_example_stored_procedures()
if ("$ENV{BOOST_MYSQL_NO_UNIX_SOCKET_TESTS}" STREQUAL "")
    add_example(unix_socket             example_user example_password)
endif()





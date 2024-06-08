#
# Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# Note: examples count as integration tests. This is only processed
# when BOOST_MYSQL_INTEGRATION_TESTS is on

# Get the MySQL hostname to use for examples
if(DEFINED ENV{BOOST_MYSQL_SERVER_HOST})
    set(SERVER_HOST $ENV{BOOST_MYSQL_SERVER_HOST})
else()
    set(SERVER_HOST "127.0.0.1")
endif()

add_library(boost_mysql_examples_common INTERFACE)
target_link_libraries(
    boost_mysql_examples_common
    INTERFACE
    boost_mysql_compiled
)

# Declare an example target
function(add_example_target EXAMPLE_NAME EXAMPLE_PATH)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_PATH})
    target_link_libraries(${EXAMPLE_NAME} PRIVATE boost_mysql_examples_common ${ARGN})
    boost_mysql_common_target_settings(${EXAMPLE_NAME})
endfunction()

# Run an example through a Python runner
function(run_python_example EXAMPLE_TARGET RUNNER_NAME)
    add_test(
        NAME ${EXAMPLE_TARGET}
        COMMAND
        python
        ${CMAKE_CURRENT_SOURCE_DIR}/private/${RUNNER_NAME}
        $<TARGET_FILE:${EXAMPLE_TARGET}>
        ${SERVER_HOST}
    )
endfunction()

# Build and run an example
function(add_example EXAMPLE_NAME EXAMPLE_PATH)
    add_example_target(${EXAMPLE_NAME} ${EXAMPLE_PATH})

    add_test(
        NAME ${EXAMPLE_NAME}
        COMMAND ${EXAMPLE_NAME} ${ARGN}
    )
endfunction()

# Coroutines needs Boost.Context and shouldn't be memchecked
function(add_example_coroutines EXAMPLE_NAME)
    set(EXECUTABLE_NAME boost_mysql_example_${EXAMPLE_NAME})
    add_example_target(${EXECUTABLE_NAME} ${EXAMPLE_NAME}.cpp Boost::context)
    add_test(
        NAME ${EXECUTABLE_NAME}
        COMMAND ${EXECUTABLE_NAME} example_user example_password ${SERVER_HOST}
    )
endfunction()

# The order management examples must be run several times through a Python script
function(add_example_order_management EXAMPLE_NAME EXAMPLE_PATH)
    add_example_target(${EXAMPLE_NAME} ${EXAMPLE_PATH})
    run_python_example(${EXAMPLE_NAME} run_stored_procedures.py)
endfunction()

# Regular examples are the ones that require no extra linking libs and can be run
# with example_user example_password
set(REGULAR_EXAMPLES
    tutorial
    async_callbacks
    async_coroutinescpp20
    async_futures
    metadata
    ssl
    timeouts
    pipeline
)

foreach(FILE_NAME ${REGULAR_EXAMPLES})
    add_example(
        "boost_mysql_example_${FILE_NAME}"
        "${FILE_NAME}.cpp"
        example_user example_password ${SERVER_HOST}
    )
endforeach()

# Order management examples must be run several times through a Python script
set(ORDER_EXAMPLES
    prepared_statements_cpp11
    prepared_statements_cpp14
    stored_procedures_cpp11
    stored_procedures_cpp14
)

foreach(FILE_NAME ${ORDER_EXAMPLES})
    add_example_order_management(
        "boost_mysql_example_${FILE_NAME}"
        "order_management/${FILE_NAME}.cpp"
    )
endforeach()

# Coroutine examples
add_example_coroutines(async_coroutines)
add_example_coroutines(any_connection)

# UNIX sockets
if("$ENV{BOOST_MYSQL_NO_UNIX_SOCKET_TESTS}" STREQUAL "")
    add_example(
        boost_mysql_example_unix_socket
        unix_socket.cpp
        example_user example_password
    )
endif()

# Source script
add_example(
    boost_mysql_example_source_script
    source_script.cpp
    example_user example_password ${SERVER_HOST} ${CMAKE_CURRENT_SOURCE_DIR}/private/test_script.sql
)

# Patch updates
add_example_target(boost_mysql_example_patch_updates patch_updates.cpp)
run_python_example(boost_mysql_example_patch_updates run_patch_updates.py)

# Dynamic filters
add_example_target(boost_mysql_example_dynamic_filters dynamic_filters.cpp)
target_link_libraries(boost_mysql_example_dynamic_filters PRIVATE Boost::context)
run_python_example(boost_mysql_example_dynamic_filters run_dynamic_filters.py)

# Batch inserts
add_example_target(boost_mysql_example_batch_inserts batch_inserts.cpp)
target_link_libraries(boost_mysql_example_batch_inserts PRIVATE Boost::json)
run_python_example(boost_mysql_example_batch_inserts run_batch_inserts.py)

# Batch inserts, generic version
add_example_target(boost_mysql_example_batch_inserts_generic batch_inserts_generic.cpp)
target_link_libraries(boost_mysql_example_batch_inserts_generic PRIVATE Boost::json)
run_python_example(boost_mysql_example_batch_inserts_generic run_batch_inserts.py)

# HTTP server
add_executable(
    boost_mysql_example_connection_pool
    connection_pool/repository.cpp
    connection_pool/handle_request.cpp
    connection_pool/server.cpp
    connection_pool/main.cpp
)
target_link_libraries(
    boost_mysql_example_connection_pool
    PRIVATE
    boost_mysql_examples_common
    Boost::context
    Boost::json
    Boost::url
    Boost::beast
)
boost_mysql_common_target_settings(boost_mysql_example_connection_pool)
run_python_example(boost_mysql_example_connection_pool run_connection_pool.py)

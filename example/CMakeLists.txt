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
    target_link_libraries(${EXAMPLE_NAME} PRIVATE boost_mysql_examples_common)
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

function(add_example EXAMPLE_NAME EXAMPLE_PATH)
    set(MULTI_VALUE_ARGS LIBS ARGS)
    cmake_parse_arguments(ADD_EXAMPLE "" "" "${MULTI_VALUE_ARGS}" ${ARGN})

    set(TARGET_NAME "boost_mysql_example_${EXAMPLE_NAME}")
    add_example_target(${TARGET_NAME} ${EXAMPLE_PATH})
    target_link_libraries(${TARGET_NAME} PRIVATE ${ADD_EXAMPLE_LIBS})
    add_test(
        NAME ${TARGET_NAME}
        COMMAND ${TARGET_NAME} ${ADD_EXAMPLE_ARGS}
    )
endfunction()

function(add_simple_example EXAMPLE_NAME)
    add_example(${EXAMPLE_NAME} "2_simple/${EXAMPLE_NAME}.cpp" ${ARGN})
endfunction()

# The order management examples must be run several times through a Python script
# function(add_example_order_management EXAMPLE_NAME EXAMPLE_PATH)
#     add_example_target(${EXAMPLE_NAME} ${EXAMPLE_PATH})
#     run_python_example(${EXAMPLE_NAME} run_stored_procedures.py)
# endfunction()

set(REGULAR_ARGS example_user example_password ${SERVER_HOST})

# Tutorials
add_example(tutorial_sync             1_tutorial/1_sync.cpp             ARGS ${REGULAR_ARGS})
add_example(tutorial_async            1_tutorial/2_async.cpp            ARGS ${REGULAR_ARGS})
add_example(tutorial_with_params      1_tutorial/3_with_params.cpp      ARGS ${REGULAR_ARGS} 1)
add_example(tutorial_static_interface 1_tutorial/4_static_interface.cpp ARGS ${REGULAR_ARGS} 1 LIBS Boost::pfr)

# Simple
add_simple_example(callbacks                    ARGS ${REGULAR_ARGS})
add_simple_example(coroutines_cpp11             ARGS ${REGULAR_ARGS} LIBS Boost::context)
add_simple_example(batch_inserts                ARGS ${REGULAR_ARGS} LIBS Boost::json)
add_simple_example(batch_inserts_generic        ARGS ${REGULAR_ARGS} LIBS Boost::json)
add_simple_example(patch_updates                ARGS ${REGULAR_ARGS})
add_simple_example(dynamic_filters              ARGS ${REGULAR_ARGS})
add_simple_example(unix_socket                  ARGS example_user example_password)
add_simple_example(disable_tls                  ARGS ${REGULAR_ARGS})
add_simple_example(tls_certificate_verification ARGS ${REGULAR_ARGS})
add_simple_example(metadata                     ARGS ${REGULAR_ARGS})
add_simple_example(prepared_statements          ARGS ${REGULAR_ARGS} "HGS")
add_simple_example(timeouts                     ARGS ${REGULAR_ARGS} "HGS")
add_simple_example(pipeline                     ARGS ${REGULAR_ARGS} "HGS")
add_simple_example(multi_function               ARGS ${REGULAR_ARGS})
add_simple_example(transactions                 ARGS ${REGULAR_ARGS} 1 1)



# # Regular examples are the ones that require no extra linking libs and can be run
# # with example_user example_password
# set(REGULAR_EXAMPLES
#     tutorial
#     tutorial_async
#     simple/callbacks
#     simple/
#     async_callbacks
#     async_coroutinescpp20
#     async_futures
#     metadata
#     ssl
#     timeouts
#     pipeline
# )

# foreach(FILE_NAME ${REGULAR_EXAMPLES})
#     add_example(
#         "boost_mysql_example_${FILE_NAME}"
#         "${FILE_NAME}.cpp"
#         example_user example_password ${SERVER_HOST}
#     )
# endforeach()

# # Order management examples must be run several times through a Python script
# set(ORDER_EXAMPLES
#     prepared_statements_cpp11
#     prepared_statements_cpp14
#     stored_procedures_cpp11
#     stored_procedures_cpp14
# )

# foreach(FILE_NAME ${ORDER_EXAMPLES})
#     add_example_order_management(
#         "boost_mysql_example_${FILE_NAME}"
#         "order_management/${FILE_NAME}.cpp"
#     )
# endforeach()

# # Coroutine examples
# add_example_coroutines(async_coroutines)
# add_example_coroutines(any_connection)

# # UNIX sockets. Don't run the example on Windows machines
# if (NOT WIN32)
#     add_example(
#         boost_mysql_example_unix_socket
#         unix_socket.cpp
#         example_user example_password
#     )
# endif()

# # Source script
# add_example(
#     boost_mysql_example_source_script
#     source_script.cpp
#     example_user example_password ${SERVER_HOST} ${CMAKE_CURRENT_SOURCE_DIR}/private/test_script.sql
# )

# # Patch updates
# add_example_target(boost_mysql_example_patch_updates patch_updates.cpp)
# run_python_example(boost_mysql_example_patch_updates run_patch_updates.py)

# # Dynamic filters
# add_example_target(boost_mysql_example_dynamic_filters dynamic_filters.cpp)
# target_link_libraries(boost_mysql_example_dynamic_filters PRIVATE Boost::context)
# run_python_example(boost_mysql_example_dynamic_filters run_dynamic_filters.py)

# # Batch inserts
# add_example_target(boost_mysql_example_batch_inserts batch_inserts.cpp)
# target_link_libraries(boost_mysql_example_batch_inserts PRIVATE Boost::json)
# run_python_example(boost_mysql_example_batch_inserts run_batch_inserts.py)

# # Batch inserts, generic version
# add_example_target(boost_mysql_example_batch_inserts_generic batch_inserts_generic.cpp)
# target_link_libraries(boost_mysql_example_batch_inserts_generic PRIVATE Boost::json)
# run_python_example(boost_mysql_example_batch_inserts_generic run_batch_inserts.py)

# # HTTP server
# add_executable(
#     boost_mysql_example_connection_pool
#     connection_pool/repository.cpp
#     connection_pool/handle_request.cpp
#     connection_pool/server.cpp
#     connection_pool/main.cpp
# )
# target_link_libraries(
#     boost_mysql_example_connection_pool
#     PRIVATE
#     boost_mysql_examples_common
#     Boost::context
#     Boost::json
#     Boost::url
#     Boost::beast
# )
# boost_mysql_common_target_settings(boost_mysql_example_connection_pool)
# run_python_example(boost_mysql_example_connection_pool run_connection_pool.py)

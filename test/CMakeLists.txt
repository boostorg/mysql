#
# Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# GTest/GMock
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        release-1.10.0
)

FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE) # Avoid GTest linking against wrong CRT
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif()

# Unit testing. We do not use gtest_discover_tests because
# of the runtime penalty (specially considerable in integration tests)
add_executable(
    boost_mysql_unittests
    common/test_main.cpp
    unit/detail/auxiliar/static_string.cpp
    unit/detail/auth/auth_calculator.cpp
    unit/detail/protocol/serialization_test_common.cpp
    unit/detail/protocol/serialization.cpp
    unit/detail/protocol/common_messages.cpp
    unit/detail/protocol/handshake_messages.cpp
    unit/detail/protocol/query_messages.cpp
    unit/detail/protocol/prepared_statement_messages.cpp
    unit/detail/protocol/capabilities.cpp
    unit/detail/protocol/text_deserialization_value.cpp
    unit/detail/protocol/text_deserialization_error.cpp
    unit/detail/protocol/binary_deserialization_value.cpp
    unit/detail/protocol/binary_deserialization_error.cpp
    unit/detail/protocol/row_deserialization.cpp
    unit/detail/protocol/binary_serialization.cpp
    unit/detail/protocol/null_bitmap_traits.cpp
    unit/detail/protocol/date.cpp
    unit/detail/protocol/channel.cpp
    unit/metadata.cpp
    unit/value.cpp
    unit/row.cpp
    unit/error.cpp
    unit/prepared_statement.cpp
    unit/resultset.cpp
    unit/connection.cpp
)
target_include_directories(
    boost_mysql_unittests
    PRIVATE
    common
)
target_link_libraries(
    boost_mysql_unittests
    PRIVATE
    gtest
    gmock
    Boost::mysql
)
common_target_settings(boost_mysql_unittests)

if (BOOST_MYSQL_VALGRIND_TESTS)
    add_memcheck_test(
        NAME boost_mysql_unittests_memcheck
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/boost_mysql_unittests
    )
else()
    add_test(
        NAME boost_mysql_unittests
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/boost_mysql_unittests
    )
endif()

# Integration testing
find_package(Boost REQUIRED COMPONENTS coroutine)

add_executable(
    boost_mysql_integrationtests
    common/test_main.cpp
    integration/network_functions/network_functions_impl.cpp
    integration/network_functions/sync.cpp
    integration/network_functions/async_callback.cpp
    integration/network_functions/async_coroutine.cpp
    integration/network_functions/async_coroutinecpp20.cpp
    integration/network_functions/async_future.cpp
    integration/metadata_validator.cpp
    integration/connection.cpp
    integration/connect.cpp
    integration/handshake.cpp
    integration/query.cpp
    integration/prepare_statement.cpp
    integration/execute_statement.cpp
    integration/close_statement.cpp
    integration/resultset.cpp
    integration/prepared_statement_lifecycle.cpp
    integration/database_types.cpp
    integration/quit_connection.cpp
    integration/close_connection.cpp
)
target_link_libraries(
    boost_mysql_integrationtests
    PRIVATE
    gtest
    gmock
    Boost::mysql
    Boost::coroutine
)
target_include_directories(
    boost_mysql_integrationtests
    PRIVATE
    common
)
common_target_settings(boost_mysql_integrationtests)

# Regular integration tests
add_test(
    NAME boost_mysql_integrationtests
    COMMAND 
        ${CMAKE_CURRENT_BINARY_DIR}/boost_mysql_integrationtests
        "--gtest_filter=-*RequiresSha256*" # Exclude anything using SHA256
)

# If we are using memcheck, then run a subset of the integration tests
# under valgrind. Coroutine tests don't work well under Valgrind, and
# SSL tests are too slow. We do some other exclusions to reduce runtime
if (BOOST_MYSQL_VALGRIND_TESTS)
    set(GTEST_FILTER "--gtest_filter=-*RequiresSha256*:*sslrequire*:*coroutine*:*UNIX*:*noerrinfo*:*TCPDefaultToken*")
    add_memcheck_test(
        NAME boost_mysql_integrationtests_memcheck
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/boost_mysql_integrationtests ${GTEST_FILTER}
    )
endif()

# SHA256 tests 
if (BOOST_MYSQL_SHA256_TESTS)
    # Actual tests
    add_test(
        NAME boost_mysql_integrationtests_sha256
        COMMAND 
            ${CMAKE_CURRENT_BINARY_DIR}/boost_mysql_integrationtests
            "--gtest_filter=*RequiresSha256*" # Run only SHA256 stuff
    )
endif()

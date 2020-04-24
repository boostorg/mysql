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
    mysql_unittests
    unit/detail/auxiliar/static_string.cpp
    unit/detail/auth/auth_calculator.cpp
    unit/detail/protocol/serialization_test_common.cpp
    unit/detail/protocol/serialization.cpp
    unit/detail/protocol/common_messages.cpp
    unit/detail/protocol/handshake_messages.cpp
    unit/detail/protocol/query_messages.cpp
    unit/detail/protocol/prepared_statement_messages.cpp
    unit/detail/protocol/capabilities.cpp
    unit/detail/protocol/text_deserialization.cpp
    unit/detail/protocol/binary_deserialization.cpp
    unit/detail/protocol/null_bitmap_traits.cpp
    unit/metadata.cpp
    unit/value.cpp
    unit/row.cpp
    unit/error.cpp
    unit/prepared_statement.cpp
)
# A codegen issue in MSVC C++17 makes gmock expectations not work
if (NOT MSVC)
    target_sources(mysql_unittests PRIVATE unit/detail/protocol/channel.cpp)
endif()

target_include_directories(
    mysql_unittests
    PRIVATE
    common
)
target_link_libraries(
    mysql_unittests
    PRIVATE
    gtest
    gtest_main
    gmock
    Boost_mysql
)
_mysql_common_target_settings(mysql_unittests)

if (BOOST_MYSQL_VALGRIND_TESTS)
    Mysqlvalgrind_AddMemcheckTest(
        NAME mysql_unittests_memcheck
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/mysql_unittests
    )
else()
    add_test(
        NAME mysql_unittests
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/mysql_unittests
    )
endif()

# Integration testing
find_package(Boost REQUIRED COMPONENTS coroutine)

add_executable(
    mysql_integrationtests
    integration/metadata_validator.cpp
    integration/network_functions.cpp
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
    mysql_integrationtests
    PRIVATE
    gtest
    gtest_main
    gmock
    Boost_mysql
    Boost::coroutine
)
target_include_directories(
    mysql_integrationtests
    PRIVATE
    common
)
_mysql_common_target_settings(mysql_integrationtests)

# Integration test setup
_mysql_sql_setup_fixture(
    TEST_NAME    mysql_integrationtests_setup
    FIXTURE_NAME mysql_integrationtests_fixture
    SQL_FILE     ${CMAKE_CURRENT_SOURCE_DIR}/integration/db_setup.sql
    SKIP_VAR     BOOST_MYSQL_SKIP_DB_SETUP
)

# Regular integration tests
add_test(
    NAME mysql_integrationtests
    COMMAND 
        ${CMAKE_CURRENT_BINARY_DIR}/mysql_integrationtests
        "--gtest_filter=-*RequiresSha256*" # Exclude anything using SHA256
)
set_tests_properties(
    mysql_integrationtests
    PROPERTIES FIXTURES_REQUIRED mysql_integrationtests_setup
)

# If we are using memcheck, then run a subset of the integration tests
# under valgrind. Coroutine tests don't work well under Valgrind, and
# SSL tests are too slow
if (BOOST_MYSQL_VALGRIND_TESTS)
    if (BOOST_MYSQL_SHA256_TESTS)
        set(GTEST_FILTER "--gtest_filter=-*sslrequire*:*coroutine*")
    else()
        set(GTEST_FILTER "--gtest_filter=-*RequiresSha256*:*sslrequire*:*coroutine*")
    endif()
    
    add_test(
        NAME mysql_integrationtests_memcheck
        COMMAND 
            ${CMAKE_CURRENT_BINARY_DIR}/mysql_integrationtests
            ${GTEST_FILTER}
    )
    set_tests_properties(
        mysql_integrationtests_memcheck
        PROPERTIES FIXTURES_REQUIRED mysql_integrationtests_setup
    )
endif()

# SHA256 tests 
if (BOOST_MYSQL_SHA256_TESTS)
    # Setup
    _mysql_sql_setup_fixture(
        TEST_NAME    mysql_integrationtests_sha256_setup
        FIXTURE_NAME mysql_integrationtests_sha256_fixture
        SQL_FILE     ${CMAKE_CURRENT_SOURCE_DIR}/integration/db_setup_sha256.sql
        SKIP_VAR     BOOST_MYSQL_SKIP_DB_SETUP
    )
    
    # Actual tests
    add_test(
        NAME mysql_integrationtests_sha256
        COMMAND 
            ${CMAKE_CURRENT_BINARY_DIR}/mysql_integrationtests
            "--gtest_filter=*RequiresSha256*" # Run only SHA256 stuff
    )
    set_tests_properties(
        mysql_integrationtests_sha256
        PROPERTIES FIXTURES_REQUIRED 
        "mysql_integrationtests_fixture;mysql_integrationtests_sha256_fixture"
    )
endif()

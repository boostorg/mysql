#
# Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

find_package(Boost 1.72 REQUIRED COMPONENTS
    unit_test_framework
    coroutine
)

# Test common
add_library(boost_mysql_testing INTERFACE)
target_include_directories(
    boost_mysql_testing
    INTERFACE
    common
)
target_link_libraries(
    boost_mysql_testing
    INTERFACE
    Boost::unit_test_framework
    Boost::mysql
    Boost::disable_autolinking
    Boost::dynamic_linking
)

# Unit testing
add_executable(
    boost_mysql_unittests
    unit/detail/auth/auth_calculator.cpp
    unit/detail/auxiliar/static_string.cpp
    unit/detail/auxiliar/value_type_traits.cpp
    unit/detail/protocol/capabilities.cpp
    unit/detail/protocol/date.cpp
    unit/detail/protocol/null_bitmap_traits.cpp
    unit/detail/protocol/serialization_test.cpp
    unit/detail/protocol/text_deserialization_value.cpp
    unit/detail/protocol/text_deserialization_error.cpp
    unit/detail/protocol/binary_deserialization_value.cpp
    unit/detail/protocol/binary_deserialization_error.cpp
    unit/detail/protocol/row_deserialization.cpp
    unit/metadata.cpp
    unit/value.cpp
    unit/value_constexpr.cpp
    unit/row.cpp
    unit/error.cpp
    unit/execute_params.cpp
    unit/prepared_statement.cpp
    unit/resultset.cpp
    unit/connection.cpp
    unit/entry_point.cpp
)
target_link_libraries(
    boost_mysql_unittests
    PRIVATE
    boost_mysql_testing
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
add_executable(
    boost_mysql_integrationtests
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
    integration/quit_connection.cpp
    integration/close_connection.cpp
    integration/database_types.cpp
    integration/entry_point.cpp
)
target_link_libraries(
    boost_mysql_integrationtests
    PRIVATE
    boost_mysql_testing
    Boost::coroutine
)
common_target_settings(boost_mysql_integrationtests)

if ("$ENV{BOOST_MYSQL_TEST_FILTER}" STREQUAL "")
    add_test(
        NAME boost_mysql_integrationtests
        COMMAND boost_mysql_integrationtests
    )
else()
    add_test(
        NAME boost_mysql_integrationtests
        COMMAND boost_mysql_integrationtests "-t" $ENV{BOOST_MYSQL_TEST_FILTER}
    )
endif()

# If we are using memcheck, then run a subset of the integration tests
# under valgrind. Coroutine tests don't work well under Valgrind, and
# SSL tests are too slow. We do some other exclusions to reduce runtime
if (BOOST_MYSQL_VALGRIND_TESTS)
    set(TEST_FILTER "!@sha256:!@sslrequire:!@async_coroutine_errinfo:!@async_coroutine_noerrinfo:!@unix:!@tcp_default_token:!@unix")
    add_memcheck_test(
        NAME boost_mysql_integrationtests_memcheck
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/boost_mysql_integrationtests "-t" ${TEST_FILTER}
    )
endif()

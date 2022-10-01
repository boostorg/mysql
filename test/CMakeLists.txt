#
# Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
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
)

# Unit testing
add_executable(
    boost_mysql_unittests
    unit/detail/channel/read_buffer.cpp
    unit/detail/channel/message_parser.cpp
    unit/detail/channel/message_reader.cpp
    unit/detail/channel/message_writer_processor.cpp
    unit/detail/channel/message_writer.cpp
    unit/detail/auth/auth_calculator.cpp
    unit/detail/auxiliar/static_string.cpp
    # unit/detail/auxiliar/value_type_traits.cpp
    # unit/detail/protocol/capabilities.cpp
    # unit/detail/protocol/date.cpp
    # unit/detail/protocol/null_bitmap_traits.cpp
    # unit/detail/protocol/serialization_test.cpp
    # unit/detail/protocol/text_deserialization_value.cpp
    # unit/detail/protocol/text_deserialization_error.cpp
    # unit/detail/protocol/binary_deserialization_value.cpp
    # unit/detail/protocol/binary_deserialization_error.cpp
    # unit/detail/protocol/row_deserialization.cpp

    unit/field_view.cpp
    unit/field.cpp
    unit/row_view.cpp
    unit/row.cpp

    # unit/metadata.cpp
    # unit/error.cpp
    # unit/execute_params.cpp
    # unit/prepared_statement.cpp
    # unit/resultset.cpp
    # unit/connection.cpp
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
if(BOOST_MYSQL_INTEGRATION_TESTS)
    add_executable(
        boost_mysql_integrationtests

        # Utilities
        integration/utils/src/get_endpoint.cpp
        integration/utils/src/metadata_validator.cpp
        integration/utils/src/network_result.cpp
        integration/utils/src/er_network_variant.cpp
        integration/utils/src/sync_errc.cpp
        integration/utils/src/sync_exc.cpp
        integration/utils/src/async_callback.cpp
        integration/utils/src/async_callback_noerrinfo.cpp
        integration/utils/src/async_future.cpp
        integration/utils/src/async_coroutine.cpp
        integration/utils/src/async_coroutinecpp20.cpp
        integration/utils/src/default_completion_tokens.cpp

        # Actual tests
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
        integration/reconnect.cpp
        integration/database_types.cpp
        integration/entry_point.cpp
    )
    target_include_directories(
        boost_mysql_integrationtests
        PRIVATE
        integration/utils/include
    )
    target_link_libraries(
        boost_mysql_integrationtests
        PRIVATE
        boost_mysql_testing
        Boost::coroutine
    )
    common_target_settings(boost_mysql_integrationtests)

    # Compose the test filter
    if (NOT "$ENV{BOOST_MYSQL_NO_UNIX_SOCKET_TESTS}" STREQUAL "")
        list(APPEND TEST_EXCLUSIONS "!@unix")
    endif()
    if (NOT "$ENV{BOOST_MYSQL_NO_SHA256_TESTS}" STREQUAL "")
        list(APPEND TEST_EXCLUSIONS "!@sha256")
    endif()
    string(JOIN ":" TEST_FILTER ${TEST_EXCLUSIONS})

    if ("${TEST_FILTER}" STREQUAL "")
        add_test(
            NAME boost_mysql_integrationtests
            COMMAND boost_mysql_integrationtests
        )
    else()
        add_test(
            NAME boost_mysql_integrationtests
            COMMAND boost_mysql_integrationtests "-t" ${TEST_FILTER}
        )
    endif()

    # If we are using memcheck, then run a subset of the integration tests
    # under valgrind. Coroutine tests don't work well under Valgrind, and
    # SSL tests are too slow. We do some other exclusions to reduce runtime
    if (BOOST_MYSQL_VALGRIND_TESTS)
        string(JOIN ":" TEST_FILTER ${TEST_EXCLUSIONS} "!@ssl" "!@async_coroutine")
        add_memcheck_test(
            NAME boost_mysql_integrationtests_memcheck
            COMMAND ${CMAKE_CURRENT_BINARY_DIR}/boost_mysql_integrationtests "-t" ${TEST_FILTER}
        )
    endif()
endif()

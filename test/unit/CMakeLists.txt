#
# Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# TODO: this is not gonna work when we build as a shared lib as it requires access to internal functions
add_executable(
    boost_mysql_unittests

    # Helpers
    src/test_stream.cpp
    src/serialization.cpp

    # Actual tests
    test/auth/auth.cpp
    test/protocol/static_buffer.cpp
    test/protocol/capabilities.cpp
    test/protocol/null_bitmap_traits.cpp
    test/protocol/protocol_field_type.cpp
    test/protocol/serialization.cpp
    test/protocol/binary_serialization.cpp
    test/protocol/deserialize_text_field.cpp
    test/protocol/deserialize_binary_field.cpp
    test/protocol/protocol.cpp
    test/channel/read_buffer.cpp
    test/channel/message_parser.cpp
    test/channel/message_reader.cpp
    test/channel/message_writer.cpp
    test/channel/write_message.cpp
    test/execution_processor/execution_processor.cpp
    test/execution_processor/execution_state_impl.cpp
    test/execution_processor/static_execution_state_impl.cpp
    test/execution_processor/results_impl.cpp
    test/execution_processor/static_results_impl.cpp
    test/network_algorithms/read_resultset_head.cpp
    test/network_algorithms/start_execution.cpp
    test/network_algorithms/read_some_rows.cpp
    test/network_algorithms/read_some_rows_dynamic.cpp
    test/network_algorithms/execute.cpp
    test/network_algorithms/close_statement.cpp
    test/network_algorithms/ping.cpp
    test/network_algorithms/read_some_rows_static.cpp
    test/connection.cpp
    test/misc.cpp
    test/multifn.cpp
    # TODO: these should be in unit_iface
    test/execution_state.cpp
    test/static_execution_state.cpp
    test/results.cpp
    test/static_results.cpp
    test/resultset_view.cpp
    test/resultset.cpp
)
target_include_directories(
    boost_mysql_unittests
    PRIVATE
    "include"
)
target_link_libraries(
    boost_mysql_unittests
    PRIVATE
    boost_mysql_testing
    boost_mysql_impl_private
    boost_mysql_impl_public
)
if (${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.16)
    target_precompile_headers(
        boost_mysql_unittests
        PRIVATE
        pch.hpp
    )
endif()
common_target_settings(boost_mysql_unittests)

if (BOOST_MYSQL_VALGRIND_TESTS)
    add_memcheck_test(
        NAME boost_mysql_unittests_memcheck
        TARGET boost_mysql_unittests
    )
else()
    add_test(
        NAME boost_mysql_unittests
        COMMAND boost_mysql_unittests
    )
    add_dependencies(tests boost_mysql_unittests)
endif()

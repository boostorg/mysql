#
# Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# TODO: this is not gonna work when we build as a shared lib as it requires access to internal functions
add_executable(
    boost_mysql_unittests
    src/test_stream.cpp
    src/serialization.cpp
    auth/auth.cpp
    protocol/static_buffer.cpp
    protocol/capabilities.cpp
    protocol/null_bitmap_traits.cpp
    protocol/protocol_field_type.cpp
    protocol/serialization.cpp
    protocol/binary_serialization.cpp
    protocol/deserialize_text_field.cpp
    protocol/deserialize_binary_field.cpp
    protocol/protocol.cpp
    channel/read_buffer.cpp
    channel/message_parser.cpp
    channel/message_reader.cpp
    channel/message_writer.cpp
    channel/write_message.cpp
    execution_processor/execution_processor.cpp
    execution_processor/execution_state_impl.cpp
    execution_processor/static_execution_state_impl.cpp
    execution_processor/results_impl.cpp
    execution_processor/static_results_impl.cpp
    network_algorithms/read_resultset_head.cpp
    network_algorithms/start_execution.cpp
    network_algorithms/read_some_rows.cpp
    network_algorithms/read_some_rows_dynamic.cpp
    network_algorithms/execute.cpp
    network_algorithms/close_statement.cpp
    network_algorithms/ping.cpp
    network_algorithms/read_some_rows_static.cpp
    interface/connection.cpp
    interface/misc.cpp
    interface/multifn.cpp
    # TODO: these should be in unit_iface
    interface/execution_state.cpp
    interface/static_execution_state.cpp
    interface/results.cpp
    interface/static_results.cpp
    interface/resultset_view.cpp
    interface/resultset.cpp
    entry_point.cpp
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
set_source_files_properties(
    entry_point.cpp
    PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON
)
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

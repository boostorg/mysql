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
    # detail/channel/message_reader.cpp
    channel/message_writer_processor.cpp
    channel/message_writer.cpp
    # detail/auxiliar/rows_iterator.cpp
    # detail/auxiliar/execution_request.cpp
    # detail/auxiliar/datetime.cpp
    # detail/auxiliar/row_impl.cpp
    # detail/execution_processor/execution_processor.cpp
    # detail/execution_processor/execution_state_impl.cpp
    # detail/execution_processor/static_execution_state_impl.cpp
    # detail/execution_processor/results_impl.cpp
    # detail/execution_processor/static_results_impl.cpp
    # detail/execution_processor/concepts.cpp
    # detail/network_algorithms/read_resultset_head.cpp
    # detail/network_algorithms/start_execution_impl.cpp
    # detail/network_algorithms/read_some_rows_dynamic.cpp
    # detail/network_algorithms/read_some_rows_static.cpp
    # detail/network_algorithms/read_some_rows_impl.cpp
    # detail/network_algorithms/execute_impl.cpp
    # detail/network_algorithms/high_level_execution.cpp
    # detail/network_algorithms/close_statement.cpp
    # detail/network_algorithms/ping.cpp
    # detail/typing/meta_check_context.cpp
    # detail/typing/pos_map.cpp
    # detail/typing/readable_field_traits.cpp
    # detail/typing/writable_field_traits.cpp
    # detail/typing/row_traits.cpp
    # detail/typing/get_type_index.cpp
    # date.cpp
    # datetime.cpp
    # field_view.cpp
    # field.cpp
    # row_view.cpp
    # row.cpp
    # rows_view.cpp
    # rows.cpp
    # metadata.cpp
    # client_errc.cpp
    # common_server_errc.cpp
    # mysql_server_errc.cpp
    # mariadb_server_errc.cpp
    # diagnostics.cpp
    # statement.cpp
    # execution_state.cpp
    # static_execution_state.cpp
    # results.cpp
    # static_results.cpp
    # resultset_view.cpp
    # resultset.cpp
    # connection.cpp
    # throw_on_error.cpp
    # misc.cpp
    # multifn.cpp
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

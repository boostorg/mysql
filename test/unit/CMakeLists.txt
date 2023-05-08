#
# Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

add_executable(
    boost_mysql_unittests
    detail/channel/read_buffer.cpp
    detail/channel/message_parser.cpp
    detail/channel/message_reader.cpp
    detail/channel/message_writer_processor.cpp
    detail/channel/message_writer.cpp
    detail/auth/auth_calculator.cpp
    detail/auxiliar/static_string.cpp
    detail/auxiliar/rows_iterator.cpp
    detail/auxiliar/execution_request.cpp
    detail/auxiliar/datetime.cpp
    detail/auxiliar/row_impl.cpp
    detail/protocol/capabilities.cpp
    detail/protocol/null_bitmap_traits.cpp
    detail/protocol/deserialize_errc.cpp
    detail/protocol/serialization_test.cpp
    detail/protocol/deserialize_text_field.cpp
    detail/protocol/deserialize_binary_field.cpp
    detail/protocol/deserialize_row.cpp
    detail/protocol/deserialize_execution_messages.cpp
    detail/protocol/process_error_packet.cpp
    detail/execution_processor/execution_processor.cpp
    detail/execution_processor/execution_state_impl.cpp
    detail/execution_processor/static_execution_state_impl.cpp
    detail/execution_processor/results_impl.cpp
    detail/execution_processor/static_results_impl.cpp
    detail/network_algorithms/process_row_message.cpp
    detail/network_algorithms/read_resultset_head.cpp
    detail/network_algorithms/start_execution_impl.cpp
    detail/network_algorithms/read_some_rows.cpp
    # detail/network_algorithms/execute_impl.cpp
    # detail/network_algorithms/high_level_execution.cpp
    detail/network_algorithms/close_statement.cpp
    detail/network_algorithms/ping.cpp
    detail/typing/meta_check_context.cpp
    detail/typing/cpp2db_map.cpp
    detail/typing/readable_field_traits.cpp
    detail/typing/writable_field_traits.cpp
    detail/typing/row_traits.cpp
    detail/typing/get_type_index.cpp
    date.cpp
    datetime.cpp
    field_view.cpp
    field.cpp
    row_view.cpp
    row.cpp
    rows_view.cpp
    rows.cpp
    metadata.cpp
    client_errc.cpp
    common_server_errc.cpp
    mysql_server_errc.cpp
    mariadb_server_errc.cpp
    diagnostics.cpp
    statement.cpp
    # execution_state.cpp
    # results.cpp
    # resultset_view.cpp
    # resultset.cpp
    connection.cpp
    throw_on_error.cpp
    regressions.cpp
    # multifn.cpp
    entry_point.cpp
)
target_compile_options(boost_mysql_unittests PRIVATE -fsanitize=address)
target_link_options(boost_mysql_unittests PRIVATE -fsanitize=address)
target_link_libraries(
    boost_mysql_unittests
    PRIVATE
    boost_mysql_testing
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

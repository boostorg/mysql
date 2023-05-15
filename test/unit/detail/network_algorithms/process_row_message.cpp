//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/column_type.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/execution_processor/execution_state_impl.hpp>
#include <boost/mysql/detail/network_algorithms/process_row_message.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/deserialization_context.hpp>

#include <boost/test/unit_test.hpp>

#include "creation/create_execution_state.hpp"
#include "creation/create_message.hpp"
#include "creation/create_meta.hpp"
#include "creation/create_row_message.hpp"
#include "mock_execution_processor.hpp"
#include "printing.hpp"
#include "test_channel.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
using boost::mysql::detail::deserialization_context;
using boost::mysql::detail::output_ref;
using boost::mysql::detail::process_row_message;

namespace {

BOOST_AUTO_TEST_SUITE(test_process_row_message)

BOOST_AUTO_TEST_CASE(row_success)
{
    // Setup
    auto st = exec_builder().meta({meta_builder().type(column_type::int_).build()}).build();
    diagnostics diag;

    // Channel, with a message waiting to be retrieved
    auto chan = create_channel(create_text_row_message(0, 42));
    read_some_and_check(chan);

    // Process the message
    auto err = process_row_message(chan, st, diag, output_ref());
    throw_on_error(err, diag);
    BOOST_TEST(chan.shared_fields() == make_fv_vector(42));
}

BOOST_AUTO_TEST_CASE(row_error)
{
    diagnostics diag;

    // Simulate an error when reading the row
    mock_execution_processor p;
    add_meta(p, {meta_builder().build()});
    p.actions.on_row = [](deserialization_context, const output_ref&) {
        return error_code(client_errc::static_row_parsing_error);
    };

    // Channel with message
    auto chan = create_channel(create_text_row_message(0, 42));
    read_some_and_check(chan);

    // Process the message
    auto err = process_row_message(chan, p, diag, output_ref());
    BOOST_TEST(err == client_errc::static_row_parsing_error);
    BOOST_TEST(p.num_calls.on_row == 1u);
    BOOST_TEST(p.num_calls.on_row_ok_packet == 0u);
}

BOOST_AUTO_TEST_CASE(eof_success)
{
    // Setup
    auto st = exec_builder().meta({meta_builder().build()}).build();
    diagnostics diag;

    // Channel, with a message waiting to be retrieved
    auto chan = create_channel(ok_msg_builder().affected_rows(42).build_eof());
    read_some_and_check(chan);

    // Process the message
    auto err = process_row_message(chan, st, diag, output_ref());
    throw_on_error(err, diag);
    BOOST_TEST(st.get_affected_rows() == 42u);
}

BOOST_AUTO_TEST_CASE(eof_error)
{
    diagnostics diag;

    // Simulate an error when reading the row
    mock_execution_processor p;
    add_meta(p, {meta_builder().build()});
    p.actions.on_row_ok_packet = [](const detail::ok_packet&) {
        return error_code(client_errc::metadata_check_failed);
    };

    // Channel with message
    auto chan = create_channel(ok_msg_builder().affected_rows(42).build_eof());
    read_some_and_check(chan);

    // Process the message
    auto err = process_row_message(chan, p, diag, output_ref());
    BOOST_TEST(err == client_errc::metadata_check_failed);
    BOOST_TEST(p.num_calls.on_row == 0u);
    BOOST_TEST(p.num_calls.on_row_ok_packet == 1u);
}

BOOST_AUTO_TEST_CASE(errorpack)
{
    diagnostics diag;
    mock_execution_processor p;

    // Channel with message
    auto chan = create_channel(create_err_packet_message(0, common_server_errc::er_auto_convert));
    read_some_and_check(chan);

    // Process the message
    auto err = process_row_message(chan, p, diag, output_ref());
    BOOST_TEST(err == common_server_errc::er_auto_convert);
    BOOST_TEST(p.num_calls.on_row == 0u);
    BOOST_TEST(p.num_calls.on_row_ok_packet == 0u);
}

BOOST_AUTO_TEST_CASE(seqnum_mismatch)
{
    diagnostics diag;
    mock_execution_processor p;

    // Channel with message
    auto chan = create_channel(ok_msg_builder().seqnum(42).build_eof());
    read_some_and_check(chan);

    // Process the message
    auto err = process_row_message(chan, p, diag, output_ref());
    BOOST_TEST(err == client_errc::sequence_number_mismatch);
    BOOST_TEST(p.num_calls.on_row == 0u);
    BOOST_TEST(p.num_calls.on_row_ok_packet == 0u);
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
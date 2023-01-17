//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/row_view.hpp>

#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/asio/buffer.hpp>
#include <boost/test/unit_test.hpp>

#include "create_execution_state.hpp"
#include "create_message.hpp"
#include "netfun_maker.hpp"
#include "test_channel.hpp"
#include "test_common.hpp"
#include "test_connection.hpp"

using boost::mysql::client_errc;
using boost::mysql::error_code;
using boost::mysql::execution_state;
using boost::mysql::row_view;
using boost::mysql::detail::protocol_field_type;
using boost::mysql::detail::resultset_encoding;
using namespace boost::mysql::test;

namespace {

using netfun_maker = netfun_maker_mem<row_view, test_connection, execution_state&>;

struct
{
    typename netfun_maker::signature read_one_row;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&test_connection::read_one_row),             "sync_errc"    },
    {netfun_maker::sync_exc(&test_connection::read_one_row),              "sync_exc"     },
    {netfun_maker::async_errinfo(&test_connection::async_read_one_row),   "async_errinfo"},
    {netfun_maker::async_noerrinfo(&test_connection::async_read_one_row), "async_errinfo"}
};

BOOST_AUTO_TEST_SUITE(test_read_one_row)

BOOST_AUTO_TEST_CASE(success)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto row1 = create_message(4, {0x00, 0x00, 0x03, 0x6d, 0x69, 0x6e, 0x6d, 0x07});
            auto row2 = create_message(5, {0x00, 0x08, 0x03, 0x6d, 0x61, 0x78});
            auto ok_packet = create_ok_packet_message(6, 1, 6, 0, 9, "ab");
            auto st = create_execution_state(
                resultset_encoding::binary,
                {protocol_field_type::var_string, protocol_field_type::short_},
                4  // seqnum
            );
            test_connection conn;
            conn.stream().add_message(concat_copy(row1, row2, ok_packet));
            conn.get_channel().shared_fields().emplace_back("abc");  // from previous call

            // 1st row
            row_view rv = fns.read_one_row(conn, st).get();
            BOOST_TEST(rv == makerow("min", 1901));
            BOOST_TEST(!st.complete());
            BOOST_TEST(conn.get_channel().shared_sequence_number() == 0u);  // not used

            // 2nd row
            rv = fns.read_one_row(conn, st).get();
            BOOST_TEST(rv == makerow("max", nullptr));
            BOOST_TEST(!st.complete());

            // ok packet
            rv = fns.read_one_row(conn, st).get();
            BOOST_TEST(st.complete());
            BOOST_TEST(st.affected_rows() == 1u);
            BOOST_TEST(st.last_insert_id() == 6u);
            BOOST_TEST(st.warning_count() == 9u);
            BOOST_TEST(st.info() == "ab");
            BOOST_TEST(rv.empty());
        }
    }
}

BOOST_AUTO_TEST_CASE(resultset_already_complete)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto st = create_execution_state(resultset_encoding::text, {});
            st.complete(boost::mysql::detail::ok_packet{});
            test_connection conn;

            row_view rv = fns.read_one_row(conn, st).get();
            BOOST_TEST(rv.empty());
            BOOST_TEST(st.complete());

            // Doing it again works, too
            rv = fns.read_one_row(conn, st).get();
            BOOST_TEST(rv.empty());
            BOOST_TEST(st.complete());
        }
    }
}

BOOST_AUTO_TEST_CASE(error_reading_row)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto st = create_execution_state(resultset_encoding::text, {});
            test_connection conn;
            conn.stream().set_fail_count(fail_count(0, client_errc::server_unsupported));

            fns.read_one_row(conn, st).validate_error_exact(client_errc::server_unsupported);
        }
    }
}

BOOST_AUTO_TEST_CASE(error_deserializing_row)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto r = create_message(0, {0x00});  // invalid row
            auto st = create_execution_state(
                resultset_encoding::binary,
                {protocol_field_type::var_string}
            );
            test_connection conn;
            conn.stream().add_message(r);

            // deserialize row error
            fns.read_one_row(conn, st).validate_error_exact(client_errc::incomplete_message);
            BOOST_TEST(!st.complete());
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
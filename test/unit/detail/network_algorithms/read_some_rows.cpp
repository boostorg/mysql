//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/detail/network_algorithms/read_some_rows.hpp>
#include "boost/mysql/detail/protocol/resultset_encoding.hpp"
#include "boost/mysql/error.hpp"
#include "boost/mysql/resultset_base.hpp"
#include "boost/mysql/row_view.hpp"
#include <boost/asio/buffer.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/system_executor.hpp>
#include <boost/test/tools/context.hpp>
#include <boost/test/tools/interface.hpp>
#include <boost/test/unit_test.hpp>
#include "create_message.hpp"
#include "create_resultset.hpp"
#include "test_channel.hpp"
#include "test_common.hpp"

using boost::mysql::resultset_base;
using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::rows_view;
using boost::mysql::errc;
using boost::mysql::detail::resultset_encoding;
using boost::mysql::detail::protocol_field_type;
using namespace boost::mysql::test;


namespace
{

// Machinery to be able to cover the sync and async
// functions with the same test code
struct
{
    rows_view (*read_some_rows) (
        test_channel& chan,
        resultset_base& result,
        error_code& err,
        error_info& info
    );
    const char* name;
} all_fns [] = {
    {
        &boost::mysql::detail::read_some_rows<test_stream>,
        "sync"
    },
    {
        [](
            test_channel& channel,
            resultset_base& result,
            error_code& err,
            error_info& info
        )
        {
            boost::asio::io_context ctx_;
            rows_view res;
            boost::mysql::detail::async_read_some_rows(
                channel,
                result,
                info,
                boost::asio::bind_executor(ctx_.get_executor(), [&](error_code ec, rows_view rv) {
                    err = ec;
                    res = rv;
                })
            );
            ctx_.run();
            return res;
        },
        "async"
    }
};

BOOST_AUTO_TEST_SUITE(test_read_some_rows)

BOOST_AUTO_TEST_CASE(success_row_row_eof)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto row1 = create_message(4, { 0x00, 0x00, 0x03, 0x6d, 0x69, 0x6e, 0x6d, 0x07 });
            auto row2 = create_message(5, { 0x00, 0x08, 0x03, 0x6d, 0x61, 0x78 });
            auto ok_packet = create_message(6, { 0xfe, 0x01, 0x06, 0x02, 0x00, 0x09, 0x00, 0x02, 0x61, 0x62 });
            auto result = create_resultset(
                resultset_encoding::binary, 
                { protocol_field_type::var_string, protocol_field_type::short_ },
                4 // seqnum
            );
            auto chan = create_channel(concat_copy(row1, row2, ok_packet), 1024);
            chan.shared_fields().emplace_back("abc"); // from previous call
            error_code err;
            error_info info;

            rows_view rv = fns.read_some_rows(chan, result, err, info);
            BOOST_TEST(err == error_code());
            BOOST_TEST(info.message() == "");
            BOOST_TEST_REQUIRE(rv.size() == 2);
            BOOST_TEST(rv[0] == makerow("min", 1901));
            BOOST_TEST(rv[1] == makerow("max", nullptr));
            BOOST_TEST(result.complete());
            BOOST_TEST(result.affected_rows() == 1);
            BOOST_TEST(result.last_insert_id() == 6);
            BOOST_TEST(result.warning_count() == 9);
            BOOST_TEST(result.info() == "ab");
            BOOST_TEST(chan.shared_sequence_number() == 0); // not used
        }
    }
}

BOOST_AUTO_TEST_CASE(success_row_row_eof_separate)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto row1 = create_message(4, { 0x00, 0x00, 0x03, 0x6d, 0x69, 0x6e, 0x6d, 0x07 });
            auto row2 = create_message(5, { 0x00, 0x08, 0x03, 0x6d, 0x61, 0x78 });
            auto ok_packet = create_message(6, { 0xfe, 0x01, 0x06, 0x02, 0x00, 0x09, 0x00, 0x02, 0x61, 0x62 });
            auto result = create_resultset(
                resultset_encoding::binary, 
                { protocol_field_type::var_string, protocol_field_type::short_ },
                4 // seqnum
            );
            auto chan = create_channel({}, 1024);
            chan.lowest_layer().add_message(row1);
            chan.lowest_layer().add_message(concat_copy(row2, ok_packet));
            chan.shared_fields().emplace_back("abc"); // from previous call
            error_code err;
            error_info info;

            // 1st read
            rows_view rv = fns.read_some_rows(chan, result, err, info);
            BOOST_TEST(err == error_code());
            BOOST_TEST(info.message() == "");
            BOOST_TEST_REQUIRE(rv.size() == 1);
            BOOST_TEST(rv[0] == makerow("min", 1901));
            BOOST_TEST(!result.complete());

            // 2nd read
            rv = fns.read_some_rows(chan, result, err, info);
            BOOST_TEST(err == error_code());
            BOOST_TEST(info.message() == "");
            BOOST_TEST_REQUIRE(rv.size() == 1);
            BOOST_TEST(rv[0] == makerow("max", nullptr));
            BOOST_TEST(result.complete());
            BOOST_TEST(result.affected_rows() == 1);
            BOOST_TEST(result.last_insert_id() == 6);
            BOOST_TEST(result.warning_count() == 9);
            BOOST_TEST(result.info() == "ab");
            BOOST_TEST(chan.shared_sequence_number() == 0); // not used
        }
    }
}

BOOST_AUTO_TEST_CASE(success_row_eof_separate)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto row1 = create_message(4, { 0x00, 0x00, 0x03, 0x6d, 0x69, 0x6e, 0x6d, 0x07 });
            auto ok_packet = create_message(5, { 0xfe, 0x01, 0x06, 0x02, 0x00, 0x09, 0x00, 0x02, 0x61, 0x62 });
            auto result = create_resultset(
                resultset_encoding::binary, 
                { protocol_field_type::var_string, protocol_field_type::short_ },
                4 // seqnum
            );
            auto chan = create_channel({}, 1024);
            chan.lowest_layer().add_message(row1);
            chan.lowest_layer().add_message(ok_packet);
            chan.shared_fields().emplace_back("abc"); // from previous call
            error_code err;
            error_info info;

            // row
            rows_view rv = fns.read_some_rows(chan, result, err, info);
            BOOST_TEST(err == error_code());
            BOOST_TEST(info.message() == "");
            BOOST_TEST_REQUIRE(rv.size() == 1);
            BOOST_TEST(rv[0] == makerow("min", 1901));
            BOOST_TEST(!result.complete());

            // eof
            rv = fns.read_some_rows(chan, result, err, info);
            BOOST_TEST(rv.size() == 0);
            BOOST_TEST(result.complete());
            BOOST_TEST(result.affected_rows() == 1);
            BOOST_TEST(result.last_insert_id() == 6);
            BOOST_TEST(result.warning_count() == 9);
            BOOST_TEST(result.info() == "ab");
            BOOST_TEST(chan.shared_sequence_number() == 0); // not used
        }
    }
}

BOOST_AUTO_TEST_CASE(success_eof)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto ok_packet = create_message(4, { 0xfe, 0x01, 0x06, 0x02, 0x00, 0x09, 0x00, 0x02, 0x61, 0x62 });
            auto result = create_resultset(
                resultset_encoding::binary, 
                { protocol_field_type::var_string, protocol_field_type::short_ },
                4 // seqnum
            );
            auto chan = create_channel(ok_packet, 1024);
            chan.shared_fields().emplace_back("abc"); // from previous call
            error_code err;
            error_info info;

            rows_view rv = fns.read_some_rows(chan, result, err, info);
            BOOST_TEST(err == error_code());
            BOOST_TEST(info.message() == "");
            BOOST_TEST(rv.size() == 0);
            BOOST_TEST(result.complete());
            BOOST_TEST(result.affected_rows() == 1);
            BOOST_TEST(result.last_insert_id() == 6);
            BOOST_TEST(result.warning_count() == 9);
            BOOST_TEST(result.info() == "ab");
            BOOST_TEST(chan.shared_sequence_number() == 0); // not used
        }
    }
}

BOOST_AUTO_TEST_CASE(resultset_already_complete)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto result = create_resultset(resultset_encoding::text, {});
            result.complete(boost::mysql::detail::ok_packet{});
            test_channel chan = create_channel();
            error_code err;
            error_info info;

            rows_view rv = fns.read_some_rows(chan, result, err, info);
            BOOST_TEST(err == error_code());
            BOOST_TEST(info.message() == "");
            BOOST_TEST(rv.empty());
            BOOST_TEST(result.complete());

            // Doing it again works, too
            rv = fns.read_some_rows(chan, result, err, info);
            BOOST_TEST(err == error_code());
            BOOST_TEST(info.message() == "");
            BOOST_TEST(rv.empty());
            BOOST_TEST(result.complete());
        }
    }
}

BOOST_AUTO_TEST_CASE(error_reading_row)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto result = create_resultset(resultset_encoding::text, {});
            test_channel chan = create_channel();
            error_code err;
            error_info info;
            chan.lowest_layer().set_fail_count(fail_count(0, errc::no));

            rows_view rv = fns.read_some_rows(chan, result, err, info);
            BOOST_TEST(err == error_code(errc::no));
            BOOST_TEST(info.message() == "");
            BOOST_TEST(rv.empty());
            BOOST_TEST(!result.complete());
        }
    }
}

BOOST_AUTO_TEST_CASE(error_deserializing_row)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto r = create_message(0, { 0x00 }); // invalid row
            auto result = create_resultset(
                resultset_encoding::binary, 
                { protocol_field_type::var_string }
            );
            test_channel chan = create_channel();
            error_code err;
            error_info info;
            chan.lowest_layer().add_message(r);

            // deserialize row error
            rows_view rv = fns.read_some_rows(chan, result, err, info);
            BOOST_TEST(err == error_code(errc::incomplete_message));
            BOOST_TEST(info.message() == "");
            BOOST_TEST(rv.empty());
            BOOST_TEST(!result.complete());
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

}
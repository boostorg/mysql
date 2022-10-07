//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/detail/network_algorithms/read_one_row.hpp>
#include "boost/mysql/detail/protocol/common_messages.hpp"
#include "boost/mysql/detail/protocol/constants.hpp"
#include "boost/mysql/detail/protocol/resultset_encoding.hpp"
#include "boost/mysql/error.hpp"
#include "boost/mysql/resultset_base.hpp"
#include "boost/mysql/row_view.hpp"
#include <boost/asio/buffer.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/system_executor.hpp>
#include <boost/test/tools/context.hpp>
#include <boost/test/unit_test.hpp>
#include "create_message.hpp"
#include "create_resultset.hpp"
#include "test_channel.hpp"
#include "test_common.hpp"

using boost::mysql::resultset_base;
using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::row_view;
using boost::mysql::detail::resultset_encoding;
using namespace boost::mysql::test;


namespace
{

// Machinery to be able to cover the sync and async
// functions with the same test code
class read_one_row_fns
{
public:
    virtual ~read_one_row_fns() {}
    virtual row_view read_one_row(
        test_channel& channel,
        resultset_base& result,
        error_code& err,
        error_info& info
    ) = 0;
    virtual const char* name() const noexcept = 0;
};

class sync_read_one_row_fns : public read_one_row_fns
{
public:
    row_view read_one_row(
        test_channel& channel,
        resultset_base& result,
        error_code& err,
        error_info& info
    ) final override
    {
        return boost::mysql::detail::read_one_row(channel, result, err, info);
    }
    const char* name() const noexcept final override { return "sync"; };
};

class async_read_one_row_fns : public read_one_row_fns
{
public:
    row_view read_one_row(
        test_channel& channel,
        resultset_base& result,
        error_code& err,
        error_info& info
    ) final override
    {
        boost::asio::io_context ctx_;
        row_view res;
        boost::mysql::detail::async_read_one_row(
            channel,
            result,
            info,
            boost::asio::bind_executor(ctx_.get_executor(), [&](error_code ec, row_view rv) {
                err = ec;
                res = rv;
            })
        );
        ctx_.run();
        return res;
    }
    const char* name() const noexcept final override { return "async"; };
};


sync_read_one_row_fns sync;
async_read_one_row_fns async;
read_one_row_fns* all_reader_fns [] = { &sync, &async };


BOOST_AUTO_TEST_SUITE(test_read_one_row)

// Resultset is already complete
// Error reading row
// Error deserializing row

BOOST_AUTO_TEST_CASE(success)
{
    for (auto* fns : all_reader_fns)
    {
        BOOST_TEST_CONTEXT(fns->name())
        {
            auto row1 = create_message(4, { 0x00, 0x00, 0x03, 0x6d, 0x69, 0x6e, 0x6d, 0x07 });
            auto row2 = create_message(5, { 0x00, 0x08, 0x03, 0x6d, 0x61, 0x78 });
            auto ok_packet = create_message(6, { 0xfe, 0x01, 0x06, 0x02, 0x00, 0x09, 0x00, 0x02, 0x61, 0x62 });
            auto result = create_resultset(
                resultset_encoding::binary, 
                { boost::mysql::detail::protocol_field_type::var_string, boost::mysql::detail::protocol_field_type::short_ },
                4 // seqnum
            );
            test_channel chan;
            error_code err;
            error_info info;
            chan.lowest_layer().add_message(concat_copy(row1, row2, ok_packet));

            // 1st row
            row_view rv = fns->read_one_row(chan, result, err, info);
            BOOST_TEST(err == error_code());
            BOOST_TEST(info.message() == "");
            BOOST_TEST(rv == makerow("min", 1901));
            BOOST_TEST(!result.complete());
            BOOST_TEST(chan.shared_sequence_number() == 0); // not used

            // 2nd row
            rv = fns->read_one_row(chan, result, err, info);
            BOOST_TEST(err == error_code());
            BOOST_TEST(info.message() == "");
            BOOST_TEST(rv == makerow("max", nullptr));
            BOOST_TEST(!result.complete());

            // ok packet
            rv = fns->read_one_row(chan, result, err, info);
            BOOST_TEST(result.complete());
            BOOST_TEST(result.affected_rows() == 1);
            BOOST_TEST(result.last_insert_id() == 6);
            BOOST_TEST(result.warning_count() == 9);
            BOOST_TEST(result.info() == "ab");
            BOOST_TEST(rv.empty());
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

}
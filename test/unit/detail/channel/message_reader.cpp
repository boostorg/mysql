//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/system_executor.hpp>
#include <boost/test/tools/old/interface.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>
#include <boost/test/data/monomorphic/collection.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/mysql/detail/channel/message_reader.hpp>
#include "boost/mysql/errc.hpp"
#include "boost/mysql/error.hpp"
#include "assert_buffer_equals.hpp"
#include "test_stream.hpp"

using boost::mysql::detail::message_reader;
using boost::mysql::test::test_stream;
using boost::mysql::test::fail_count;
using boost::mysql::error_code;
using boost::mysql::errc;
using boost::asio::buffer;

/**
read_some
    has already a message
    + message that fits in the buffer
    message that fits in the buffer, but segmented in two
    message that doesn't fit in the buffer (segmented)
    there is a previous message, keep_messages = false
    there is a previous message, keep_messages = true
    + error in a read
next_message
    passed number seqnum mismatch
    intermediate frame seqnum mismatch
    OK, there is next message
    + OK, there isn't next message
    seqnum overflow
read_one
 */

namespace
{

// TODO: duplicated
std::vector<std::uint8_t> create_frame(std::uint8_t seqnum, std::vector<std::uint8_t> body)
{
    std::uint32_t body_size = body.size();
    boost::mysql::detail::packet_header header { boost::mysql::detail::int3{body_size}, seqnum };
    body.resize(body_size + 4);
    std::memmove(body.data() + 4, body.data(), body_size);
    boost::mysql::detail::serialization_context ctx (boost::mysql::detail::capabilities(), body.data());
    boost::mysql::detail::serialize(ctx, header);
    return body;
}


BOOST_AUTO_TEST_SUITE(test_message_reader)

BOOST_AUTO_TEST_SUITE(read_some)

BOOST_AUTO_TEST_CASE(message_fits_in_buffer)
{
    message_reader reader (512);
    std::uint8_t seqnum = 2;
    std::vector<std::uint8_t> msg_body {0x01, 0x02, 0x03};
    test_stream stream (create_frame(seqnum, msg_body));
    error_code err = boost::mysql::make_error_code(errc::no);

    // Doesn't have a message initially
    BOOST_TEST(!reader.has_message());

    // Read succesfully
    reader.read_some(stream, err);
    BOOST_TEST(err == error_code());
    BOOST_REQUIRE(reader.has_message());

    // Get next message and validate it
    auto msg = reader.get_next_message(seqnum, err);
    BOOST_REQUIRE(err == error_code());
    BOOST_TEST(seqnum == 3);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(msg, buffer(msg_body));

    // There isn't another message
    BOOST_TEST(!reader.has_message());
}

BOOST_AUTO_TEST_CASE(error)
{
    message_reader reader (512);
    test_stream stream (fail_count(0, error_code(errc::base64_decode_error)));
    error_code err = boost::mysql::make_error_code(errc::no);

    // Read with error
    reader.read_some(stream, err);
    BOOST_TEST(!reader.has_message());
    BOOST_TEST(err == error_code(errc::base64_decode_error));
}

BOOST_AUTO_TEST_SUITE_END()



// Most code is shared with the sync version,
// so we only test a success and an error case here
BOOST_AUTO_TEST_SUITE(async_read_some)

BOOST_AUTO_TEST_CASE(success)
{
    message_reader reader (512);
    std::uint8_t seqnum = 2;
    std::vector<std::uint8_t> msg_body {0x01, 0x02, 0x03};
    boost::asio::io_context ctx;
    test_stream stream (test_stream::read_behavior{create_frame(seqnum, msg_body)}, fail_count(), ctx.get_executor());
    error_code err = boost::mysql::make_error_code(errc::no);

    // Doesn't have a message initially
    BOOST_TEST(!reader.has_message());

    // Read succesfully
    reader.async_read_some(stream, [&](error_code e) { err = e; });
    ctx.run();
    BOOST_TEST(err == error_code());
    BOOST_REQUIRE(reader.has_message());

    // Get next message and validate it
    auto msg = reader.get_next_message(seqnum, err);
    BOOST_REQUIRE(err == error_code());
    BOOST_TEST(seqnum == 3);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(msg, buffer(msg_body));

    // There isn't another message
    BOOST_TEST(!reader.has_message());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()


}
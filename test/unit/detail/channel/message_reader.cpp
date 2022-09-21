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
#include "buffer_concat.hpp"

using boost::mysql::detail::message_reader;
using boost::mysql::test::test_stream;
using boost::mysql::test::fail_count;
using boost::mysql::test::concat_copy;
using boost::mysql::error_code;
using boost::mysql::errc;
using boost::asio::buffer;

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
    BOOST_TEST(stream.num_unread_bytes() == 0);

    // Get next message and validate it
    auto msg = reader.get_next_message(seqnum, err);
    BOOST_REQUIRE(err == error_code());
    BOOST_TEST(seqnum == 3);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(msg, buffer(msg_body));

    // There isn't another message
    BOOST_TEST(!reader.has_message());
}

BOOST_AUTO_TEST_CASE(fragmented_message_fits_in_buffer)
{
    message_reader reader (512);
    std::uint8_t seqnum = 2;
    std::vector<std::uint8_t> msg_body {0x01, 0x02, 0x03};
    test_stream stream (test_stream::read_behavior(
        create_frame(seqnum, msg_body),
        { 3, 5 } // break the message at bytes 3 and 5
    ));
    error_code err = boost::mysql::make_error_code(errc::no);

    // Doesn't have a message initially
    BOOST_TEST(!reader.has_message());

    // Read succesfully
    reader.read_some(stream, err);
    BOOST_TEST(err == error_code());
    BOOST_REQUIRE(reader.has_message());
    BOOST_TEST(stream.num_unread_bytes() == 0);

    // Get next message and validate it
    auto msg = reader.get_next_message(seqnum, err);
    BOOST_REQUIRE(err == error_code());
    BOOST_TEST(seqnum == 3);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(msg, buffer(msg_body));

    // There isn't another message
    BOOST_TEST(!reader.has_message());
}

BOOST_AUTO_TEST_CASE(message_doesnt_fit_in_buffer)
{
    message_reader reader (0);
    std::uint8_t seqnum = 2;
    std::vector<std::uint8_t> msg_body {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    test_stream stream (create_frame(seqnum, msg_body));
    error_code err = boost::mysql::make_error_code(errc::no);

    // Doesn't have a message initially
    BOOST_TEST(!reader.has_message());

    // Read succesfully
    reader.read_some(stream, err);
    BOOST_TEST(err == error_code());
    BOOST_REQUIRE(reader.has_message());
    BOOST_TEST(reader.buffer().size() >= msg_body.size());
    BOOST_TEST(stream.num_unread_bytes() == 0);

    // Get next message and validate it
    auto msg = reader.get_next_message(seqnum, err);
    BOOST_REQUIRE(err == error_code());
    BOOST_TEST(seqnum == 3);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(msg, buffer(msg_body));

    // There isn't another message
    BOOST_TEST(!reader.has_message());
}

BOOST_AUTO_TEST_CASE(two_messages)
{
    message_reader reader (512);
    std::uint8_t seqnum1 = 2;
    std::uint8_t seqnum2 = 5;
    std::vector<std::uint8_t> msg1_body {0x01, 0x02, 0x03};
    std::vector<std::uint8_t> msg2_body {0x05, 0x06, 0x07, 0x08};
    test_stream stream (
        concat_copy(
            create_frame(seqnum1, msg1_body),
            create_frame(seqnum2, msg2_body)
        )
    );
    error_code err = boost::mysql::make_error_code(errc::no);

    // Doesn't have a message initially
    BOOST_TEST(!reader.has_message());

    // Read succesfully
    reader.read_some(stream, err);
    BOOST_TEST(err == error_code());
    BOOST_REQUIRE(reader.has_message());
    BOOST_TEST(stream.num_unread_bytes() == 0);

    // Get next message and validate it
    auto msg = reader.get_next_message(seqnum1, err);
    BOOST_REQUIRE(err == error_code());
    BOOST_TEST(seqnum1 == 3);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(msg, buffer(msg1_body));

    // Reading again does nothing
    reader.read_some(stream, err);
    BOOST_TEST(err == error_code());

    // Get the 2nd message and validate it
    msg = reader.get_next_message(seqnum2, err);
    BOOST_REQUIRE(err == error_code());
    BOOST_TEST(seqnum2 == 6);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(msg, buffer(msg2_body));

    // There isn't another message
    BOOST_TEST(!reader.has_message());
}

BOOST_AUTO_TEST_CASE(previous_message_keep_messages_false)
{
    message_reader reader (512);
    std::uint8_t seqnum1 = 2;
    std::uint8_t seqnum2 = 5;
    std::vector<std::uint8_t> msg1_body {0x01, 0x02, 0x03};
    std::vector<std::uint8_t> msg2_body {0x05, 0x06, 0x07};
    test_stream stream;
    stream.add_message(create_frame(seqnum1, msg1_body));
    stream.add_message(create_frame(seqnum2, msg2_body));
    error_code err = boost::mysql::make_error_code(errc::no);

    // Read and get 1st message
    reader.read_some(stream, err, false);
    BOOST_TEST(err == error_code());
    BOOST_REQUIRE(reader.has_message());
    auto msg1 = reader.get_next_message(seqnum1, err);
    BOOST_REQUIRE(err == error_code());

    // Read and get 2nd message
    reader.read_some(stream, err, false);
    BOOST_TEST(err == error_code());
    BOOST_REQUIRE(reader.has_message());
    auto msg2 = reader.get_next_message(seqnum2, err);
    BOOST_REQUIRE(err == error_code());
    BOOST_TEST(stream.num_unread_bytes() == 0);
    BOOST_TEST(seqnum2 == 6);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(msg2, buffer(msg2_body));
    BOOST_TEST(!reader.has_message());

    // The 2nd message is located where the 1st message was
    BOOST_TEST(msg1.data() == msg2.data());
}

BOOST_AUTO_TEST_CASE(previous_message_keep_messages_true)
{
    message_reader reader (512);
    std::uint8_t seqnum1 = 2;
    std::uint8_t seqnum2 = 5;
    std::vector<std::uint8_t> msg1_body {0x01, 0x02, 0x03};
    std::vector<std::uint8_t> msg2_body {0x05, 0x06, 0x07};
    test_stream stream;
    stream.add_message(create_frame(seqnum1, msg1_body));
    stream.add_message(create_frame(seqnum2, msg2_body));
    error_code err = boost::mysql::make_error_code(errc::no);

    // Read and get 1st message
    reader.read_some(stream, err, true);
    BOOST_TEST(err == error_code());
    BOOST_REQUIRE(reader.has_message());
    auto msg1 = reader.get_next_message(seqnum1, err);
    BOOST_REQUIRE(err == error_code());

    // Read and get 2nd message
    reader.read_some(stream, err, true);
    BOOST_TEST(err == error_code());
    BOOST_REQUIRE(reader.has_message());
    auto msg2 = reader.get_next_message(seqnum2, err);
    BOOST_REQUIRE(err == error_code());
    BOOST_TEST(stream.num_unread_bytes() == 0);
    BOOST_TEST(seqnum2 == 6);
    BOOST_TEST(!reader.has_message());

    // Both messages are valid
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(msg1, buffer(msg1_body));
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(msg2, buffer(msg2_body));
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



// Cases specific to get_next_message
BOOST_AUTO_TEST_SUITE(get_next_message)

BOOST_AUTO_TEST_CASE(multiframe_message)
{
    message_reader reader (512, 8);
    std::uint8_t seqnum = 2;
    test_stream stream (
        concat_copy(
            create_frame(seqnum, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }),
            create_frame(3,      { 0x09, 0x0a })
        )
    );
    std::vector<std::uint8_t> expected_msg { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a };
    error_code err = boost::mysql::make_error_code(errc::no);

    // Read succesfully
    reader.read_some(stream, err);
    BOOST_TEST(err == error_code());
    BOOST_REQUIRE(reader.has_message());
    BOOST_TEST(stream.num_unread_bytes() == 0);

    // Get next message and validate it
    auto msg = reader.get_next_message(seqnum, err);
    BOOST_REQUIRE(err == error_code());
    BOOST_TEST(seqnum == 4);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(msg, buffer(expected_msg));

    // There isn't another message
    BOOST_TEST(!reader.has_message());
}

BOOST_AUTO_TEST_CASE(seqnum_overflow)
{
    message_reader reader (512);
    std::uint8_t seqnum = 0xff;
    std::vector<std::uint8_t> msg_body { 0x01, 0x02, 0x03 };
    test_stream stream (create_frame(seqnum, msg_body));
    error_code err = boost::mysql::make_error_code(errc::no);

    // Read succesfully
    reader.read_some(stream, err);
    BOOST_TEST(err == error_code());
    BOOST_REQUIRE(reader.has_message());
    BOOST_TEST(stream.num_unread_bytes() == 0);

    // Get next message and validate it
    auto msg = reader.get_next_message(seqnum, err);
    BOOST_REQUIRE(err == error_code());
    BOOST_TEST(seqnum == 0);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(msg, buffer(msg_body));
}

BOOST_AUTO_TEST_CASE(error_passed_seqnum_mismatch)
{
    message_reader reader (512);
    test_stream stream (create_frame(2, {0x01, 0x02, 0x03}));
    error_code err = boost::mysql::make_error_code(errc::no);

    // Read succesfully
    reader.read_some(stream, err);
    BOOST_TEST(err == error_code());
    BOOST_REQUIRE(reader.has_message());
    BOOST_TEST(stream.num_unread_bytes() == 0);

    // Passed-in seqnum is invalid
    std::uint8_t bad_seqnum = 0;
    reader.get_next_message(bad_seqnum, err);
    BOOST_TEST(err == make_error_code(errc::sequence_number_mismatch));
    BOOST_TEST(bad_seqnum == 0);
}

BOOST_AUTO_TEST_CASE(error_intermediate_frame_seqnum_mismatch)
{
    message_reader reader (512, 8); // frames are broken each 8 bytes
    std::vector<std::uint8_t> msg_body {0x01, 0x02, 0x03};
    std::uint8_t seqnum = 2;
    test_stream stream (
        concat_copy(
            create_frame(seqnum, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }),
            create_frame(4,      { 0x11, 0x12, 0x13, 0x14 }) // the right seqnum would be 3
        )
    );
    error_code err = boost::mysql::make_error_code(errc::no);

    // Read succesfully
    reader.read_some(stream, err);
    BOOST_TEST(err == error_code());
    BOOST_REQUIRE(reader.has_message());
    BOOST_TEST(stream.num_unread_bytes() == 0);

    // The read frame has a mismatched seqnum
    reader.get_next_message(seqnum, err);
    BOOST_TEST(err == make_error_code(errc::sequence_number_mismatch));
    BOOST_TEST(seqnum == 2);
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
    BOOST_TEST(stream.num_unread_bytes() == 0);

    // Reading again does nothing
    reader.async_read_some(stream, [&](error_code e) { err = e; });
    ctx.run();
    BOOST_TEST(err == error_code());

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
    boost::asio::io_context ctx;
    test_stream stream (
        fail_count(0, error_code(errc::base64_decode_error)),
        ctx.get_executor()
    );
    error_code err = boost::mysql::make_error_code(errc::no);

    // Read with error
    reader.async_read_some(stream, [&](error_code ec) { err = ec; });
    ctx.run();
    BOOST_TEST(!reader.has_message());
    BOOST_TEST(err == error_code(errc::base64_decode_error));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()


}
//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/impl/internal/sansio/message_writer.hpp>

#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <vector>

#include "test_common/assert_buffer_equals.hpp"
#include "test_common/buffer_concat.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/mock_message.hpp"

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using boost::span;

namespace {

BOOST_AUTO_TEST_SUITE(test_message_writer)

BOOST_AUTO_TEST_CASE(regular_message)
{
    message_writer writer;
    const std::vector<std::uint8_t> msg_body{0x01, 0x02, 0x03};
    std::uint8_t seqnum = 2;

    // Operation start
    writer.prepare_write(mock_message{msg_body}, seqnum);
    BOOST_TEST(!writer.done());

    // First (and only) chunk
    auto chunk = writer.current_chunk();
    auto expected = create_frame(2, msg_body);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful
    writer.resume(7);
    BOOST_TEST(seqnum == 3u);
    BOOST_TEST(writer.done());
}

BOOST_AUTO_TEST_CASE(short_writes)
{
    message_writer writer;
    const std::vector<std::uint8_t> msg_body{0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    std::uint8_t seqnum = 2;

    // Operation start
    writer.prepare_write(mock_message{msg_body}, seqnum);

    // First chunk
    auto chunk = writer.current_chunk();
    auto expected = create_frame(2, msg_body);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // Write signals partial write
    writer.resume(3);
    BOOST_TEST(seqnum == 3u);
    BOOST_TEST(!writer.done());

    // Remaining of the chunk
    chunk = writer.current_chunk();
    span<const std::uint8_t> expected_buff(expected.data() + 3, 7);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected_buff);

    // Another partial write
    writer.resume(2);
    BOOST_TEST(seqnum == 3u);
    BOOST_TEST(!writer.done());

    // Remaining of the chunk
    chunk = writer.current_chunk();
    expected_buff = span<const std::uint8_t>(expected.data() + 5, 5);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected_buff);

    // Zero bytes partial writes work correctly
    BOOST_TEST(seqnum == 3u);
    BOOST_TEST(!writer.done());

    // Remaining of the chunk
    chunk = writer.current_chunk();
    expected_buff = span<const std::uint8_t>(expected.data() + 5, 5);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected_buff);

    // Final bytes
    writer.resume(5);
    BOOST_TEST(seqnum == 3u);
    BOOST_TEST(writer.done());
}

BOOST_AUTO_TEST_CASE(several_messages)
{
    message_writer writer;
    std::vector<std::uint8_t> msg_1{0x01, 0x02, 0x04};
    std::vector<std::uint8_t> msg_2{0x04, 0x05, 0x06, 0x09, 0xff};
    std::vector<std::uint8_t> msg_3{0x02, 0xab};
    std::uint8_t seqnum_1 = 2;
    std::uint8_t seqnum_2 = 42;
    std::uint8_t seqnum_3 = 21;

    // Operation start for message 1
    writer.prepare_write(mock_message{msg_1}, seqnum_1);
    BOOST_TEST(!writer.done());

    // Chunk 1
    auto chunk = writer.current_chunk();
    auto expected = create_frame(2, msg_1);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful message 1
    writer.resume(7);
    BOOST_TEST(seqnum_1 == 3u);
    BOOST_TEST(writer.done());

    // Operation start for message 2
    writer.prepare_write(mock_message{msg_2}, seqnum_2);
    BOOST_TEST(!writer.done());

    // Chunk 2
    chunk = writer.current_chunk();
    expected = create_frame(42, msg_2);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful message 2
    writer.resume(9);
    BOOST_TEST(seqnum_2 == 43u);
    BOOST_TEST(writer.done());

    // Operation start for message 3
    writer.prepare_write(mock_message{msg_3}, seqnum_3);
    BOOST_TEST(!writer.done());

    // Chunk 3
    chunk = writer.current_chunk();
    expected = create_frame(21, msg_3);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful message 3
    writer.resume(6);
    BOOST_TEST(seqnum_3 == 22u);
    BOOST_TEST(writer.done());
}

// After a message has been partially written, we can
// prepare a write and resume operation normally. This can
// happen after a tiemout ellapses and a reconnection occurs.
BOOST_AUTO_TEST_CASE(message_half_written)
{
    message_writer writer;
    std::vector<std::uint8_t> msg_1{0x01, 0x02, 0x04};
    std::vector<std::uint8_t> msg_2{0x04, 0x05, 0x06, 0x09, 0xff};
    std::uint8_t seqnum_1 = 2;
    std::uint8_t seqnum_2 = 42;

    // Operation start for message 1
    writer.prepare_write(mock_message{msg_1}, seqnum_1);
    BOOST_TEST(!writer.done());

    // Chunk 1
    auto chunk = writer.current_chunk();
    auto expected = create_frame(2, msg_1);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // Remaining of message 1 is never written.
    // Operation start for message 2
    writer.prepare_write(mock_message{msg_2}, seqnum_2);
    BOOST_TEST(!writer.done());

    // Chunk 2
    chunk = writer.current_chunk();
    expected = create_frame(42, msg_2);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful message 2
    writer.resume(9);
    BOOST_TEST(seqnum_2 == 43u);
    BOOST_TEST(writer.done());
}

BOOST_AUTO_TEST_CASE(pipelined_write)
{
    message_writer writer;
    std::vector<std::uint8_t> msg_1{0x01, 0x02, 0x04, 0x05};
    std::vector<std::uint8_t> msg_2{0x04, 0x05, 0x06, 0x09, 0xff};
    std::uint8_t seqnum_1 = 2;
    std::uint8_t seqnum_2 = 42;

    // Prepare a pipelined write. This is currently limited
    // to two messages which are shorter than the max frame size.
    writer.prepare_pipelined_write(mock_message{msg_1}, seqnum_1, mock_message{msg_2}, seqnum_2);
    BOOST_TEST(!writer.done());

    // Chunk 1
    auto chunk = writer.current_chunk();
    auto expected = buffer_builder().add(create_frame(2, msg_1)).add(create_frame(42, msg_2)).build();
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // Some bytes acknowledged
    writer.resume(5);
    chunk = writer.current_chunk();
    auto expected_view = span<std::uint8_t>(expected).subspan(5);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected_view);
    BOOST_TEST(!writer.done());

    // More bytes acknowledged
    writer.resume(10);
    chunk = writer.current_chunk();
    expected_view = span<std::uint8_t>(expected).subspan(15);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected_view);
    BOOST_TEST(!writer.done());

    // Final bytes acknowledged
    writer.resume(2);
    BOOST_TEST(writer.done());
    BOOST_TEST(seqnum_1 == 3u);
    BOOST_TEST(seqnum_2 == 43u);
}

// Interleaving pipelined writes with regular writes works
BOOST_AUTO_TEST_CASE(pipelined_write_interleaved)
{
    message_writer writer;
    std::vector<std::uint8_t> msg_1{0x01, 0x02, 0x04};
    std::vector<std::uint8_t> msg_2{0x04, 0x05, 0x06, 0x09, 0xff};
    std::vector<std::uint8_t> msg_3{0x02, 0xab};
    std::vector<std::uint8_t> msg_4{0x05};
    std::uint8_t seqnum_1 = 2;
    std::uint8_t seqnum_2 = 42;
    std::uint8_t seqnum_3 = 21;
    std::uint8_t seqnum_4 = 100;

    // Operation start for message 1
    writer.prepare_write(mock_message{msg_1}, seqnum_1);
    BOOST_TEST(!writer.done());

    // Chunk 1
    auto chunk = writer.current_chunk();
    auto expected = create_frame(2, msg_1);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful message 1
    writer.resume(7);
    BOOST_TEST(seqnum_1 == 3u);
    BOOST_TEST(writer.done());

    // Operation start for messages 2 and 3
    writer.prepare_pipelined_write(mock_message{msg_2}, seqnum_2, mock_message{msg_3}, seqnum_3);
    BOOST_TEST(!writer.done());

    // Chunk 2
    chunk = writer.current_chunk();
    expected = concat_copy(create_frame(42, msg_2), create_frame(21, msg_3));
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful messages 2 and 3
    writer.resume(15);
    BOOST_TEST(seqnum_2 == 43u);
    BOOST_TEST(seqnum_3 == 22u);
    BOOST_TEST(writer.done());

    // Operation start for message 4
    writer.prepare_write(mock_message{msg_4}, seqnum_4);
    BOOST_TEST(!writer.done());

    // Chunk 4
    chunk = writer.current_chunk();
    expected = create_frame(100, msg_4);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful message 4
    writer.resume(5);
    BOOST_TEST(seqnum_4 == 101u);
    BOOST_TEST(writer.done());
}

// Leaving a message half written is not a problem
BOOST_AUTO_TEST_CASE(pipelined_message_half_written)
{
    message_writer writer;
    std::vector<std::uint8_t> msg_1{0x01, 0x02, 0x04};
    std::vector<std::uint8_t> msg_2{0x04, 0x05, 0x06, 0x09, 0xff};
    std::vector<std::uint8_t> msg_3{0x02, 0xab};
    std::uint8_t seqnum_1 = 2;
    std::uint8_t seqnum_2 = 42;
    std::uint8_t seqnum_3 = 21;

    // Operation start for messages 1 and 2
    writer.prepare_pipelined_write(mock_message{msg_1}, seqnum_1, mock_message{msg_2}, seqnum_2);
    BOOST_TEST(!writer.done());
    auto chunk = writer.current_chunk();
    auto expected = concat_copy(create_frame(2, msg_1), create_frame(42, msg_2));
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // Part of this pipeline is written
    writer.resume(8);
    BOOST_TEST(!writer.done());

    // The rest is never written
    // Operation start for message 3
    writer.prepare_write(mock_message{msg_3}, seqnum_3);
    BOOST_TEST(!writer.done());

    // Chunk
    chunk = writer.current_chunk();
    expected = create_frame(21, msg_3);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful message 3
    writer.resume(6);
    BOOST_TEST(seqnum_3 == 22u);
    BOOST_TEST(writer.done());
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

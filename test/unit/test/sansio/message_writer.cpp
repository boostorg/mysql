//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/impl/internal/sansio/message_writer.hpp>

#include <boost/test/unit_test.hpp>

#include <cstddef>
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

BOOST_AUTO_TEST_SUITE(chunk_processor_)
BOOST_AUTO_TEST_CASE(reset)
{
    chunk_processor chunk_proc;
    BOOST_TEST(chunk_proc.done());
}

BOOST_AUTO_TEST_CASE(zero_to_size_steps)
{
    chunk_processor chunk_proc;
    std::vector<std::uint8_t> buff(10, 0);

    chunk_proc.reset(0, 10);
    BOOST_TEST(!chunk_proc.done());
    auto chunk = chunk_proc.get_chunk(buff);
    BOOST_TEST(chunk.data() == buff.data());
    BOOST_TEST(chunk.size() == 10u);

    chunk_proc.on_bytes_written(3);
    BOOST_TEST(!chunk_proc.done());
    chunk = chunk_proc.get_chunk(buff);
    BOOST_TEST(chunk.data() == buff.data() + 3);
    BOOST_TEST(chunk.size() == 7u);

    chunk_proc.on_bytes_written(6);
    BOOST_TEST(!chunk_proc.done());
    chunk = chunk_proc.get_chunk(buff);
    BOOST_TEST(chunk.data() == buff.data() + 9);
    BOOST_TEST(chunk.size() == 1u);

    chunk_proc.on_bytes_written(1);
    BOOST_TEST(chunk_proc.done());
    chunk = chunk_proc.get_chunk(buff);
    BOOST_TEST(chunk.size() == 0u);
}

BOOST_AUTO_TEST_CASE(nonzero_to_size_steps)
{
    chunk_processor chunk_proc;
    chunk_proc.reset(2, 21);  // simulate a previous op
    std::vector<std::uint8_t> buff(10, 0);

    chunk_proc.reset(3, 7);
    BOOST_TEST(!chunk_proc.done());
    auto chunk = chunk_proc.get_chunk(buff);
    BOOST_TEST(chunk.data() == buff.data() + 3);
    BOOST_TEST(chunk.size() == 4u);

    chunk_proc.on_bytes_written(3);
    BOOST_TEST(!chunk_proc.done());
    chunk = chunk_proc.get_chunk(buff);
    BOOST_TEST(chunk.data() == buff.data() + 6);
    BOOST_TEST(chunk.size() == 1u);

    chunk_proc.on_bytes_written(1);
    BOOST_TEST(chunk_proc.done());
    chunk = chunk_proc.get_chunk(buff);
    BOOST_TEST(chunk.size() == 0u);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(regular_message)
{
    message_writer writer(8);
    std::vector<std::uint8_t> msg_body{0x01, 0x02, 0x03};
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
    message_writer writer(8);
    std::vector<std::uint8_t> msg_body{0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
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

BOOST_AUTO_TEST_CASE(empty_message)
{
    message_writer writer(8);
    std::uint8_t seqnum = 2;

    // Operation start
    writer.prepare_write(mock_message{}, seqnum);
    BOOST_TEST(!writer.done());

    // Chunk should only contain the header
    auto chunk = writer.current_chunk();
    auto expected = create_empty_frame(2);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful
    writer.resume(4);
    BOOST_TEST(seqnum == 3u);
    BOOST_TEST(writer.done());
}

BOOST_AUTO_TEST_CASE(message_with_max_frame_size_length)
{
    message_writer writer(8);
    std::vector<std::uint8_t> msg_body{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    std::uint8_t seqnum = 2;

    // Operation start
    writer.prepare_write(mock_message{msg_body}, seqnum);
    BOOST_TEST(!writer.done());

    // Chunk
    auto chunk = writer.current_chunk();
    auto expected = create_frame(2, msg_body);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful
    writer.resume(12);
    BOOST_TEST(!writer.done());

    // Next chunk is an empty frame
    chunk = writer.current_chunk();
    expected = create_empty_frame(3);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful
    writer.resume(4);
    BOOST_TEST(seqnum == 4u);
    BOOST_TEST(writer.done());
}

BOOST_AUTO_TEST_CASE(multiframe_message)
{
    message_writer writer(8);
    std::vector<std::uint8_t> msg_frame_1{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    std::vector<std::uint8_t> msg_frame_2{0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18};
    std::vector<std::uint8_t> msg_frame_3{0x21};
    auto msg = buffer_builder().add(msg_frame_1).add(msg_frame_2).add(msg_frame_3).build();
    std::uint8_t seqnum = 2;

    // Operation start
    writer.prepare_write(mock_message{msg}, seqnum);
    BOOST_TEST(!writer.done());

    // Chunk 1
    auto chunk = writer.current_chunk();
    auto expected = create_frame(2, msg_frame_1);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful 1 (short write)
    writer.resume(4);
    BOOST_TEST(!writer.done());

    // Rest of chunk 1
    chunk = writer.current_chunk();
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, span<const std::uint8_t>(expected.data() + 4, 8));

    // On write rest of chunk 1
    writer.resume(8);
    BOOST_TEST(!writer.done());

    // Chunk 2
    chunk = writer.current_chunk();
    expected = create_frame(3, msg_frame_2);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful 2
    writer.resume(12);
    BOOST_TEST(!writer.done());

    // Chunk 3
    chunk = writer.current_chunk();
    expected = create_frame(4, msg_frame_3);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful
    writer.resume(5);
    BOOST_TEST(seqnum == 5u);
    BOOST_TEST(writer.done());
}

BOOST_AUTO_TEST_CASE(multiframe_message_with_max_frame_size)
{
    message_writer writer(8);
    std::vector<std::uint8_t> msg_frame_1{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    std::vector<std::uint8_t> msg_frame_2{0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18};
    auto msg = concat_copy(msg_frame_1, msg_frame_2);
    std::uint8_t seqnum = 2;

    // Operation start
    writer.prepare_write(mock_message{msg}, seqnum);
    BOOST_TEST(!writer.done());

    // Chunk 1
    auto chunk = writer.current_chunk();
    auto expected = create_frame(2, msg_frame_1);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful 1
    writer.resume(12);
    BOOST_TEST(!writer.done());

    // Chunk 2
    chunk = writer.current_chunk();
    expected = create_frame(3, msg_frame_2);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful 2
    writer.resume(12);
    BOOST_TEST(!writer.done());

    // Chunk 3 (empty)
    chunk = writer.current_chunk();
    expected = create_empty_frame(4);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful
    writer.resume(4);
    BOOST_TEST(seqnum == 5u);
    BOOST_TEST(writer.done());
}

BOOST_AUTO_TEST_CASE(seqnum_overflow)
{
    message_writer writer(8);
    std::vector<std::uint8_t> msg{0x01, 0x02};
    std::uint8_t seqnum = 0xff;

    // Operation start
    writer.prepare_write(mock_message{msg}, seqnum);
    BOOST_TEST(!writer.done());

    // Prepare chunk
    auto chunk = writer.current_chunk();
    auto expected = create_frame(0xff, msg);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful
    writer.resume(6);
    BOOST_TEST(seqnum == 0);
    BOOST_TEST(writer.done());
}

BOOST_AUTO_TEST_CASE(several_messages)
{
    message_writer writer(8);
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
    message_writer writer(8);
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
    message_writer writer(8);
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
    message_writer writer(8);
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
    message_writer writer(8);
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

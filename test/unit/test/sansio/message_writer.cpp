//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/impl/internal/sansio/message_writer.hpp>

#include <boost/core/span.hpp>
#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <vector>

#include "test_common/assert_buffer_equals.hpp"
#include "test_unit/create_frame.hpp"

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using boost::span;

namespace {

BOOST_AUTO_TEST_SUITE(test_message_writer)

// TODO: we should test in connection_state_data that we increase seqnum correctly
// and that we clear the buffer

BOOST_AUTO_TEST_CASE(regular_message)
{
    message_writer writer;
    const std::vector<std::uint8_t> msg_body{0x01, 0x02, 0x03};

    // Operation start
    writer.reset(msg_body);
    BOOST_TEST(!writer.done());

    // First (and only) chunk
    auto chunk = writer.current_chunk();
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, msg_body);

    // On write successful
    writer.resume(3);
    BOOST_TEST(writer.done());
}

BOOST_AUTO_TEST_CASE(short_writes)
{
    message_writer writer;
    const std::vector<std::uint8_t> msg_body{0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

    // Operation start
    writer.reset(msg_body);

    // First chunk
    auto chunk = writer.current_chunk();
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, msg_body);

    // Write signals partial write
    writer.resume(2);
    BOOST_TEST(!writer.done());

    // Remaining of the chunk
    chunk = writer.current_chunk();
    span<const std::uint8_t> expected_buff(msg_body.data() + 2, 4);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected_buff);

    // Another partial write
    writer.resume(2);
    BOOST_TEST(!writer.done());

    // Remaining of the chunk
    chunk = writer.current_chunk();
    expected_buff = span<const std::uint8_t>(msg_body.data() + 4, 2);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected_buff);

    // Zero bytes partial writes work correctly
    BOOST_TEST(!writer.done());

    // Remaining of the chunk
    chunk = writer.current_chunk();
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected_buff);

    // Final bytes
    writer.resume(2);
    BOOST_TEST(writer.done());
}

BOOST_AUTO_TEST_CASE(several_messages)
{
    message_writer writer;
    std::vector<std::uint8_t> msg_1{0x01, 0x02, 0x04};
    std::vector<std::uint8_t> msg_2{0x04, 0x05, 0x06, 0x09, 0xff};
    std::vector<std::uint8_t> msg_3{0x02, 0xab};

    // Operation start for message 1
    writer.reset(msg_1);
    BOOST_TEST(!writer.done());

    // Chunk 1
    auto chunk = writer.current_chunk();
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, msg_1);

    // On write successful message 1
    writer.resume(3);
    BOOST_TEST(writer.done());

    // Operation start for message 2
    writer.reset(msg_2);
    BOOST_TEST(!writer.done());

    // Chunk 2
    chunk = writer.current_chunk();
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, msg_2);

    // Partial write for chunk 2
    writer.resume(1);
    chunk = writer.current_chunk();
    BOOST_TEST(!writer.done());
    span<const std::uint8_t> expected{msg_2.data() + 1, 4};
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful message 2
    writer.resume(4);
    BOOST_TEST(writer.done());

    // Operation start for message 3
    writer.reset(msg_3);
    BOOST_TEST(!writer.done());

    // Chunk 3
    chunk = writer.current_chunk();
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, msg_3);

    // On write successful message 3
    writer.resume(2);
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

    // Operation start for message 1
    writer.reset(msg_1);
    BOOST_TEST(!writer.done());

    // Chunk 1
    auto chunk = writer.current_chunk();
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, msg_1);

    // Write part of message 1
    writer.resume(1);
    BOOST_TEST(!writer.done());
    chunk = writer.current_chunk();
    span<const std::uint8_t> expected{msg_1.data() + 1, 2};
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // Remaining of message 1 is never written.
    // Operation start for message 2
    writer.reset(msg_2);
    BOOST_TEST(!writer.done());

    // Chunk 2
    chunk = writer.current_chunk();
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, msg_2);

    // Write part of message 2
    writer.resume(2);
    BOOST_TEST(!writer.done());
    chunk = writer.current_chunk();
    expected = {msg_2.data() + 2, 3};
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chunk, expected);

    // On write successful message 2
    writer.resume(3);
    BOOST_TEST(writer.done());
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

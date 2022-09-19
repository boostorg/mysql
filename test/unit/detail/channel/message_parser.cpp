//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/detail/protocol/capabilities.hpp>
#include <boost/mysql/detail/protocol/serialization.hpp>
#include <boost/mysql/detail/protocol/serialization_context.hpp>
#include <boost/mysql/detail/channel/read_buffer.hpp>
#include <boost/mysql/detail/channel/message_parser.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include "assert_buffer_equals.hpp"

using boost::mysql::detail::message_parser;
using boost::mysql::detail::read_buffer;
using boost::mysql::detail::packet_header;
using boost::mysql::detail::int3;

/**
process_message
    fragmented msg 1
        header pìece
        header piece
        completes header
        body piece
        body piece
        completes body
    fragmented msg 2
        header
        body
    fragmented msg 3
        header + body part
        end of body + next header + next body part 
    full message: header + body
    several messages, then fragmented one
        header 1 + body 1 + header 2 + body 2 + header 3 + body fragment 3
    several messages
        header 1 + body 1 + header 2 + body 2
    2-frame message
        header 1 + body 1 part
        body 1 part
        body 1 part + header 2 + body 2 part
        body 2 part
    3-frame message
        header 1 + body 1 part
        body 1 part
        body 1 part + header 2 + body 2 part
        body 2 part
        body 2 part + header 3 + body 3
    2-frame message in single read (not possible?)
        header 1 + body 1 + header 2 + body 2
    2-frame message with fragmented header
        header 1 piece
        header 1 piece + body 1 piece
        body 1 piece + header 2 piece
    coming from an already processed message (can we get rid of this?)
    coming from an error (can we get rid of this?)
    with reserved area
    2-frame message with mismatched seqnums
    2 different frames with "mismatched" seqnums
 * 
 */

namespace
{

class parser_test
{
    message_parser parser_;
    read_buffer buff_;
    std::vector<std::uint8_t> contents_;
    std::size_t bytes_written_ {0};
    const std::uint8_t* buffer_first_;
public:
    parser_test(std::vector<std::uint8_t> contents, std::size_t buffsize = 512) :
        buff_(buffsize), contents_(std::move(contents)), buffer_first_(buff_.first()) {}
    read_buffer& buffer() noexcept { return buff_; }
    message_parser::result parse_bytes(std::size_t num_bytes)
    {
        message_parser::result res;
        std::memcpy(buff_.free_first(), contents_.data() + bytes_written_, num_bytes);
        bytes_written_ += num_bytes;
        buff_.move_to_pending(num_bytes);
        parser_.parse_message(buff_, res);
        return res;
    }
    void check_message(const std::vector<std::uint8_t>& contents)
    {
        BOOST_MYSQL_ASSERT_BUFFER_EQUALS(
            boost::asio::buffer(buff_.current_message_first() - contents.size(), contents.size()),
            boost::asio::buffer(contents)
        );
    }
    void check_buffer_stability()
    {
        BOOST_TEST(buff_.first() == buffer_first_);
    }
};

std::vector<std::uint8_t> create_message(std::uint8_t seqnum, std::vector<std::uint8_t> body)
{
    std::uint32_t body_size = body.size();
    packet_header header { int3{body_size}, seqnum };
    body.resize(body_size + 4);
    std::memmove(body.data() + 4, body.data(), body_size);
    boost::mysql::detail::serialization_context ctx (boost::mysql::detail::capabilities(), body.data());
    boost::mysql::detail::serialize(ctx, header);
    return body;
}

BOOST_AUTO_TEST_SUITE(message_parser_parse_message)

BOOST_AUTO_TEST_CASE(fragmented_header_and_body)
{
    // message to be parsed
    parser_test fixture (create_message(0, { 0x01, 0x02, 0x03 }));

    // 1 byte in the header received
    auto res = fixture.parse_bytes(1);
    BOOST_TEST(!res.has_message);
    BOOST_TEST(res.required_size == 3);

    // Another 2 bytes received
    res = fixture.parse_bytes(2);
    BOOST_TEST(!res.has_message);
    BOOST_TEST(res.required_size == 1);

    // Header fully received
    res = fixture.parse_bytes(1);
    BOOST_TEST(!res.has_message);
    BOOST_TEST(res.required_size == 3);

    // 1 byte in body received
    res = fixture.parse_bytes(1);
    BOOST_TEST(!res.has_message);
    BOOST_TEST(res.required_size == 2);

    // body fully received (single frame messages keep header as an optimization)
    res = fixture.parse_bytes(2);
    fixture.check_message({ 0x01, 0x02, 0x03 });
    BOOST_TEST(res.has_message);
    BOOST_TEST(res.message.size == 3);
    BOOST_TEST(res.message.seqnum_first == 0);
    BOOST_TEST(res.message.seqnum_last == 0);
    BOOST_TEST(!res.message.has_seqnum_mismatch);

    // Buffer did not reallocate
    fixture.check_buffer_stability();
}

BOOST_AUTO_TEST_SUITE_END()

}
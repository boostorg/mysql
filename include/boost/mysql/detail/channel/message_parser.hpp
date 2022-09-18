//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CHANNEL_MESSAGE_PARSER_HPP
#define BOOST_MYSQL_DETAIL_CHANNEL_MESSAGE_PARSER_HPP

#include <cstddef>
#include <cstdint>
#include <boost/mysql/detail/channel/read_buffer.hpp>

namespace boost {
namespace mysql {
namespace detail {


class message_parser
{
    struct state_t
    {
        bool is_first_frame {true};
        std::uint8_t seqnum_first {};
        std::uint8_t seqnum_last {};
        bool reading_header {false};
        std::size_t remaining_bytes {0};
        bool more_frames_follow {false};
        bool has_seqnum_mismatch {false};
    };

    state_t state_;
public:
    struct result
    {
        bool has_message; // whether it has a message or not
        std::size_t required_size; // if !has_message, number of bytes required to parse the current message
        struct message_t
        {
            std::uint8_t seqnum_first;
            std::uint8_t seqnum_last;
            std::size_t size;
            bool has_seqnum_mismatch; // for multi-frame messages, set to true if an error mismatch happened
        } message; // if has_message, the actual parsed message

        void set_required_size(std::size_t size) noexcept
        {
            has_message = false;
            required_size = size;
        }

        void set_message(const message_t& msg) noexcept
        {
            has_message = true;
            message = msg;
        }
    };

    message_parser() = default;

    // Attempts to process a message from buff and puts it into msg.
    // If a message is read, returns true, and msg.message_first and msg.message_last
    // will contain offsets into buff's reserved area. Otherwise, required_size will contain
    // the number of bytes needed to complete the message part we're parsing. 
    inline void parse_message(read_buffer& buff, result& res) noexcept;
};


} // detail
} // mysql
} // boost


#include <boost/mysql/detail/channel/impl/message_parser.ipp>

#endif

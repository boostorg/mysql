//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CHANNEL_MESSAGE_READER_HPP
#define BOOST_MYSQL_DETAIL_CHANNEL_MESSAGE_READER_HPP

#include <boost/asio/buffer.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/detail/channel/read_buffer.hpp>
#include <boost/mysql/detail/auxiliar/valgrind.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <cassert>
#include <cstddef>
#include <cstdint>


namespace boost {
namespace mysql {
namespace detail {

class message_reader
{
public:
    message_reader(std::size_t initial_buffer_size) : buffer_(initial_buffer_size) {}
    bool keep_messages() const noexcept { return keep_messages_; }
    void set_keep_messages(bool v) noexcept { keep_messages_ = v; }

    bool has_message() const noexcept { return result_.is_message(); }
    const std::uint8_t* buffer_first() const noexcept { return buffer_.reserved_first(); }

    inline boost::asio::const_buffer get_next_message(std::uint8_t& seqnum, error_code& ec) noexcept;

    // Reads some messages from stream, until there is at least one
    template <class Stream>
    void read_some(Stream& stream, error_code& ec);

    template <class Stream, class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_read_some(Stream& stream, CompletionToken&& token);

    // Exposed for the sake of testing
    read_buffer& buffer() noexcept { return buffer_; }
    inline error_code process_message();
    std::size_t grow_buffer_to_fit() const noexcept { assert(result_.is_state()); return result_.state.grow_buffer_to_fit; }
private:

    template <class Stream>
    struct read_op;

    enum class result_type
    {
        message,
        state,
        error
    };

    struct message_t
    {
        std::uint8_t seqnum_first;
        std::uint8_t seqnum_last;
    };

    struct state_t
    {
        bool is_first_frame {true};
        std::uint8_t first_seqnum {};
        std::uint8_t last_seqnum {};
        bool reading_header {false};
        std::size_t remaining_bytes {0};
        bool more_frames_follow {false};
        std::size_t grow_buffer_to_fit {};
    };


    // Union-like; the result produced by process_message may be
    // either a state (representing an incomplete message), a message
    // (representing a complete message) or an error_code
    struct result_t
    {
        result_type type { result_type::state };
        state_t state;
        message_t message;
        error_code next_error;

        bool is_state() const noexcept { return type == result_type::state; }
        bool is_message() const noexcept { return type == result_type::message; }
    };

    read_buffer buffer_;
    bool keep_messages_ {false};
    result_t result_;

    inline void maybe_remove_processed_messages();
    inline void maybe_resize_buffer();
    inline error_code on_read_bytes(size_t num_bytes);
};


} // detail
} // mysql
} // boost

#include <boost/mysql/detail/channel/message_reader.hpp>

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_STATIC_STRING_HPP_ */





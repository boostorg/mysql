//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CHANNEL_MESSAGE_READER_HPP
#define BOOST_MYSQL_DETAIL_CHANNEL_MESSAGE_READER_HPP

#include <algorithm>
#include <boost/asio/buffer.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/mysql/error.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/mysql/detail/channel/read_buffer.hpp>
#include <boost/mysql/detail/auxiliar/valgrind.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

class message_reader
{
public:
    message_reader(std::size_t initial_buffer_size) :
        buffer_(initial_buffer_size),
        message_cursor_(0)
    {
    }

    std::size_t num_cached_messages() const noexcept { return cached_messages_.size(); }

    boost::asio::const_buffer get_next_message(std::uint8_t& seqnum, error_code& ec) noexcept
    {
        assert(message_cursor_ < cached_messages_.size());
        const auto& msg = cached_messages_[message_cursor_];
        if (seqnum != msg.seqnum_first)
        {
            ec = make_error_code(errc::sequence_number_mismatch);
            return {};
        }
        seqnum = msg.seqnum_last + 1;
        ++message_cursor_;
        return boost::asio::buffer(buffer_.processing_first() + msg.first, msg.length);
    }

    // Reads at least num_messages from Stream
    template <class Stream>
    void read(Stream& stream, std::size_t num_messages, error_code& ec)
    {
        discard_processed_messages();
        while (num_cached_messages() < num_messages)
        {
            std::size_t bytes_read = stream.read_some(buffer_.free_area(), ec);
            if (ec) break;
            valgrind_make_mem_defined(buffer_.free_first(), bytes_read);
            ec = on_read_bytes(bytes_read);
            if (ec) break;
        }
    }

    template <class Stream, class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_read(Stream& stream, std::size_t num_messages, CompletionToken&& token)
    {
        return boost::asio::async_compose<CompletionToken, void(error_code)>(
            read_op(num_messages, *this, stream),
            token,
            stream
        );
    }

private:

    template <class Stream>
    struct read_op : boost::asio::coroutine
    {
        std::size_t num_messages_;
        message_reader& reader_;
        Stream& stream_;

        read_op(std::size_t num_messages, message_reader& reader, Stream& stream) noexcept :
            num_messages_(num_messages),
            reader_(reader),
            stream_(stream)
        {
        }

        template<class Self>
        void operator()(
            Self& self,
            error_code ec = {},
            std::size_t bytes_read = 0
        )
        {
            // Error handling
            if (ec)
            {
                self.complete(ec);
                return;
            }

            // Non-error path
            BOOST_ASIO_CORO_REENTER(*this)
            {
                reader_.discard_processed_messages();
                assert(reader_.num_cached_messages() < num_messages_);
                while (reader_.num_cached_messages() < num_messages_)
                {
                    BOOST_ASIO_CORO_YIELD stream_.async_read_some(
                        reader_.buffer_.free_area(),
                        std::move(*this)    
                    );
                    valgrind_make_mem_defined(reader_.buffer_.free_first(), bytes_read);
                    ec = reader_.on_read_bytes(bytes_read);
                    if (ec) self.complete(ec);
                }

                // TODO: post if messages are cached

                self.complete(error_code());
            }
        }
    };

    struct message
    {
        std::size_t first; // Offset in the processing area
        std::size_t length;
        std::uint8_t seqnum_first;
        std::uint8_t seqnum_last;

        std::size_t last() const noexcept { return first + length; }
    };

    struct state_t
    {
        bool is_first_frame_ {true};
        std::uint8_t first_seqnum_ {};
        std::uint8_t last_seqnum_ {};
        bool reading_header_ {false};
        std::size_t remaining_bytes_ {0};
        bool more_frames_follow_ {false};
        std::size_t total_length_ {};
    };

    read_buffer buffer_;
    std::vector<message> cached_messages_;
    std::size_t message_cursor_;
    state_t state_;

    void discard_processed_messages()
    {
        if (message_cursor_ != 0)
        {
            std::size_t bytes_to_remove = cached_messages_[message_cursor_ - 1].last();
            buffer_.remove_from_processing(0, bytes_to_remove);
            cached_messages_.erase(cached_messages_.begin(), cached_messages_.begin() + message_cursor_);
            message_cursor_ = 0;
            for (auto& msg : cached_messages_)
            {
                msg.first -= bytes_to_remove;
            }
        }
        
    }

    error_code on_read_bytes(size_t num_bytes)
    {
        buffer_.move_to_processing(num_bytes);
        while (true)
        {
            if (state_.reading_header_)
            {
                // If there are not enough bytes to process a header, request more
                if (buffer_.processing_size() < HEADER_SIZE)
                {
                    buffer_.grow_to_fit(HEADER_SIZE);
                    return error_code();
                }

                // Deserialize the header
                packet_header header;
                deserialization_context ctx (
                    boost::asio::buffer(buffer_.processing_first(), HEADER_SIZE),
                    capabilities(0) // unaffected by capabilities
                );
                errc err = deserialize(ctx, header);
                if (err != errc::ok)
                {
                    return make_error_code(err);
                }

                // Process the sequence number
                if (state_.is_first_frame_)
                {
                    state_.is_first_frame_ = false;
                    state_.first_seqnum_ = header.sequence_number;
                    state_.last_seqnum_ = header.sequence_number;
                }
                else
                {
                    std::uint8_t expected_seqnum = state_.last_seqnum_ + 1;
                    if (header.sequence_number != expected_seqnum)
                    {
                        return make_error_code(errc::sequence_number_mismatch);
                    }
                    state_.last_seqnum_ = expected_seqnum;
                }

                // Process the packet size
                state_.remaining_bytes_ = header.packet_size.value;
                state_.more_frames_follow_ = (state_.remaining_bytes_ == MAX_PACKET_SIZE);

                // We are done with the header
                buffer_.remove_from_processing(0, HEADER_SIZE);
                state_.reading_header_ = false;
            }

            if (!state_.reading_header_)
            {
                // Update the state
                std::size_t new_bytes = std::min(buffer_.processing_size(), state_.remaining_bytes_);
                state_.remaining_bytes_ -= new_bytes;
                state_.total_length_ += new_bytes;
                if (state_.remaining_bytes_ == 0)
                {
                    state_.reading_header_ = true;
                }

                // If we still have remaining bytes, make space for them
                if (state_.remaining_bytes_)
                {
                    buffer_.grow_to_fit(state_.remaining_bytes_);
                    return error_code();
                }

                // If we fully read a message, track it
                if (!state_.remaining_bytes_ && !state_.more_frames_follow_)
                {
                    std::size_t offset = cached_messages_.empty() ?
                        buffer_.dead_size() : cached_messages_.back().last();
                    cached_messages_.push_back(message{
                        offset,
                        state_.total_length_,
                        state_.first_seqnum_,
                        state_.last_seqnum_
                    });
                    state_ = state_t();
                }
            }
        }
    }
};


} // detail
} // mysql
} // boost


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_STATIC_STRING_HPP_ */





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
#include <cstddef>
#include <cstdint>


namespace boost {
namespace mysql {
namespace detail {

class message_reader
{
public:
    message_reader(std::size_t initial_buffer_size) :
        buffer_(initial_buffer_size)
    {
    }

    bool keep_messages() const noexcept { return keep_messages_; }
    void set_keep_messages(bool v) noexcept { keep_messages_ = v; }

    bool has_message() const noexcept { return result_type_ == result_type::message; }
    const std::uint8_t* buffer_first() const noexcept { return buffer_.reserved_first(); }

    boost::asio::const_buffer get_next_message(std::uint8_t& seqnum, error_code& ec) noexcept
    {
        assert(has_message());
        if (seqnum != message_.seqnum_first)
        {
            ec = make_error_code(errc::sequence_number_mismatch);
            return {};
        }
        seqnum = message_.seqnum_last + 1;
        auto res = buffer_.current_message();
        buffer_.move_to_reserved(buffer_.current_message_size());
        error_code next_ec = process_message();
        if (next_ec)
        {
            result_type_ = result_type::error;
            next_error_ = next_ec;
        }
        return res;
    }

    // Reads some messages from stream, until there is at least one
    template <class Stream>
    void read_some(Stream& stream, error_code& ec)
    {
        // If we already have a message, complete immediately
        if (has_message())
            return;
        
        // Remove processed messages if we can
        maybe_remove_processed_messages();

        while (!has_message())
        {
            // If any previous process_message indicated that we need more
            // buffer space, resize the buffer now
            maybe_resize_buffer();

            // Actually read bytes
            std::size_t bytes_read = stream.read_some(buffer_.free_area(), ec);
            if (ec) break;
            valgrind_make_mem_defined(buffer_.free_first(), bytes_read);

            // Process them
            ec = on_read_bytes(bytes_read);
            if (ec) break;
        }
    }

    template <class Stream, class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_read_some(Stream& stream, CompletionToken&& token)
    {
        return boost::asio::async_compose<CompletionToken, void(error_code)>(
            read_op(*this, stream),
            token,
            stream
        );
    }

private:

    template <class Stream>
    struct read_op : boost::asio::coroutine
    {
        message_reader& reader_;
        Stream& stream_;

        read_op(message_reader& reader, Stream& stream) noexcept :
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
                // If we already have a message, complete immediately
                if (reader_.has_message())
                {
                    BOOST_ASIO_CORO_YIELD boost::asio::post(std::move(self));
                    self.complete(error_code());
                    BOOST_ASIO_CORO_YIELD break;
                }

                // Remove processed messages if we can
                reader_.maybe_remove_processed_messages();

                while (!reader_.has_message())
                {
                    // If any previous process_message indicated that we need more
                    // buffer space, resize the buffer now
                    reader_.maybe_resize_buffer();

                    // Actually read bytes
                    BOOST_ASIO_CORO_YIELD stream_.async_read_some(
                        reader_.buffer_.free_area(),
                        std::move(*this)    
                    );
                    valgrind_make_mem_defined(reader_.buffer_.free_first(), bytes_read);

                    // Process them
                    ec = reader_.on_read_bytes(bytes_read);
                    if (ec)
                    {
                        self.complete(ec);
                        BOOST_ASIO_CORO_YIELD break;
                    }
                }

                self.complete(error_code());
            }
        }
    };

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
        bool is_first_frame_ {true};
        std::uint8_t first_seqnum_ {};
        std::uint8_t last_seqnum_ {};
        bool reading_header_ {false};
        std::size_t remaining_bytes_ {0};
        bool more_frames_follow_ {false};
        std::size_t grow_buffer_to_fit_ {};
    };

    read_buffer buffer_;
    bool keep_messages_ {false};

    // Union-like; the result produced by process_message may be
    // either a state (representing an incomplete message), a message
    // (representing a complete message) or an error_code
    result_type result_type_ {result_type::state};
    state_t state_;
    message_t message_;
    error_code next_error_;

    error_code process_message()
    {
        // If we have a message, the caller has already read the previous message
        // and we need to parse another one. Reset the state
        // If the last operation indicated an error, clear it in hope we can recover from it
        if (result_type_ != result_type::state)
        {
            result_type_ = result_type::state;
            state_ = state_t();
        }

        while (true)
        {
            if (state_.reading_header_)
            {
                // If there are not enough bytes to process a header, request more
                if (buffer_.pending_size() < HEADER_SIZE)
                {
                    state_.grow_buffer_to_fit_ = HEADER_SIZE;
                    return error_code();
                }

                // Mark the header as belonging to the current message
                buffer_.move_to_current_message(HEADER_SIZE);

                // Deserialize the header
                packet_header header;
                deserialization_context ctx (
                    boost::asio::buffer(buffer_.pending_first(), HEADER_SIZE),
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
                if (state_.is_first_frame_)
                {
                    // If it's the 1st frame, we can just move the header bytes to the reserved
                    // area, avoiding a big memmove
                    buffer_.move_to_reserved(HEADER_SIZE);
                }
                else
                {
                    buffer_.remove_current_message_last(HEADER_SIZE);
                }
                state_.reading_header_ = false;
            }

            if (!state_.reading_header_)
            {
                // Get the number of bytes belonging to this message
                std::size_t new_bytes = std::min(buffer_.pending_size(), state_.remaining_bytes_);

                // Mark them as belonging to the current message in the buffer
                buffer_.move_to_current_message(new_bytes);

                // Update remaining bytes
                state_.remaining_bytes_ -= new_bytes;
                if (state_.remaining_bytes_ == 0)
                {
                    state_.reading_header_ = true;
                }
                else
                {
                    state_.grow_buffer_to_fit_ = state_.remaining_bytes_;
                    return error_code();
                }

                // If we've fully read a message, we're done
                if (!state_.remaining_bytes_ && !state_.more_frames_follow_)
                {
                    result_type_ = result_type::message;
                    message_ = message_t {
                        state_.first_seqnum_,
                        state_.last_seqnum_
                    };
                    return error_code();
                }
            }
        }
    }

    void maybe_remove_processed_messages()
    {
        if (!keep_messages_)
        {
            buffer_.remove_reserved();
        }
    }

    void maybe_resize_buffer()
    {
        if (result_type_ == result_type::state && state_.grow_buffer_to_fit_ != 0)
        {
            buffer_.grow_to_fit(state_.grow_buffer_to_fit_);
            state_.grow_buffer_to_fit_ = 0;
        }
    }

    error_code on_read_bytes(size_t num_bytes)
    {
        buffer_.move_to_pending(num_bytes);
        return process_message();
    }
};


} // detail
} // mysql
} // boost


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_STATIC_STRING_HPP_ */





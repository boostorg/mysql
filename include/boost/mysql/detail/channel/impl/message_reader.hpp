//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CHANNEL_IMPL_MESSAGE_READER_HPP
#define BOOST_MYSQL_DETAIL_CHANNEL_IMPL_MESSAGE_READER_HPP

#pragma once

#include <boost/mysql/detail/channel/message_reader.hpp>


boost::asio::const_buffer boost::mysql::detail::message_reader::get_next_message(
    std::uint8_t& seqnum,
    error_code& ec
) noexcept
{
    assert(has_message());
    if (seqnum != result_.message.seqnum_first)
    {
        ec = make_error_code(errc::sequence_number_mismatch);
        return {};
    }
    seqnum = result_.message.seqnum_last + 1;
    auto res = buffer_.current_message();
    buffer_.move_to_reserved(buffer_.current_message_size());
    error_code next_ec = process_message();
    if (next_ec)
    {
        result_.type = result_type::error;
        result_.next_error = next_ec;
    }
    return res;
}

template <class Stream>
void boost::mysql::detail::message_reader::read_some(
    Stream& stream,
    error_code& ec
)
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

template <class Stream>
struct boost::mysql::detail::message_reader::read_op : boost::asio::coroutine
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

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(::boost::mysql::error_code))
boost::mysql::detail::message_reader::async_read_some(
    Stream& stream,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code)>(
        read_op(*this, stream),
        token,
        stream
    );
}


boost::mysql::error_code boost::mysql::detail::message_reader::process_message()
{
    // If we have a message, the caller has already read the previous message
    // and we need to parse another one. Reset the state
    // If the last operation indicated an error, clear it in hope we can recover from it
    if (!result_.is_state())
    {
        result_ = result_t();
    }

    while (true)
    {
        if (result_.state.reading_header)
        {
            // If there are not enough bytes to process a header, request more
            if (buffer_.pending_size() < HEADER_SIZE)
            {
                result_.state.grow_buffer_to_fit = HEADER_SIZE;
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
            if (result_.state.is_first_frame)
            {
                result_.state.is_first_frame = false;
                result_.state.first_seqnum = header.sequence_number;
                result_.state.last_seqnum = header.sequence_number;
            }
            else
            {
                std::uint8_t expected_seqnum = result_.state.last_seqnum + 1;
                if (header.sequence_number != expected_seqnum)
                {
                    return make_error_code(errc::sequence_number_mismatch);
                }
                result_.state.last_seqnum = expected_seqnum;
            }

            // Process the packet size
            result_.state.remaining_bytes = header.packet_size.value;
            result_.state.more_frames_follow = (result_.state.remaining_bytes == MAX_PACKET_SIZE);

            // We are done with the header
            if (result_.state.is_first_frame)
            {
                // If it's the 1st frame, we can just move the header bytes to the reserved
                // area, avoiding a big memmove
                buffer_.move_to_reserved(HEADER_SIZE);
            }
            else
            {
                buffer_.remove_current_message_last(HEADER_SIZE);
            }
            result_.state.reading_header = false;
        }

        if (!result_.state.reading_header)
        {
            // Get the number of bytes belonging to this message
            std::size_t new_bytes = std::min(buffer_.pending_size(), result_.state.remaining_bytes);

            // Mark them as belonging to the current message in the buffer
            buffer_.move_to_current_message(new_bytes);

            // Update remaining bytes
            result_.state.remaining_bytes -= new_bytes;
            if (result_.state.remaining_bytes == 0)
            {
                result_.state.reading_header = true;
            }
            else
            {
                result_.state.grow_buffer_to_fit = result_.state.remaining_bytes;
                return error_code();
            }

            // If we've fully read a message, we're done
            if (!result_.state.remaining_bytes && !result_.state.more_frames_follow)
            {
                result_.type = result_type::message;
                result_.message = message_t {
                    result_.state.first_seqnum,
                    result_.state.last_seqnum
                };
                return error_code();
            }
        }
    }
}


void boost::mysql::detail::message_reader::maybe_remove_processed_messages()
{
    if (!keep_messages_)
    {
        buffer_.remove_reserved();
    }
}

void boost::mysql::detail::message_reader::maybe_resize_buffer()
{
    if (result_.is_state() && result_.state.grow_buffer_to_fit != 0)
    {
        buffer_.grow_to_fit(result_.state.grow_buffer_to_fit);
        result_.state.grow_buffer_to_fit = 0;
    }
}

boost::mysql::error_code boost::mysql::detail::message_reader::on_read_bytes(size_t num_bytes)
{
    buffer_.move_to_pending(num_bytes);
    return process_message();
}

#endif

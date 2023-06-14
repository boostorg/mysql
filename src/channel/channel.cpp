//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "channel/channel.hpp"

#include <boost/mysql/client_errc.hpp>

#include <boost/mysql/detail/any_stream_impl.hpp>
#include <boost/mysql/detail/channel_ptr.hpp>

#include <cstddef>

#include "channel/message_parser.hpp"
#include "channel/message_reader.hpp"
#include "protocol/protocol.hpp"

boost::mysql::detail::channel_ptr::channel_ptr(std::size_t read_buff_size, std::unique_ptr<any_stream> stream)
    : chan_(new channel(read_buff_size, std::move(stream)))
{
}

boost::mysql::detail::channel_ptr::channel_ptr(channel_ptr&& rhs) noexcept : chan_(std::move(rhs.chan_)) {}

boost::mysql::detail::channel_ptr& boost::mysql::detail::channel_ptr::operator=(channel_ptr&& rhs) noexcept
{
    chan_ = std::move(rhs.chan_);
    return *this;
}

boost::mysql::detail::channel_ptr::~channel_ptr() {}

const boost::mysql::detail::any_stream& boost::mysql::detail::channel_ptr::get_stream() const
{
    return chan_->stream();
}

boost::mysql::metadata_mode boost::mysql::detail::channel_ptr::meta_mode() const noexcept
{
    return chan_->meta_mode();
}

void boost::mysql::detail::channel_ptr::set_meta_mode(metadata_mode v) noexcept { chan_->set_meta_mode(v); }

template class boost::mysql::detail::any_stream_impl<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>;
template class boost::mysql::detail::any_stream_impl<boost::asio::ip::tcp::socket>;

void boost::mysql::detail::message_parser::parse_message(read_buffer& buff, result& res) noexcept
{
    while (true)
    {
        if (state_.reading_header)
        {
            // If there are not enough bytes to process a header, request more
            if (buff.pending_size() < HEADER_SIZE)
            {
                res.set_required_size(HEADER_SIZE - buff.pending_size());
                return;
            }

            // Mark the header as belonging to the current message
            buff.move_to_current_message(HEADER_SIZE);

            // Deserialize the header
            auto header = deserialize_frame_header(
                span<const std::uint8_t, frame_header_size>(buff.pending_first() - HEADER_SIZE, HEADER_SIZE)
            );

            // Process the sequence number
            if (state_.is_first_frame)
            {
                state_.seqnum_first = header.sequence_number;
                state_.seqnum_last = header.sequence_number;
            }
            else
            {
                std::uint8_t expected_seqnum = state_.seqnum_last + 1;
                if (header.sequence_number != expected_seqnum)
                {
                    state_.has_seqnum_mismatch = true;
                }
                state_.seqnum_last = expected_seqnum;
            }

            // Process the packet size
            state_.remaining_bytes = header.size;
            state_.more_frames_follow = (state_.remaining_bytes == max_frame_size_);

            // We are done with the header
            if (state_.is_first_frame)
            {
                // If it's the 1st frame, we can just move the header bytes to the reserved
                // area, avoiding a big memmove
                buff.move_to_reserved(HEADER_SIZE);
            }
            else
            {
                buff.remove_current_message_last(HEADER_SIZE);
            }
            state_.is_first_frame = false;
            state_.reading_header = false;
        }

        if (!state_.reading_header)
        {
            // Get the number of bytes belonging to this message
            std::size_t new_bytes = (std::min)(buff.pending_size(), state_.remaining_bytes);

            // Mark them as belonging to the current message in the buffer
            buff.move_to_current_message(new_bytes);

            // Update remaining bytes
            state_.remaining_bytes -= new_bytes;
            if (state_.remaining_bytes == 0)
            {
                state_.reading_header = true;
            }
            else
            {
                res.set_required_size(state_.remaining_bytes);
                return;
            }

            // If we've fully read a message, we're done
            if (!state_.remaining_bytes && !state_.more_frames_follow)
            {
                std::size_t message_size = buff.current_message_size();
                buff.move_to_reserved(message_size);
                res.set_message({
                    state_.seqnum_first,
                    state_.seqnum_last,
                    message_size,
                    state_.has_seqnum_mismatch,
                });
                state_ = state_t();
                return;
            }
        }
    }
}

boost::span<const std::uint8_t> boost::mysql::detail::message_reader::get_next_message(
    std::uint8_t& seqnum,
    error_code& ec
) noexcept
{
    BOOST_ASSERT(has_message());
    if (result_.message.has_seqnum_mismatch || seqnum != result_.message.seqnum_first)
    {
        ec = make_error_code(client_errc::sequence_number_mismatch);
        return {};
    }
    seqnum = result_.message.seqnum_last + 1;
    span<const std::uint8_t> res(
        buffer_.current_message_first() - result_.message.size,
        result_.message.size
    );
    parse_message();
    ec = error_code();
    return res;
}
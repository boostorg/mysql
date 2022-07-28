//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_CHANNEL_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_CHANNEL_HPP

#include <boost/asio/buffer.hpp>
#include <boost/asio/coroutine.hpp>
#pragma once

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/compose.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <utility>
#include <cstdint>


namespace boost {
namespace mysql {
namespace detail {

inline std::uint32_t compute_size_to_write(
    std::size_t buffer_size,
    std::size_t transferred_size
)
{
    return static_cast<std::uint32_t>(std::min(
        MAX_PACKET_SIZE,
        buffer_size - transferred_size
    ));
}


template<class Stream>
struct boost::mysql::detail::channel<Stream>::read_one_op
    : boost::asio::coroutine
{
    channel<Stream>& chan_;
    std::uint8_t& seqnum_;

    read_one_op(
        channel<Stream>& chan,
        std::uint8_t& seqnum
    ) :
        chan_(chan),
        seqnum_(seqnum)
    {
    }

    template<class Self>
    void operator()(
        Self& self,
        error_code code = {}
    )
    {
        // Error handling
        if (code)
        {
            self.complete(code);
            return;
        }

        // Non-error path
        boost::asio::const_buffer b;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            BOOST_ASIO_CORO_YIELD chan_.async_read(1, std::move(self));
            b = chan_.next_read_message(seqnum_, code);
            self.complete(code, b);
        }
    }
};


} // detail
} // mysql
} // boost


template <class Stream>
void boost::mysql::detail::channel<Stream>::process_header_write(
    std::uint32_t size_to_write,
    std::uint8_t seqnum
)
{
    packet_header header;
    header.packet_size.value = size_to_write;
    header.sequence_number = seqnum;
    serialization_context ctx (capabilities(0), header_buffer_.data()); // capabilities not relevant here
    serialize(ctx, header);
}


template <class Stream>
void boost::mysql::detail::channel<Stream>::write(
    boost::asio::const_buffer buffer,
    std::uint8_t& seqnum,
    error_code& code
)
{
    std::size_t transferred_size = 0;
    auto bufsize = buffer.size();
    auto first = static_cast<const std::uint8_t*>(buffer.data());

    // If the packet is empty, we should still write the header, saying
    // we are sending an empty packet.
    do
    {
        auto size_to_write = compute_size_to_write(bufsize, transferred_size);
        process_header_write(size_to_write, seqnum++);
        boost::asio::write(
            stream_,
            std::array<boost::asio::const_buffer, 2> {{
                boost::asio::buffer(header_buffer_),
                boost::asio::buffer(first + transferred_size, size_to_write)
            }},
            code
        );
        if (code)
            return;
        transferred_size += size_to_write;
    } while (transferred_size < bufsize);
}


template<class Stream>
struct boost::mysql::detail::channel<Stream>::write_op
    : boost::asio::coroutine
{
    channel<Stream>& chan_;
    boost::asio::const_buffer buffer_;
    std::uint8_t& seqnum_;
    std::size_t total_transferred_size_ = 0;

    write_op(
        channel<Stream>& chan,
        boost::asio::const_buffer buffer,
        std::uint8_t& seqnum
    ) :
        chan_(chan),
        buffer_(buffer),
        seqnum_(seqnum)
    {
    }

    template<class Self>
    void operator()(
        Self& self,
        error_code code = {},
        std::size_t bytes_transferred = 0
    )
    {
        // Error handling
        if (code)
        {
            self.complete(code);
            return;
        }

        // Non-error path
        std::uint32_t size_to_write;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // Force write the packet header on an empty packet, at least.
            do
            {
                size_to_write = compute_size_to_write(buffer_.size(), total_transferred_size_);
                chan_.process_header_write(size_to_write, seqnum_++);

                BOOST_ASIO_CORO_YIELD chan_.stream_.async_write(
                    std::array<boost::asio::const_buffer, 2> {{
                        boost::asio::buffer(chan_.header_buffer_),
                        boost::asio::buffer(buffer_ + total_transferred_size_, size_to_write)
                    }},
                    std::move(self)
                );

                total_transferred_size_ += (bytes_transferred - 4); // header size

            } while (total_transferred_size_ < buffer_.size());

            self.complete(error_code());
        }
    }
};

template <class Stream>
template <class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::detail::channel<Stream>::async_write(
    boost::asio::const_buffer buffer,
    std::uint8_t& seqnum,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code)>(
        write_op(*this, buffer, seqnum),
        token,
        *this
    );
}

template <class Stream>
template <class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code, ::boost::asio::const_buffer)
)
boost::mysql::detail::channel<Stream>::async_read_one(
    std::uint8_t& seqnum,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code)>(
        read_one_op(*this, seqnum),
        token,
        *this
    );
}

template <class Stream>
boost::mysql::error_code boost::mysql::detail::channel<Stream>::close()
{
    error_code err;
    lowest_layer().shutdown(lowest_layer_type::shutdown_both, err);
    lowest_layer().close(err);
    return err;
}


#endif

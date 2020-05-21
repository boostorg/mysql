//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_CHANNEL_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_CHANNEL_HPP

#include "boost/mysql/error.hpp"
#include "boost/mysql/detail/auxiliar/bytestring.hpp"
#include "boost/mysql/detail/protocol/capabilities.hpp"
#include <boost/asio/buffer.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/beast/core/async_base.hpp>
#include <array>
#include <optional>

namespace boost {
namespace mysql {
namespace detail {

// Implements the message layer of the MySQL protocol
template <typename Stream>
class channel
{
    // TODO: static asserts for Stream concept
    struct ssl_block
    {
        boost::asio::ssl::context ctx;
        boost::asio::ssl::stream<Stream&> stream;

        ssl_block(Stream& base_stream):
            ctx(boost::asio::ssl::context::tls_client),
            stream (base_stream, ctx) {}
    };

    Stream& stream_;
    std::optional<ssl_block> ssl_block_;
    std::uint8_t sequence_number_ {0};
    std::array<std::uint8_t, 4> header_buffer_ {}; // for async ops
    bytestring shared_buff_; // for async ops
    capabilities current_caps_;

    bool process_sequence_number(std::uint8_t got);
    std::uint8_t next_sequence_number() { return sequence_number_++; }

    error_code process_header_read(std::uint32_t& size_to_read); // reads from header_buffer_
    void process_header_write(std::uint32_t size_to_write); // writes to header_buffer_

    void create_ssl_block() { ssl_block_.emplace(stream_); }

    template <typename BufferSeq>
    std::size_t read_impl(BufferSeq&& buff, error_code& ec);

    template <typename BufferSeq>
    std::size_t write_impl(BufferSeq&& buff, error_code& ec);

    template <typename BufferSeq, typename CompletionToken>
    auto async_read_impl(BufferSeq&& buff, CompletionToken&& token);

    template <typename BufferSeq, typename CompletionToken>
    auto async_write_impl(BufferSeq&& buff, CompletionToken&& token);

    struct read_op;
    struct write_op;
public:
    using executor_type = typename Stream::executor_type;

    channel(Stream& stream): stream_(stream) {}

    executor_type get_executor() {return stream_.get_executor();}

    // Reading
    void read(bytestring& buffer, error_code& code);

    using read_signature = void(error_code);

    template <typename CompletionToken>
    BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, read_signature)
    async_read(bytestring& buffer, CompletionToken&& token);

    // Writing
    void write(boost::asio::const_buffer buffer, error_code& code);

    using write_signature = void(error_code);

    template <typename CompletionToken>
    BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, write_signature)
    async_write(boost::asio::const_buffer buffer, CompletionToken&& token);

    // SSL
    bool ssl_active() const noexcept { return ssl_block_.has_value(); }

    void ssl_handshake(error_code& ec);

    using ssl_handshake_signature = void(error_code);

    template <typename CompletionToken>
    BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, ssl_handshake_signature)
    async_ssl_handshake(CompletionToken&& token);

    // Closing (only available for sockets)
    error_code close();

    // Sequence numbers
    void reset_sequence_number(std::uint8_t value = 0) { sequence_number_ = value; }
    std::uint8_t sequence_number() const { return sequence_number_; }

    // Getting the underlying stream
    using stream_type = Stream;
    stream_type& next_layer() { return stream_; }

    // Capabilities
    capabilities current_capabilities() const noexcept { return current_caps_; }
    void set_current_capabilities(capabilities value) noexcept { current_caps_ = value; }

    // Internal buffer
    const bytestring& shared_buffer() const noexcept { return shared_buff_; }
    bytestring& shared_buffer() noexcept { return shared_buff_; }
};

template <
    typename Stream,
    typename CompletionToken,
    typename HandlerSignature
>
using async_op_base = boost::beast::async_base<
    BOOST_ASIO_HANDLER_TYPE(CompletionToken, HandlerSignature),
    typename Stream::executor_type
>;

// The base for async operations involving a channel
// Defined here so it can also be used for channel function implementation
template <
    typename Stream
>
class async_op :
    public boost::asio::coroutine
{
    channel<Stream>& channel_;
    error_info* output_info_;

//    using base_type = async_op_base<Stream, CompletionToken, HandlerSignature>;
public:

    async_op(channel<Stream>& chan, error_info* output_info) :
        channel_(chan),
        output_info_(output_info)
    {
    }

    channel<Stream>& get_channel() noexcept { return channel_; }
    error_info* get_output_info() noexcept { return output_info_; }

     // Reads from channel against the channel internal buffer, using itself as
    template<class Self>
    void async_read(Self&& self) { async_read(std::move(self), channel_.shared_buffer()); }

    template<class Self>
    void async_read(Self&& self, bytestring& buff)
    {
        channel_.async_read(
            buff,
            std::move(self)
        );
    }

    template<class Self>
    void async_write(Self&& self, const bytestring& buff)
    {
        channel_.async_write(
            boost::asio::buffer(buff),
            std::move(self)
        );
    }

    template <class Self>
    void async_write(Self&& self) { async_write(std::move(self), channel_.shared_buffer()); }
};


} // detail
} // mysql
} // boost

#include "boost/mysql/detail/protocol/impl/channel.hpp"

#endif

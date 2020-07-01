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
#include <boost/optional/optional.hpp>
#include <array>

namespace boost {
namespace mysql {
namespace detail {

// Implements the message layer of the MySQL protocol
template <class Stream>
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

    Stream stream_;
    boost::optional<ssl_block> ssl_block_;
    std::uint8_t sequence_number_ {0};
    std::array<std::uint8_t, 4> header_buffer_ {}; // for async ops
    bytestring shared_buff_; // for async ops
    capabilities current_caps_;
    error_info shared_info_; // for async ops

    bool process_sequence_number(std::uint8_t got);
    std::uint8_t next_sequence_number() { return sequence_number_++; }

    error_code process_header_read(std::uint32_t& size_to_read); // reads from header_buffer_
    void process_header_write(std::uint32_t size_to_write); // writes to header_buffer_

    void create_ssl_block() { ssl_block_.emplace(stream_); }

    template <class BufferSeq>
    std::size_t read_impl(BufferSeq&& buff, error_code& ec);

    template <class BufferSeq>
    std::size_t write_impl(BufferSeq&& buff, error_code& ec);

    template <class BufferSeq, class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, std::size_t))
    async_read_impl(BufferSeq&& buff, CompletionToken&& token);

    template <class BufferSeq, class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, std::size_t))
    async_write_impl(BufferSeq&& buff, CompletionToken&& token);

    struct read_op;
    struct write_op;
public:
    template <class... Args>
    channel(Args&&... args): stream_(std::forward<Args>(args)...) {}

    // Executor
    using executor_type = typename Stream::executor_type;
    executor_type get_executor() { return stream_.get_executor(); }

    // Reading
    void read(bytestring& buffer, error_code& code);

    template <class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_read(bytestring& buffer, CompletionToken&& token);

    // Writing
    void write(boost::asio::const_buffer buffer, error_code& code);
    void write(const bytestring& buffer, error_code& code)
    {
        write(boost::asio::buffer(buffer), code);
    }

    template <class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_write(boost::asio::const_buffer buffer, CompletionToken&& token);

    template <class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_write(const bytestring& buffer, CompletionToken&& token)
    {
        return async_write(boost::asio::buffer(buffer), std::forward<CompletionToken>(token));
    }

    // SSL
    bool ssl_active() const noexcept { return ssl_block_.has_value(); }

    void ssl_handshake(error_code& ec);

    template <class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
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

    // Internal buffer & error_info to help async ops
    const bytestring& shared_buffer() const noexcept { return shared_buff_; }
    bytestring& shared_buffer() noexcept { return shared_buff_; }
    error_info& shared_info() noexcept { return shared_info_; }
};

} // detail
} // mysql
} // boost

#include "boost/mysql/detail/protocol/impl/channel.hpp"

#endif

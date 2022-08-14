//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CHANNEL_CHANNEL_HPP
#define BOOST_MYSQL_DETAIL_CHANNEL_CHANNEL_HPP

#include <boost/asio/buffer.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/detail/auxiliar/bytestring.hpp>
#include <boost/mysql/detail/protocol/capabilities.hpp>
#include <boost/mysql/detail/channel/disableable_ssl_stream.hpp>
#include <boost/mysql/detail/channel/message_reader.hpp>
#include <array>
#include <cstddef>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {


// Implements the message layer of the MySQL protocol
template <class Stream>
class channel
{
    disableable_ssl_stream<Stream> stream_;
    capabilities current_caps_;
    message_reader reader_;
    std::array<std::uint8_t, 4> header_buffer_ {}; // for async ops
    std::uint8_t shared_sequence_number_ {}; // for async ops
    bytestring shared_buff_; // for async ops
    error_info shared_info_; // for async ops
    std::vector<field_view> shared_fields_; // for read_some ops

    void process_header_write(std::uint32_t size_to_write, std::uint8_t seqnum); // writes to header_buffer_

    struct read_one_op;
    struct write_op;
public:
    channel() = default; // Simplify life if stream is default constructible, mainly for tests

    template <class... Args>
    channel(Args&&... args) : 
        stream_(std::forward<Args>(args)...) 
    {
    }

    void reset()
    {
        stream_.set_ssl_active(false);
    }

    // Executor
    using executor_type = typename Stream::executor_type;
    executor_type get_executor() { return stream_.get_executor(); }

    // Reading
    const std::uint8_t* buffer_first() const noexcept { return reader_.buffer_first(); }
    bool has_read_messages() const noexcept { return reader_.has_message(); }
    boost::asio::const_buffer next_read_message(std::uint8_t& seqnum, error_code& err) noexcept
    {
        return reader_.get_next_message(seqnum, err);
    }

    void read_some(error_code& code) { return reader_.read_some(stream_, code); }

    template <class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_read_some(CompletionToken&& token)
    {
        return reader_.async_read_some(stream_, std::forward<CompletionToken>(token));
    }

    boost::asio::const_buffer read_one(std::uint8_t& seqnum, error_code& ec)
    {
        ec = reader_.read_some(stream_, ec);
        if (ec)
            return {};
        return next_read_message(seqnum, ec);
    }

    template <class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, ::boost::asio::const_buffer))
    async_read_one(std::uint8_t& seqnum, CompletionToken&& token);

    bool keep_messages() const noexcept { return reader_.keep_messages(); }
    void set_keep_messages(bool v) noexcept { reader_.set_keep_messages(v); }

    // Writing
    void write(boost::asio::const_buffer buffer, std::uint8_t& seqnum, error_code& code);
    void write(const bytestring& buffer, std::uint8_t& seqnum, error_code& code)
    {
        write(boost::asio::buffer(buffer), seqnum, code);
    }

    template <class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_write(boost::asio::const_buffer buffer, std::uint8_t& seqnum, CompletionToken&& token);

    template <class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_write(const bytestring& buffer, CompletionToken&& token)
    {
        return async_write(boost::asio::buffer(buffer), std::forward<CompletionToken>(token));
    }

    // SSL
    bool ssl_active() const noexcept { return stream_.ssl_active(); }
    void set_ssl_active(bool v) noexcept { stream_.set_ssl_active(v); }

    // Closing (only available for sockets)
    error_code close();

    // Getting the underlying stream
    using stream_type = Stream;
    stream_type& next_layer() noexcept { return stream_.next_layer(); }
    const stream_type& next_layer() const noexcept { return stream_.next_layer(); }

    using lowest_layer_type = typename stream_type::lowest_layer_type;
    lowest_layer_type& lowest_layer() noexcept { return stream_.lowest_layer(); }

    // Capabilities
    capabilities current_capabilities() const noexcept { return current_caps_; }
    void set_current_capabilities(capabilities value) noexcept { current_caps_ = value; }

    // Internal buffer, error_info and sequence_number to help async ops
    const bytestring& shared_buffer() const noexcept { return shared_buff_; }
    bytestring& shared_buffer() noexcept { return shared_buff_; }
    error_info& shared_info() noexcept { return shared_info_; }
    std::uint8_t& shared_sequence_number() noexcept { return shared_sequence_number_; }
    std::vector<field_view>& shared_fields() noexcept { return shared_fields_; }
    const std::vector<field_view>& shared_fields() const noexcept { return shared_fields_; }
};

} // detail
} // mysql
} // boost

#include <boost/mysql/detail/channel/impl/channel.hpp>

#endif

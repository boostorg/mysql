//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CHANNEL_CHANNEL_HPP
#define BOOST_MYSQL_DETAIL_CHANNEL_CHANNEL_HPP

#include <boost/mysql/detail/auxiliar/bytestring.hpp>
#include <boost/mysql/detail/channel/disableable_ssl_stream.hpp>
#include <boost/mysql/detail/channel/message_reader.hpp>
#include <boost/mysql/detail/channel/message_writer.hpp>
#include <boost/mysql/detail/protocol/capabilities.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/field_view.hpp>

#include <boost/asio/async_result.hpp>
#include <boost/asio/buffer.hpp>

#include <cstddef>
#include <utility>
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
    message_writer writer_;

    std::uint8_t shared_sequence_number_{};  // for async ops
    bytestring shared_buff_;                 // for async ops
    error_info shared_info_;                 // for async ops
    std::vector<field_view> shared_fields_;  // for read_some ops
public:
    // TODO: use this arg
    template <class... Args>
    channel(std::size_t read_buffer_size, Args&&... args)
        : stream_(std::forward<Args>(args)...), reader_(read_buffer_size)
    {
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

    void read_some(error_code& code, bool keep_messages = false)
    {
        return reader_.read_some(stream_, code, keep_messages);
    }

    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_read_some(CompletionToken&& token, bool keep_messages = false)
    {
        return reader_
            .async_read_some(stream_, std::forward<CompletionToken>(token), keep_messages);
    }

    boost::asio::const_buffer read_one(
        std::uint8_t& seqnum,
        error_code& ec,
        bool keep_messages = false
    )
    {
        return reader_.read_one(stream_, seqnum, ec, keep_messages);
    }

    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, ::boost::asio::const_buffer))
                  CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
        CompletionToken,
        void(error_code, ::boost::asio::const_buffer)
    )
    async_read_one(std::uint8_t& seqnum, CompletionToken&& token, bool keep_messages = false)
    {
        return reader_
            .async_read_one(stream_, seqnum, std::forward<CompletionToken>(token), keep_messages);
    }

    // Writing
    void write(boost::asio::const_buffer buffer, std::uint8_t& seqnum, error_code& code)
    {
        writer_.write(stream_, buffer, seqnum, code);
    }

    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_write(boost::asio::const_buffer buffer, std::uint8_t& seqnum, CompletionToken&& token)
    {
        return writer_.async_write(stream_, buffer, seqnum, std::forward<CompletionToken>(token));
    }

    void write(const bytestring& buffer, std::uint8_t& seqnum, error_code& code)
    {
        write(boost::asio::buffer(buffer), seqnum, code);
    }

    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_write(const bytestring& buffer, std::uint8_t& seqnum, CompletionToken&& token)
    {
        return async_write(
            boost::asio::buffer(buffer),
            seqnum,
            std::forward<CompletionToken>(token)
        );
    }

    // SSL
    bool ssl_active() const noexcept { return stream_.ssl_active(); }
    void set_ssl_active(bool v) noexcept { stream_.set_ssl_active(v); }

    // Closing (only available for sockets)
    error_code close()
    {
        error_code err;
        lowest_layer().shutdown(lowest_layer_type::shutdown_both, err);
        lowest_layer().close(err);
        return err;
    }

    // Getting the underlying stream
    using stream_type = disableable_ssl_stream<Stream>;
    stream_type& stream() noexcept { return stream_; }
    const stream_type& stream() const noexcept { return stream_; }

    using lowest_layer_type = typename stream_type::lowest_layer_type;
    lowest_layer_type& lowest_layer() noexcept { return stream_.lowest_layer(); }

    // Capabilities
    capabilities current_capabilities() const noexcept { return current_caps_; }
    void set_current_capabilities(capabilities value) noexcept { current_caps_ = value; }

    void reset(std::size_t read_buffer_size)
    {
        stream_.reset();
        current_caps_ = capabilities();
        reset_sequence_number();
        reader_.buffer().grow_to_fit(read_buffer_size);
    }

    // Internal buffer, error_info and sequence_number to help async ops
    const bytestring& shared_buffer() const noexcept { return shared_buff_; }
    bytestring& shared_buffer() noexcept { return shared_buff_; }
    error_info& shared_info() noexcept { return shared_info_; }
    std::uint8_t& shared_sequence_number() noexcept { return shared_sequence_number_; }
    std::uint8_t& reset_sequence_number() noexcept { return shared_sequence_number_ = 0; }
    std::vector<field_view>& shared_fields() noexcept { return shared_fields_; }
    const std::vector<field_view>& shared_fields() const noexcept { return shared_fields_; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif

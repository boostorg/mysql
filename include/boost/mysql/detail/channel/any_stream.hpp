//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CHANNEL_ANY_STREAM_HPP
#define BOOST_MYSQL_DETAIL_CHANNEL_ANY_STREAM_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/basic_socket.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/write.hpp>

#include <cstddef>
#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

class any_stream
{
    bool ssl_active_{false};

public:
    void reset() noexcept { set_ssl_active(false); }  // TODO: do we really need this?
    bool ssl_active() const noexcept { return ssl_active_; }
    void set_ssl_active(bool v) noexcept { ssl_active_ = v; }

    template <class Stream>
    Stream& cast() noexcept;

    template <class Stream>
    const Stream& cast() const noexcept;

    using executor_type = boost::asio::any_io_executor;
    using lowest_layer_type = any_stream;

    virtual executor_type get_executor() noexcept = 0;

    // SSL
    virtual bool supports_ssl() const noexcept = 0;
    virtual void handshake(error_code& ec) = 0;
    virtual void async_handshake(asio::any_completion_handler<void(error_code)>) = 0;
    virtual void shutdown(error_code& ec) = 0;
    virtual void async_shutdown(asio::any_completion_handler<void(error_code)>) = 0;

    // Reading
    virtual std::size_t read_some(boost::asio::mutable_buffer, error_code& ec) = 0;
    virtual void async_read_some(boost::asio::mutable_buffer, asio::any_completion_handler<void(error_code, std::size_t)>) = 0;

    // Writing (we always write the message header and body with asio::write)
    // TODO: this is a little bit weird
    virtual std::size_t write(std::array<boost::asio::const_buffer, 2>, error_code& ec) = 0;
    virtual void async_write(std::array<boost::asio::const_buffer, 2>, asio::any_completion_handler<void(error_code, std::size_t)>) = 0;

    // Connect and close (TODO: is there a better way?)
    virtual void connect(const void* endpoint, error_code& ec) = 0;
    virtual void async_connect(const void* endpoint, asio::any_completion_handler<void(error_code)>) = 0;
    virtual void close(error_code& ec) = 0;
    virtual bool is_open() const noexcept = 0;
};

// Connect and close helpers
template <class Stream>
void do_connect(Stream&, const void*, error_code&)
{
    BOOST_ASSERT(false);
}

template <class Protocol, class Executor>
void do_connect(asio::basic_socket<Protocol, Executor>& stream, const void* endpoint, error_code& ec)
{
    using socket_t = asio::basic_socket<Protocol, Executor>;
    stream.connect(*static_cast<const typename socket_t::endpoint_type*>(endpoint), ec);
}

template <class Stream>
void do_async_connect(Stream&, const void*, asio::any_completion_handler<void(error_code)>)
{
    BOOST_ASSERT(false);
}

template <class Protocol, class Executor>
void do_async_connect(
    asio::basic_socket<Protocol, Executor>& stream,
    const void* endpoint,
    asio::any_completion_handler<void(error_code)> handler
)
{
    using socket_t = asio::basic_socket<Protocol, Executor>;
    stream.async_connect(*static_cast<const typename socket_t::endpoint_type*>(endpoint), std::move(handler));
}

template <class Stream>
void do_close(Stream&, error_code&)
{
    BOOST_ASSERT(false);
}

template <class Protocol, class Executor>
void do_close(asio::basic_socket<Protocol, Executor>& stream, error_code& ec)
{
    stream.shutdown(asio::socket_base::shutdown_both, ec);
    stream.close(ec);
}

template <class Stream>
bool do_is_open(const Stream&) noexcept
{
    return false;
}

template <class Protocol, class Executor>
bool do_is_open(const asio::basic_socket<Protocol, Executor>& stream) noexcept
{
    return stream.is_open();
}

template <class Stream>
class any_stream_impl final : public any_stream
{
    Stream stream_;

public:
    template <class... Args>  // TODO: noexcept
    any_stream_impl(Args&&... args) : stream_(std::forward<Args>(args)...)
    {
    }

    Stream& stream() noexcept { return stream_; }
    const Stream& stream() const noexcept { return stream_; }

    executor_type get_executor() noexcept override final { return stream_.get_executor(); }

    // SSL
    bool supports_ssl() const noexcept final override { return false; };
    void handshake(error_code&) final override { BOOST_ASSERT(false); }
    void async_handshake(asio::any_completion_handler<void(error_code)>) final override
    {
        BOOST_ASSERT(false);
    }
    void shutdown(error_code&) final override { BOOST_ASSERT(false); }
    void async_shutdown(asio::any_completion_handler<void(error_code)>) final override
    {
        BOOST_ASSERT(false);
    }

    // Reading
    std::size_t read_some(boost::asio::mutable_buffer buff, error_code& ec) final override
    {
        return stream_.read_some(buff, ec);
    }
    void async_read_some(
        boost::asio::mutable_buffer buff,
        asio::any_completion_handler<void(error_code, std::size_t)> handler
    ) final override
    {
        return stream_.async_read_some(buff, std::move(handler));
    }

    // Writing (we always write the message header and body with asio::write)
    // TODO: this is a little bit weird
    std::size_t write(std::array<boost::asio::const_buffer, 2> buff, error_code& ec) final override
    {
        return asio::write(stream_, buff, ec);
    }
    void async_write(
        std::array<boost::asio::const_buffer, 2> buff,
        asio::any_completion_handler<void(error_code, std::size_t)> handler
    ) final override
    {
        return asio::async_write(stream_, buff, std::move(handler));
    }

    // Connect and close (TODO: better way?)
    void connect(const void* endpoint, error_code& ec) override { do_connect(stream_, endpoint, ec); }
    void async_connect(const void* endpoint, asio::any_completion_handler<void(error_code)> handler)
    {
        do_async_connect(stream_, endpoint, std::move(handler));
    }
    void close(error_code& ec) override final { do_close(stream_, ec); }
    bool is_open() const noexcept override { return do_is_open(stream_); }
};

template <class Stream>
class any_stream_impl<asio::ssl::stream<Stream>> : public any_stream
{
    asio::ssl::stream<Stream> stream_;

public:
    template <class... Args>  // TODO: noexcept
    any_stream_impl(Args&&... args) : stream_(std::forward<Args>(args)...)
    {
    }

    Stream& stream() noexcept { return stream_; }
    const Stream& stream() const noexcept { return stream_; }

    executor_type get_executor() noexcept override final { return stream_.get_executor(); }

    // SSL
    bool supports_ssl() const noexcept final override { return true; };
    void handshake(error_code& ec) override final
    {
        set_ssl_active(true);
        return stream_.handshake(boost::asio::ssl::stream_base::client, ec);
    }
    void async_handshake(asio::any_completion_handler<void(error_code)> handler) override final
    {
        set_ssl_active(true);
        stream_.async_handshake(boost::asio::ssl::stream_base::client, std::move(handler));
    }
    void shutdown(error_code& ec) override final { return stream_.shutdown(ec); }
    void async_shutdown(asio::any_completion_handler<void(error_code)> handler) override final
    {
        return stream_.async_shutdown(std::move(handler));
    }

    // Reading
    std::size_t read_some(boost::asio::mutable_buffer buff, error_code& ec) override final
    {
        if (ssl_active())
        {
            return stream_.read_some(buff, ec);
        }
        else
        {
            return stream_.next_layer().read_some(buff, ec);
        }
    }
    void async_read_some(
        boost::asio::mutable_buffer buff,
        asio::any_completion_handler<void(error_code, std::size_t)> handler
    ) override final
    {
        if (ssl_active())
        {
            return stream_.async_read_some(buff, std::move(handler));
        }
        else
        {
            return stream_.next_layer().async_read_some(buff, std::move(handler));
        }
    }

    // Writing (we always write the message header and body with asio::write)
    // TODO: this is a little bit weird
    std::size_t write(std::array<boost::asio::const_buffer, 2> buff, error_code& ec) override final
    {
        if (ssl_active())
        {
            return asio::write(stream_, buff, ec);
        }
        else
        {
            return asio::write(stream_.next_layer(), buff, ec);
        }
    }
    void async_write(
        std::array<boost::asio::const_buffer, 2> buff,
        asio::any_completion_handler<void(error_code, std::size_t)> handler
    ) override final
    {
        if (ssl_active())
        {
            asio::async_write(stream_, buff, std::move(handler));
        }
        else
        {
            return asio::async_write(stream_.next_layer(), buff, std::move(handler));
        }
    }

    // Connect and close. TODO: better way?
    void connect(const void* endpoint, error_code& ec) override
    {
        do_connect(stream_.lowest_layer(), endpoint, ec);
    }
    void async_connect(const void* endpoint, asio::any_completion_handler<void(error_code)> handler)
    {
        do_async_connect(stream_.lowest_layer(), endpoint, std::move(handler));
    }
    void close(error_code& ec) override final { do_close(stream_.lowest_layer(), ec); }
    bool is_open() const noexcept override { return do_is_open(stream_.lowest_layer()); }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream>
const Stream& boost::mysql::detail::any_stream::cast() const noexcept
{
    return static_cast<const any_stream_impl<Stream>&>(*this).stream();
}

template <class Stream>
Stream& boost::mysql::detail::any_stream::cast() noexcept
{
    return static_cast<any_stream_impl<Stream>&>(*this).stream();
}

#endif

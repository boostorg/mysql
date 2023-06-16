//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_ANY_STREAM_IMPL_HPP
#define BOOST_MYSQL_DETAIL_ANY_STREAM_IMPL_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/any_stream.hpp>
#include <boost/mysql/detail/config.hpp>

#include <boost/asio/basic_socket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/config.hpp>

#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

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

template <class Protocol, class Executor>
void do_connect(asio::basic_stream_socket<Protocol, Executor>& stream, const void* endpoint, error_code& ec)
{
    using socket_t = asio::basic_stream_socket<Protocol, Executor>;
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

template <class Protocol, class Executor>
void do_async_connect(
    asio::basic_stream_socket<Protocol, Executor>& stream,
    const void* endpoint,
    asio::any_completion_handler<void(error_code)> handler
)
{
    using socket_t = asio::basic_stream_socket<Protocol, Executor>;
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

template <class Protocol, class Executor>
void do_close(asio::basic_stream_socket<Protocol, Executor>& stream, error_code& ec)
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

template <class Protocol, class Executor>
bool do_is_open(const asio::basic_stream_socket<Protocol, Executor>& stream) noexcept
{
    return stream.is_open();
}

template <class Stream>
class BOOST_SYMBOL_VISIBLE any_stream_impl final : public any_stream
{
    Stream stream_;

public:
    template <class... Args>  // TODO: noexcept
    any_stream_impl(Args&&... args) : stream_(std::forward<Args>(args)...)
    {
    }

    Stream& stream() noexcept { return stream_; }
    const Stream& stream() const noexcept { return stream_; }

    executor_type get_executor() override final { return stream_.get_executor(); }

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

    // Writing
    std::size_t write_some(boost::asio::const_buffer buff, error_code& ec) final override
    {
        return stream_.write_some(buff, ec);
    }
    void async_write_some(
        boost::asio::const_buffer buff,
        asio::any_completion_handler<void(error_code, std::size_t)> handler
    ) final override
    {
        return stream_.async_write_some(buff, std::move(handler));
    }

    // Connect and close (TODO: better way?)
    void connect(const void* endpoint, error_code& ec) override final { do_connect(stream_, endpoint, ec); }
    void async_connect(const void* endpoint, asio::any_completion_handler<void(error_code)> handler)
        override final
    {
        do_async_connect(stream_, endpoint, std::move(handler));
    }
    void close(error_code& ec) override final { do_close(stream_, ec); }
    bool is_open() const noexcept override { return do_is_open(stream_); }
};

template <class Stream>
class BOOST_SYMBOL_VISIBLE any_stream_impl<asio::ssl::stream<Stream>> final : public any_stream
{
    asio::ssl::stream<Stream> stream_;

public:
    template <class... Args>  // TODO: noexcept
    any_stream_impl(Args&&... args) : stream_(std::forward<Args>(args)...)
    {
    }

    asio::ssl::stream<Stream>& stream() noexcept { return stream_; }
    const asio::ssl::stream<Stream>& stream() const noexcept { return stream_; }

    executor_type get_executor() override final { return stream_.get_executor(); }

    // SSL
    bool supports_ssl() const noexcept final override { return true; };
    void handshake(error_code& ec) override final
    {
        set_ssl_active(true);
        stream_.handshake(boost::asio::ssl::stream_base::client, ec);
    }
    void async_handshake(asio::any_completion_handler<void(error_code)> handler) override final
    {
        set_ssl_active(true);
        stream_.async_handshake(boost::asio::ssl::stream_base::client, std::move(handler));
    }
    void shutdown(error_code& ec) override final { stream_.shutdown(ec); }
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

    // Writing
    std::size_t write_some(boost::asio::const_buffer buff, error_code& ec) override final
    {
        if (ssl_active())
        {
            return stream_.write_some(buff, ec);
        }
        else
        {
            return stream_.next_layer().write_some(buff, ec);
        }
    }
    void async_write_some(
        boost::asio::const_buffer buff,
        asio::any_completion_handler<void(error_code, std::size_t)> handler
    ) override final
    {
        if (ssl_active())
        {
            stream_.async_write_some(buff, std::move(handler));
        }
        else
        {
            return stream_.next_layer().async_write_some(buff, std::move(handler));
        }
    }

    // Connect and close. TODO: better way?
    void connect(const void* endpoint, error_code& ec) override final
    {
        do_connect(stream_.lowest_layer(), endpoint, ec);
    }
    void async_connect(const void* endpoint, asio::any_completion_handler<void(error_code)> handler)
        override final
    {
        do_async_connect(stream_.lowest_layer(), endpoint, std::move(handler));
    }
    void close(error_code& ec) override final { do_close(stream_.lowest_layer(), ec); }
    bool is_open() const noexcept override { return do_is_open(stream_.lowest_layer()); }
};

template <class Stream>
const Stream& cast(const any_stream& obj) noexcept
{
    return static_cast<const any_stream_impl<Stream>&>(obj).stream();
}

template <class Stream>
Stream& cast(any_stream& obj) noexcept
{
    return static_cast<any_stream_impl<Stream>&>(obj).stream();
}

#ifdef BOOST_MYSQL_SEPARATE_COMPILATION
extern template class any_stream_impl<asio::ssl::stream<asio::ip::tcp::socket>>;
extern template class any_stream_impl<asio::ip::tcp::socket>;
#endif

}  // namespace detail
}  // namespace mysql
}  // namespace boost

// any_stream_impl.ipp explicitly instantiates any_stream_impl, so not included here

#endif

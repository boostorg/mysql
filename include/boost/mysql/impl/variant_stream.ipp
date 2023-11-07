//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_VARIANT_STREAM_IPP
#define BOOST_MYSQL_IMPL_VARIANT_STREAM_IPP

#pragma once

#include <boost/mysql/address_type.hpp>
#include <boost/mysql/connect_params.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/any_address.hpp>
#include <boost/mysql/detail/variant_stream.hpp>

struct boost::mysql::detail::variant_stream::connect_op : boost::asio::coroutine
{
    variant_stream& this_obj_;
    error_code stored_ec_;

    connect_op(variant_stream& this_obj) noexcept : this_obj_(this_obj) {}

    template <class Self>
    void operator()(Self& self, error_code ec = {}, asio::ip::tcp::resolver::results_type endpoints = {})
    {
        if (ec)
        {
            self.complete(ec);
            return;
        }

        BOOST_ASIO_CORO_REENTER(*this)
        {
            // Setup stream
            stored_ec_ = this_obj_.setup_stream();
            if (stored_ec_)
            {
                BOOST_ASIO_CORO_YIELD asio::post(this_obj_.ex_, std::move(self));
                self.complete(stored_ec_);
                return;
            }

            if (this_obj_.address_.type == address_type::tcp_address)
            {
                // Resolve endpoints
                BOOST_ASIO_CORO_YIELD
                variant2::unsafe_get<1>(this_obj_.sock_)
                    .resolv.async_resolve(
                        this_obj_.address_.address,
                        std::to_string(this_obj_.address_.port),
                        std::move(self)
                    );

                // Connect stream
                BOOST_ASIO_CORO_YIELD
                asio::async_connect(
                    variant2::unsafe_get<1>(this_obj_.sock_).sock,
                    std::move(endpoints),
                    std::move(self)
                );

                // The final handler requires a void(error_code, tcp::endpoint signature),
                // which this function can't implement. See operator() overload below.
            }
            else
            {
                BOOST_ASSERT(this_obj_.address_.type == address_type::unix_path);

                // Just connect the stream
                // TODO: we could save a copy here
                BOOST_ASIO_CORO_YIELD
                variant2::unsafe_get<2>(this_obj_.sock_)
                    .async_connect(std::string(this_obj_.address_.address), std::move(self));

                self.complete(error_code());
            }
        }
    }

    template <class Self>
    void operator()(Self& self, error_code ec, asio::ip::tcp::endpoint)
    {
        self.complete(ec);
    }
};

void boost::mysql::detail::variant_stream::handshake(error_code& ec)
{
    create_ssl_stream();
    ssl_->handshake(asio::ssl::stream_base::client, ec);
}

void boost::mysql::detail::variant_stream::async_handshake(
    asio::any_completion_handler<void(error_code)> handler
)
{
    create_ssl_stream();
    ssl_->async_handshake(asio::ssl::stream_base::client, std::move(handler));
}

void boost::mysql::detail::variant_stream::shutdown(error_code& ec)
{
    BOOST_ASSERT(ssl_.has_value());
    ssl_->shutdown(ec);
}

void boost::mysql::detail::variant_stream::async_shutdown(
    asio::any_completion_handler<void(error_code)> handler
)
{
    BOOST_ASSERT(ssl_.has_value());
    ssl_->async_shutdown(std::move(handler));
}

// Reading
std::size_t boost::mysql::detail::variant_stream::read_some(
    asio::mutable_buffer buff,
    bool use_ssl,
    error_code& ec
)
{
    if (use_ssl)
    {
        BOOST_ASSERT(ssl_.has_value());
        return ssl_->read_some(buff, ec);
    }
    else if (auto* tcp_sock = variant2::get_if<socket_and_resolver>(&sock_))
    {
        return tcp_sock->sock.read_some(buff, ec);
    }
    else if (auto* unix_sock = variant2::get_if<unix_socket>(&sock_))
    {
        return unix_sock->read_some(buff, ec);
    }
    else
    {
        BOOST_ASSERT(false);
    }
}

void boost::mysql::detail::variant_stream::async_read_some(
    asio::mutable_buffer buff,
    bool use_ssl,
    asio::any_completion_handler<void(error_code, std::size_t)> handler
)
{
    if (use_ssl)
    {
        BOOST_ASSERT(ssl_.has_value());
        ssl_->async_read_some(buff, std::move(handler));
    }
    else if (auto* tcp_sock = variant2::get_if<socket_and_resolver>(&sock_))
    {
        tcp_sock->sock.async_read_some(buff, std::move(handler));
    }
    else if (auto* unix_sock = variant2::get_if<unix_socket>(&sock_))
    {
        unix_sock->async_read_some(buff, std::move(handler));
    }
    else
    {
        BOOST_ASSERT(false);
    }
}

// Writing
std::size_t boost::mysql::detail::variant_stream::write_some(
    boost::asio::const_buffer buff,
    bool use_ssl,
    error_code& ec
)
{
    if (use_ssl)
    {
        BOOST_ASSERT(ssl_.has_value());
        return ssl_->write_some(buff, ec);
    }
    else if (auto* tcp_sock = variant2::get_if<socket_and_resolver>(&sock_))
    {
        return tcp_sock->sock.write_some(buff, ec);
    }
    else if (auto* unix_sock = variant2::get_if<unix_socket>(&sock_))
    {
        return unix_sock->write_some(buff, ec);
    }
    else
    {
        BOOST_ASSERT(false);
    }
}

void boost::mysql::detail::variant_stream::async_write_some(
    boost::asio::const_buffer buff,
    bool use_ssl,
    asio::any_completion_handler<void(error_code, std::size_t)> handler
)
{
    if (use_ssl)
    {
        BOOST_ASSERT(ssl_.has_value());
        return ssl_->async_write_some(buff, std::move(handler));
    }
    else if (auto* tcp_sock = variant2::get_if<socket_and_resolver>(&sock_))
    {
        return tcp_sock->sock.async_write_some(buff, std::move(handler));
    }
    else if (auto* unix_sock = variant2::get_if<unix_socket>(&sock_))
    {
        return unix_sock->async_write_some(buff, std::move(handler));
    }
    else
    {
        BOOST_ASSERT(false);
    }
}

void boost::mysql::detail::variant_stream::connect(error_code& ec)
{
    ec = setup_stream();
    if (ec)
        return;

    if (address_.type == address_type::tcp_address)
    {
        // Resolve endpoints. TODO: we can save the to_string allocation
        auto& tcp_sock = variant2::unsafe_get<1>(sock_);
        auto endpoints = tcp_sock.resolv.resolve(address_.address, std::to_string(address_.port), ec);
        if (ec)
            return;

        // Connect stream
        asio::connect(tcp_sock.sock, std::move(endpoints), ec);
    }
    else
    {
        BOOST_ASSERT(address_.type == address_type::unix_path);

        // Just connect the stream
        auto& unix_sock = variant2::unsafe_get<2>(sock_);
        unix_sock.connect(std::string(address_.address), ec);
    }
}

void boost::mysql::detail::variant_stream::async_connect(
    asio::any_completion_handler<void(error_code)> handler
)
{
    asio::async_compose<asio::any_completion_handler<void(error_code)>, void(error_code)>(
        connect_op(*this),
        handler,
        ex_
    );
}

void boost::mysql::detail::variant_stream::close(error_code& ec)
{
    if (auto* tcp_sock = variant2::get_if<socket_and_resolver>(&sock_))
    {
        tcp_sock->sock.close(ec);
    }
    else if (auto* unix_sock = variant2::get_if<unix_socket>(&sock_))
    {
        unix_sock->close(ec);
    }
}

boost::mysql::error_code boost::mysql::detail::variant_stream::setup_stream()
{
    BOOST_ASSERT(!address_.address.empty());
    if (address_.type == address_type::tcp_address)
    {
        auto* tcp_sock = variant2::get_if<socket_and_resolver>(&sock_);
        if (tcp_sock)
        {
            // Clean up any previous state
            if (tcp_sock->sock.is_open())
            {
                error_code ignored;
                tcp_sock->sock.close(ignored);
            }
        }
        else
        {
            // Create a socket
            sock_.emplace<socket_and_resolver>(ex_);
        }
    }
    else if (address_.type == address_type::unix_path)
    {
        // TODO: fail if not supported
        auto* unix_sock = variant2::get_if<unix_socket>(&sock_);
        if (unix_sock)
        {
            // Clean up any previous state
            if (unix_sock->is_open())
            {
                error_code ignored;
                unix_sock->close(ignored);
            }
        }
        else
        {
            // Create a socket
            sock_.emplace<unix_socket>(ex_);
        }
    }
    return error_code();
}

boost::asio::ssl::context& boost::mysql::detail::variant_stream::ensure_ssl_context()
{
    // Do we have a default context already created?
    auto* default_ctx = variant2::get_if<asio::ssl::context>(&ssl_ctx_);
    if (default_ctx)
        return *default_ctx;

    // Do we have an external context provided by the user?
    BOOST_ASSERT(variant2::holds_alternative<asio::ssl::context*>(ssl_ctx_));
    auto* external_ctx = variant2::unsafe_get<0>(ssl_ctx_);
    if (external_ctx)
        return *external_ctx;

    // Create a default context and return it
    // TODO: this is not secure. Investigate
    return ssl_ctx_.emplace<asio::ssl::context>(asio::ssl::context::tls_client);
}

void boost::mysql::detail::variant_stream::create_ssl_stream()
{
    // The stream object must be re-created even if it already exists, since
    // once used for a connection (anytime after ssl::stream::handshake is called),
    // it can't be re-used for any subsequent connections
    BOOST_ASSERT(variant2::holds_alternative<socket_and_resolver>(sock_));
    ssl_.emplace(variant2::unsafe_get<1>(sock_).sock, ensure_ssl_context());
}

#endif

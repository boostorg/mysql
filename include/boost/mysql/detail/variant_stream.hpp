//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_VARIANT_STREAM_HPP
#define BOOST_MYSQL_DETAIL_VARIANT_STREAM_HPP

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/any_stream.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/optional/optional.hpp>
#include <boost/variant2/variant.hpp>

#include <string>

namespace boost {
namespace mysql {
namespace detail {

class variant_stream final : public any_stream
{
public:
    variant_stream(asio::any_io_executor ex, asio::ssl::context* ctx)
        : any_stream(true), ex_(std::move(ex)), ssl_ctx_(ctx)
    {
    }

    // Executor
    executor_type get_executor() override final { return ex_; }

    // SSL
    void handshake(error_code& ec) override final
    {
        create_ssl_stream();
        ssl_->handshake(asio::ssl::stream_base::client, ec);
    }

    void async_handshake(asio::any_completion_handler<void(error_code)> handler) override final
    {
        create_ssl_stream();
        ssl_->async_handshake(asio::ssl::stream_base::client, std::move(handler));
    }

    void shutdown(error_code& ec) override final
    {
        BOOST_ASSERT(ssl_.has_value());
        ssl_->shutdown(ec);
    }

    void async_shutdown(asio::any_completion_handler<void(error_code)> handler) override final
    {
        BOOST_ASSERT(ssl_.has_value());
        ssl_->async_shutdown(std::move(handler));
    }

    // Reading
    std::size_t read_some(asio::mutable_buffer buff, bool use_ssl, error_code& ec) override final
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

        BOOST_ASSERT(false);
    }

    void async_read_some(
        asio::mutable_buffer buff,
        bool use_ssl,
        asio::any_completion_handler<void(error_code, std::size_t)> handler
    ) override final
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

        BOOST_ASSERT(false);
    }

    // Writing
    std::size_t write_some(boost::asio::const_buffer buff, bool use_ssl, error_code& ec) override final
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

        BOOST_ASSERT(false);
    }

    void async_write_some(
        boost::asio::const_buffer buff,
        bool use_ssl,
        asio::any_completion_handler<void(error_code, std::size_t)> handler
    ) override final
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

        BOOST_ASSERT(false);
    }

    void connect(const void* address, error_code& ec) override final
    {
        do_connect(*static_cast<const any_address_view*>(address), ec);
    }

    void async_connect(const void* address, asio::any_completion_handler<void(error_code)> handler)
        override final
    {
        asio::async_compose<asio::any_completion_handler<void(error_code)>, void(error_code)>(
            connect_op(*this, *static_cast<const any_address_view*>(address)),
            handler,
            ex_
        );
    }

    void close(error_code& ec) override final
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

private:
    struct socket_and_resolver
    {
        asio::ip::tcp::socket sock;
        asio::ip::tcp::resolver resolv;

        socket_and_resolver(asio::any_io_executor ex) : sock(ex), resolv(std::move(ex)) {}
    };

    using unix_socket = asio::local::stream_protocol::socket;

    asio::any_io_executor ex_;  // TODO: we can probably get rid of this
    variant2::variant<variant2::monostate, socket_and_resolver, unix_socket> sock_;
    variant2::variant<asio::ssl::context*, asio::ssl::context> ssl_ctx_;
    boost::optional<asio::ssl::stream<asio::ip::tcp::socket&>> ssl_;

    error_code setup_stream(any_address_view server_address)
    {
        if (server_address.type() == address_type::tcp)
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
        else if (server_address.type() == address_type::unix)
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

    asio::ssl::context& ensure_ssl_context()
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

    void do_connect(any_address_view server_address, error_code& ec)
    {
        ec = setup_stream(server_address);
        if (ec)
            return;

        if (server_address.type() == address_type::tcp)
        {
            // Resolve endpoints. TODO: we can save the to_string allocation
            auto& tcp_sock = variant2::unsafe_get<1>(sock_);
            auto endpoints = tcp_sock.resolv.resolve(
                server_address.hostname(),
                std::to_string(server_address.port()),
                ec
            );
            if (ec)
                return;

            // Connect stream
            asio::connect(tcp_sock.sock, std::move(endpoints), ec);
        }
        else
        {
            BOOST_ASSERT(server_address.type() == address_type::unix);

            // Just connect the stream
            auto& unix_sock = variant2::unsafe_get<2>(sock_);
            unix_sock.connect(
                std::string(server_address.unix_path()),
                ec
            );  // TODO: we could save a copy here
        }
    }

    struct connect_op : asio::coroutine
    {
        variant_stream& this_obj_;
        any_address_view server_address_;
        error_code stored_ec_;

        connect_op(variant_stream& this_obj, any_address_view server_address) noexcept
            : this_obj_(this_obj), server_address_(server_address)
        {
        }

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
                stored_ec_ = this_obj_.setup_stream(server_address_);
                if (stored_ec_)
                {
                    BOOST_ASIO_CORO_YIELD asio::post(this_obj_.ex_, std::move(self));
                    self.complete(stored_ec_);
                    return;
                }

                if (server_address_.type() == address_type::tcp)
                {
                    // Resolve endpoints
                    BOOST_ASIO_CORO_YIELD
                    variant2::unsafe_get<1>(this_obj_.sock_)
                        .resolv.async_resolve(
                            server_address_.hostname(),
                            std::to_string(server_address_.port()),
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
                    BOOST_ASSERT(server_address_.type() == address_type::unix);

                    // Just connect the stream
                    // TODO: we could save a copy here
                    BOOST_ASIO_CORO_YIELD
                    variant2::unsafe_get<2>(this_obj_.sock_)
                        .async_connect(std::string(server_address_.unix_path()), std::move(self));

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

    void create_ssl_stream()
    {
        // The stream object must be re-created even if it already exists, since
        // once used for a connection (anytime after ssl::stream::handshake is called),
        // it can't be re-used for any subsequent connections
        BOOST_ASSERT(variant2::holds_alternative<socket_and_resolver>(sock_));
        ssl_.emplace(variant2::unsafe_get<1>(sock_).sock, ensure_ssl_context());
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif

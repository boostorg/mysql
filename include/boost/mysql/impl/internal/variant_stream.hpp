//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_VARIANT_STREAM_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_VARIANT_STREAM_HPP

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/connect_params_helpers.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/ssl_context_with_default.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/optional/optional.hpp>
#include <boost/variant2/variant.hpp>

#include <string>
#include <utility>

namespace boost {
namespace mysql {
namespace detail {

// Asio defines a "string view parameter" to be either const std::string&,
// std::experimental::string_view or std::string_view. Casting from the Boost
// version doesn't work for std::experimental::string_view
#if defined(BOOST_ASIO_HAS_STD_STRING_VIEW)
inline std::string_view cast_asio_sv_param(string_view input) noexcept { return input; }
#elif defined(BOOST_ASIO_HAS_STD_EXPERIMENTAL_STRING_VIEW)
inline std::experimental::string_view cast_asio_sv_param(string_view input) noexcept
{
    return {input.data(), input.size()};
}
#else
inline std::string cast_asio_sv_param(string_view input) { return input; }
#endif

// Implements the EngineStream concept (see stream_adaptor)
class variant_stream
{
public:
    variant_stream(asio::any_io_executor ex, asio::ssl::context* ctx) : ex_(std::move(ex)), ssl_ctx_(ctx) {}

    bool supports_ssl() const { return true; }

    void set_endpoint(const void* value) { address_ = static_cast<const any_address*>(value); }

    // Executor
    using executor_type = asio::any_io_executor;
    executor_type get_executor() { return ex_; }

    // SSL
    void ssl_handshake(error_code& ec)
    {
        create_ssl_stream();
        ssl_->handshake(asio::ssl::stream_base::client, ec);
    }

    template <class CompletionToken>
    void async_ssl_handshake(CompletionToken&& token)
    {
        create_ssl_stream();
        ssl_->async_handshake(asio::ssl::stream_base::client, std::forward<CompletionToken>(token));
    }

    void ssl_shutdown(error_code& ec)
    {
        BOOST_ASSERT(ssl_.has_value());
        ssl_->shutdown(ec);
    }

    template <class CompletionToken>
    void async_ssl_shutdown(CompletionToken&& token)
    {
        BOOST_ASSERT(ssl_.has_value());
        ssl_->async_shutdown(std::forward<CompletionToken>(token));
    }

    // Reading
    std::size_t read_some(asio::mutable_buffer buff, bool use_ssl, error_code& ec)
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
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
        else if (auto* unix_sock = variant2::get_if<unix_socket>(&sock_))
        {
            return unix_sock->read_some(buff, ec);
        }
#endif
        else
        {
            BOOST_ASSERT(false);
            return 0u;
        }
    }

    template <class CompletionToken>
    void async_read_some(asio::mutable_buffer buff, bool use_ssl, CompletionToken&& token)
    {
        if (use_ssl)
        {
            BOOST_ASSERT(ssl_.has_value());
            ssl_->async_read_some(buff, std::forward<CompletionToken>(token));
        }
        else if (auto* tcp_sock = variant2::get_if<socket_and_resolver>(&sock_))
        {
            tcp_sock->sock.async_read_some(buff, std::forward<CompletionToken>(token));
        }
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
        else if (auto* unix_sock = variant2::get_if<unix_socket>(&sock_))
        {
            unix_sock->async_read_some(buff, std::forward<CompletionToken>(token));
        }
#endif
        else
        {
            BOOST_ASSERT(false);
        }
    }

    // Writing
    std::size_t write_some(boost::asio::const_buffer buff, bool use_ssl, error_code& ec)
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
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
        else if (auto* unix_sock = variant2::get_if<unix_socket>(&sock_))
        {
            return unix_sock->write_some(buff, ec);
        }
#endif
        else
        {
            BOOST_ASSERT(false);
            return 0u;
        }
    }

    template <class CompletionToken>
    void async_write_some(boost::asio::const_buffer buff, bool use_ssl, CompletionToken&& token)
    {
        if (use_ssl)
        {
            BOOST_ASSERT(ssl_.has_value());
            return ssl_->async_write_some(buff, std::forward<CompletionToken>(token));
        }
        else if (auto* tcp_sock = variant2::get_if<socket_and_resolver>(&sock_))
        {
            return tcp_sock->sock.async_write_some(buff, std::forward<CompletionToken>(token));
        }
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
        else if (auto* unix_sock = variant2::get_if<unix_socket>(&sock_))
        {
            return unix_sock->async_write_some(buff, std::forward<CompletionToken>(token));
        }
#endif
        else
        {
            BOOST_ASSERT(false);
        }
    }

    // Connect and close
    void connect(error_code& ec)
    {
        ec = setup_stream();
        if (ec)
            return;

        if (address_->type() == address_type::host_and_port)
        {
            // Resolve endpoints
            auto& tcp_sock = variant2::unsafe_get<1>(sock_);
            auto endpoints = tcp_sock.resolv.resolve(
                cast_asio_sv_param(address_->hostname()),
                std::to_string(address_->port()),
                ec
            );
            if (ec)
                return;

            // Connect stream
            asio::connect(tcp_sock.sock, std::move(endpoints), ec);
            if (ec)
                return;

            // Disable Naggle's algorithm
            set_tcp_nodelay();
        }
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
        else
        {
            BOOST_ASSERT(address_->type() == address_type::unix_path);

            // Just connect the stream
            auto& unix_sock = variant2::unsafe_get<2>(sock_);
            unix_sock.connect(cast_asio_sv_param(address_->unix_socket_path()), ec);
        }
#endif
    }

    template <class CompletionToken>
    void async_connect(CompletionToken&& token)
    {
        asio::async_compose<CompletionToken, void(error_code)>(connect_op(*this), token, ex_);
    }

    void close(error_code& ec)
    {
        if (auto* tcp_sock = variant2::get_if<socket_and_resolver>(&sock_))
        {
            tcp_sock->sock.close(ec);
        }
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
        else if (auto* unix_sock = variant2::get_if<unix_socket>(&sock_))
        {
            unix_sock->close(ec);
        }
#endif
    }

    // Exposed for testing
    const asio::ip::tcp::socket& tcp_socket() const { return variant2::get<socket_and_resolver>(sock_).sock; }

private:
    struct socket_and_resolver
    {
        asio::ip::tcp::socket sock;
        asio::ip::tcp::resolver resolv;

        socket_and_resolver(asio::any_io_executor ex) : sock(ex), resolv(std::move(ex)) {}
    };

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
    using unix_socket = asio::local::stream_protocol::socket;
#endif

    const any_address* address_{};
    asio::any_io_executor ex_;
    variant2::variant<
        variant2::monostate,
        socket_and_resolver
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
        ,
        unix_socket
#endif
        >
        sock_;
    ssl_context_with_default ssl_ctx_;
    boost::optional<asio::ssl::stream<asio::ip::tcp::socket&>> ssl_;

    error_code setup_stream()
    {
        if (address_->type() == address_type::host_and_port)
        {
            // Clean up any previous state
            sock_.emplace<socket_and_resolver>(ex_);
        }

        else if (address_->type() == address_type::unix_path)
        {
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
            // Clean up any previous state
            sock_.emplace<unix_socket>(ex_);
#else
            return asio::error::operation_not_supported;
#endif
        }

        return error_code();
    }

    void set_tcp_nodelay() { variant2::unsafe_get<1u>(sock_).sock.set_option(asio::ip::tcp::no_delay(true)); }

    void create_ssl_stream()
    {
        // The stream object must be re-created even if it already exists, since
        // once used for a connection (anytime after ssl::stream::handshake is called),
        // it can't be re-used for any subsequent connections
        BOOST_ASSERT(variant2::holds_alternative<socket_and_resolver>(sock_));
        ssl_.emplace(variant2::unsafe_get<1>(sock_).sock, ssl_ctx_.get());
    }

    struct connect_op
    {
        int resume_point_{0};
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

            switch (resume_point_)
            {
            case 0:

                // Setup stream
                stored_ec_ = this_obj_.setup_stream();
                if (stored_ec_)
                {
                    BOOST_MYSQL_YIELD(resume_point_, 1, asio::post(this_obj_.ex_, std::move(self)))
                    self.complete(stored_ec_);
                    return;
                }

                if (this_obj_.address_->type() == address_type::host_and_port)
                {
                    // Resolve endpoints
                    BOOST_MYSQL_YIELD(
                        resume_point_,
                        2,
                        variant2::unsafe_get<1>(this_obj_.sock_)
                            .resolv.async_resolve(
                                cast_asio_sv_param(this_obj_.address_->hostname()),
                                std::to_string(this_obj_.address_->port()),
                                std::move(self)
                            )
                    )

                    // Connect stream
                    BOOST_MYSQL_YIELD(
                        resume_point_,
                        3,
                        asio::async_connect(
                            variant2::unsafe_get<1>(this_obj_.sock_).sock,
                            std::move(endpoints),
                            std::move(self)
                        )
                    )

                    // The final handler requires a void(error_code, tcp::endpoint signature),
                    // which this function can't implement. See operator() overload below.
                }
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
                else
                {
                    BOOST_ASSERT(this_obj_.address_->type() == address_type::unix_path);

                    // Just connect the stream
                    BOOST_MYSQL_YIELD(
                        resume_point_,
                        4,
                        variant2::unsafe_get<2>(this_obj_.sock_)
                            .async_connect(
                                cast_asio_sv_param(this_obj_.address_->unix_socket_path()),
                                std::move(self)
                            )
                    )

                    self.complete(error_code());
                }
#endif
            }
        }

        template <class Self>
        void operator()(Self& self, error_code ec, asio::ip::tcp::endpoint)
        {
            if (!ec)
            {
                // Disable Naggle's algorithm
                this_obj_.set_tcp_nodelay();
            }
            self.complete(ec);
        }
    };
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif

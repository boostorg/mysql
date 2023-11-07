//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_VARIANT_STREAM_HPP
#define BOOST_MYSQL_DETAIL_VARIANT_STREAM_HPP

#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/any_address.hpp>
#include <boost/mysql/detail/any_stream.hpp>
#include <boost/mysql/detail/config.hpp>

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

#include <memory>
#include <string>

namespace boost {
namespace mysql {
namespace detail {

class variant_stream final : public any_stream
{
    any_address address_;

public:
    variant_stream(asio::any_io_executor ex, asio::ssl::context* ctx)
        : any_stream(true), ex_(std::move(ex)), ssl_ctx_(ctx)
    {
    }

    void set_address(any_address addr) noexcept { address_ = addr; }

    // Executor
    executor_type get_executor() override final { return ex_; }

    // SSL
    BOOST_MYSQL_DECL
    void handshake(error_code& ec) override final;

    BOOST_MYSQL_DECL
    void async_handshake(asio::any_completion_handler<void(error_code)> handler) override final;

    BOOST_MYSQL_DECL
    void shutdown(error_code& ec) override final;

    BOOST_MYSQL_DECL
    void async_shutdown(asio::any_completion_handler<void(error_code)> handler) override final;

    // Reading
    BOOST_MYSQL_DECL
    std::size_t read_some(asio::mutable_buffer buff, bool use_ssl, error_code& ec) override final;

    BOOST_MYSQL_DECL
    void async_read_some(
        asio::mutable_buffer buff,
        bool use_ssl,
        asio::any_completion_handler<void(error_code, std::size_t)> handler
    ) override final;

    // Writing
    BOOST_MYSQL_DECL
    std::size_t write_some(boost::asio::const_buffer buff, bool use_ssl, error_code& ec) override final;

    BOOST_MYSQL_DECL
    void async_write_some(
        boost::asio::const_buffer buff,
        bool use_ssl,
        asio::any_completion_handler<void(error_code, std::size_t)> handler
    ) override final;

    // Connect and close
    BOOST_MYSQL_DECL
    void connect(error_code& ec) override final;

    BOOST_MYSQL_DECL
    void async_connect(asio::any_completion_handler<void(error_code)> handler) override final;

    BOOST_MYSQL_DECL
    void close(error_code& ec) override final;

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

    BOOST_MYSQL_DECL
    error_code setup_stream();

    BOOST_MYSQL_DECL
    asio::ssl::context& ensure_ssl_context();

    BOOST_MYSQL_DECL
    void create_ssl_stream();

    struct connect_op;
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/variant_stream.ipp>
#endif

#endif

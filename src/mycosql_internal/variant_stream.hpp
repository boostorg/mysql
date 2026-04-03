//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_VARIANT_STREAM_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_VARIANT_STREAM_HPP

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/engine.hpp>

#include <boost/assert.hpp>
#include <boost/capy/ex/execution_context.hpp>
#include <boost/capy/io/any_stream.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/corosio/native/native_socket_option.hpp>
#include <boost/corosio/openssl_stream.hpp>
#include <boost/corosio/resolver.hpp>
#include <boost/corosio/resolver_results.hpp>
#include <boost/corosio/tcp_socket.hpp>
#include <boost/corosio/tls_context.hpp>
#include <boost/corosio/tls_stream.hpp>

#include <optional>
#include <string>

#include "mycosql_internal/coroutine.hpp"
#include "mycosql_internal/ssl_context_with_default.hpp"

namespace boost {
namespace mysql {
namespace detail {

// TODO: this should be in corosio
inline capy::io_task<> range_connect(corosio::tcp_socket& sock, const corosio::resolver_results& endpoints)
{
    error_code ec;
    for (auto const& ep : endpoints)
    {
        ec = (co_await sock.connect(ep)).ec;
        if (!ec)
        {
            co_return {};
        }
        sock.close();  // TODO: check this, check socket opening
    }
    co_return {ec};
}

// Implements the EngineStream concept (see stream_adaptor)
class variant_stream final : public mysql_stream
{
    corosio::resolver resolv_;
    corosio::tcp_socket sock_;
    std::optional<corosio::openssl_stream> ssl_stream_;
    bool tls_enabled_{false};
    // TODO: UNIX

public:
    variant_stream(capy::execution_context& ctx) : mysql_stream(true), resolv_(ctx), sock_(ctx) {}

    capy::io_task<std::size_t> read_some(capy::mutable_buffer buff) override
    {
        if (ssl_stream_.has_value())
            co_return co_await ssl_stream_->read_some(buff);
        else
            co_return co_await sock_.read_some(buff);
    }

    capy::io_task<std::size_t> write_some(capy::const_buffer buff) override
    {
        if (ssl_stream_.has_value())
            co_return co_await ssl_stream_->write_some(buff);
        else
            co_return co_await sock_.write_some(buff);
    }

    capy::io_task<> connect(const any_address& addr) override
    {
        // Clean up any previous state
        tls_enabled_ = false;
        sock_.close();

        // Set up the endpoints vector
        if (addr.type() == address_type::host_and_port)
        {
            // TODO: lazy construct the resolver?

            // Resolve the endpoints
            auto [ec1, eps] = co_await resolv_.resolve(addr.hostname(), std::to_string(addr.port()));
            if (ec1)
                co_return {ec1};

            // Connect
            auto [ec2] = co_await range_connect(sock_, eps);
            if (ec2)
                co_return {ec2};

            // Disable Naggle's algorithm
            sock_.set_option(corosio::native_socket_option::no_delay(true));
        }
        else
        {
            // TODO
            //             BOOST_ASSERT(addr_->type() == address_type::unix_path);
            // #ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
            //             endpoints_.push_back(asio::local::stream_protocol::endpoint(address()));
            // #else
            //             BOOST_MYSQL_YIELD(resume_point_, 3, vsconnect_action::immediate_tag{});
            //             return vsconnect_action(asio::error::operation_not_supported);
            // #endif
        }

        co_return {};
    }

    capy::io_task<> tls_handshake() override
    {
        BOOST_ASSERT(!tls_enabled_);

        // TODO: accept the TLS context
        // Create the stream if required
        if (ssl_stream_)
            ssl_stream_->reset();
        else
            ssl_stream_.emplace(capy::any_stream(&sock_), corosio::tls_context());

        // Actually perform the handshake
        auto [ec] = co_await ssl_stream_->handshake(corosio::tls_stream::client);
        if (ec)
            co_return {ec};

        // Record that we should be using TLS from now on
        tls_enabled_ = true;

        co_return {};
    }

    capy::io_task<> tls_shutdown() override
    {
        BOOST_ASSERT(tls_enabled_);
        BOOST_ASSERT(ssl_stream_.has_value());
        auto [ec] = co_await ssl_stream_->shutdown();
        tls_enabled_ = false;
        co_return {ec};
    }

    capy::io_task<> close() override
    {
        sock_.shutdown(corosio::tcp_socket::shutdown_both);
        sock_.close();
        co_return {};
    }

    // // SSL
    // void ssl_handshake(error_code& ec)
    // {
    //     st_.create_ssl_stream().handshake(asio::ssl::stream_base::client, ec);
    // }

    // template <class CompletionToken>
    // void async_ssl_handshake(CompletionToken&& token)
    // {
    //     st_.create_ssl_stream();
    //     st_.ssl->async_handshake(asio::ssl::stream_base::client, std::forward<CompletionToken>(token));
    // }

    // void ssl_shutdown(error_code& ec)
    // {
    //     BOOST_ASSERT(st_.ssl.has_value());
    //     st_.ssl->shutdown(ec);
    // }

    // template <class CompletionToken>
    // void async_ssl_shutdown(CompletionToken&& token)
    // {
    //     BOOST_ASSERT(st_.ssl.has_value());
    //     st_.ssl->async_shutdown(std::forward<CompletionToken>(token));
    // }

    // // Reading
    // std::size_t read_some(asio::mutable_buffer buff, bool use_ssl, error_code& ec)
    // {
    //     if (use_ssl)
    //     {
    //         BOOST_ASSERT(st_.ssl.has_value());
    //         return st_.ssl->read_some(buff, ec);
    //     }
    //     else
    //     {
    //         return st_.sock.read_some(buff, ec);
    //     }
    // }

    // template <class CompletionToken>
    // void async_read_some(asio::mutable_buffer buff, bool use_ssl, CompletionToken&& token)
    // {
    //     if (use_ssl)
    //     {
    //         BOOST_ASSERT(st_.ssl.has_value());
    //         st_.ssl->async_read_some(buff, std::forward<CompletionToken>(token));
    //     }
    //     else
    //     {
    //         st_.sock.async_read_some(buff, std::forward<CompletionToken>(token));
    //     }
    // }

    // // Writing
    // std::size_t write_some(boost::asio::const_buffer buff, bool use_ssl, error_code& ec)
    // {
    //     if (use_ssl)
    //     {
    //         BOOST_ASSERT(st_.ssl.has_value());
    //         return st_.ssl->write_some(buff, ec);
    //     }
    //     else
    //     {
    //         return st_.sock.write_some(buff, ec);
    //     }
    // }

    // template <class CompletionToken>
    // void async_write_some(boost::asio::const_buffer buff, bool use_ssl, CompletionToken&& token)
    // {
    //     if (use_ssl)
    //     {
    //         BOOST_ASSERT(st_.ssl.has_value());
    //         return st_.ssl->async_write_some(buff, std::forward<CompletionToken>(token));
    //     }
    //     else
    //     {
    //         return st_.sock.async_write_some(buff, std::forward<CompletionToken>(token));
    //     }
    // }

    // // Connect and close
    // void connect(const void* server_address, error_code& output_ec)
    // {
    //     // Setup
    //     variant_stream_connect_algo algo(st_, *static_cast<const any_address*>(server_address));
    //     error_code ec;
    //     asio::ip::tcp::resolver::results_type resolver_results;

    //     // Run until complete
    //     while (true)
    //     {
    //         // The sync algorithm doesn't support cancellation
    //         auto act = algo.resume(ec, &resolver_results, asio::cancellation_type_t::none);
    //         switch (act.type)
    //         {
    //         case vsconnect_action_type::connect: asio::connect(st_.sock, act.data.connect, ec); break;
    //         case vsconnect_action_type::resolve:
    //             resolver_results = algo.resolver()
    //                                    .resolve(*act.data.resolve.hostname, *act.data.resolve.service, ec);
    //             break;
    //         case vsconnect_action_type::immediate: break;  // has effect only for async
    //         case vsconnect_action_type::none: output_ec = act.data.err; return;
    //         default: BOOST_ASSERT(false);  // LCOV_EXCL_LINE
    //         }
    //     }
    // }

    // template <class CompletionToken>
    // void async_connect(const void* server_address, CompletionToken&& token)
    // {
    //     asio::async_compose<CompletionToken, void(error_code)>(
    //         connect_op(*this, *static_cast<const any_address*>(server_address)),
    //         token,
    //         get_executor()
    //     );
    // }

    // void close(error_code& ec)
    // {
    //     st_.sock.shutdown(asio::generic::stream_protocol::socket::shutdown_both, ec);
    //     st_.sock.close(ec);
    // }

    // // Exposed for testing
    // const asio::generic::stream_protocol::socket& socket() const { return st_.sock; }

private:
    // variant_stream_state st_;

    // struct connect_op
    // {
    //     std::unique_ptr<variant_stream_connect_algo> algo_;

    //     connect_op(variant_stream& this_obj, const any_address& server_address)
    //         : algo_(new variant_stream_connect_algo(this_obj.st_, server_address))
    //     {
    //     }

    //     template <class Self>
    //     void operator()(
    //         Self& self,
    //         error_code ec = {},
    //         const asio::ip::tcp::resolver::results_type& resolver_results = {}
    //     )
    //     {
    //         auto act = algo_->resume(ec, &resolver_results, self.cancelled());
    //         switch (act.type)
    //         {
    //         case vsconnect_action_type::connect:
    //             asio::async_connect(algo_->socket(), act.data.connect, std::move(self));
    //             break;
    //         case vsconnect_action_type::resolve:
    //             algo_->resolver()
    //                 .async_resolve(*act.data.resolve.hostname, *act.data.resolve.service, std::move(self));
    //             break;
    //         case vsconnect_action_type::immediate:
    //             asio::dispatch(
    //                 asio::get_associated_immediate_executor(self, self.get_io_executor()),
    //                 std::move(self)
    //             );
    //             break;
    //         case vsconnect_action_type::none:
    //             algo_.reset();
    //             self.complete(act.data.err);
    //             break;
    //         default: BOOST_ASSERT(false);  // LCOV_EXCL_LINE
    //         }
    //     }

    //     // Signature for range connect
    //     template <class Self>
    //     void operator()(Self& self, error_code ec, asio::generic::stream_protocol::endpoint)
    //     {
    //         (*this)(self, ec, asio::ip::tcp::resolver::results_type{});
    //     }
    // };
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif

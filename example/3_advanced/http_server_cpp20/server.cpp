//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/awaitable.hpp>
#include <boost/pfr/config.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT) && BOOST_PFR_CORE_NAME_ENABLED

//[example_connection_pool_server_cpp
//
// File: server.cpp
//
// This file contains all the boilerplate code to implement a HTTP
// server. Functions here end up invoking handle_request.

#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/write.hpp>

#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "error.hpp"
#include "handle_request.hpp"
#include "server.hpp"

namespace asio = boost::asio;
namespace http = boost::beast::http;
namespace mysql = boost::mysql;

namespace {

struct http_endpoint
{
    std::vector<std::string_view> segments;
    http::verb method;
};

static asio::awaitable<void> run_http_session(asio::ip::tcp::socket sock, mysql::connection_pool& pool)
{
    using namespace std::chrono_literals;

    boost::system::error_code ec;

    // A buffer to read incoming client requests
    boost::beast::flat_buffer buff;

    asio::steady_timer timer(co_await asio::this_coro::executor);

    while (true)
    {
        // Construct a new parser for each message
        http::request_parser<http::string_body> parser;

        // Apply a reasonable limit to the allowed size
        // of the body in bytes to prevent abuse.
        parser.body_limit(10000);

        // Read a request
        co_await http::async_read(
            sock,
            buff,
            parser.get(),
            asio::cancel_after(timer, 60s, asio::redirect_error(ec))
        );

        if (ec)
        {
            if (ec == http::error::end_of_stream)
            {
                // This means they closed the connection
                sock.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
            }
            else
            {
                // An unknown error happened
                orders::log_error("Error reading HTTP request: ", ec);
            }
            co_return;
        }

        const auto& request = parser.get();

        // Process the request to generate a response.
        // This invokes the business logic, which will need to access MySQL data
        auto response = co_await asio::co_spawn(
            co_await asio::this_coro::executor,
            [&] { return orders::handle_request(request, pool); },
            asio::cancel_after(timer, 30s)
        );

        // Determine if we should close the connection
        bool keep_alive = response.keep_alive();
        response.version(request.version());
        response.keep_alive(keep_alive);
        response.prepare_payload();

        // Send the response
        co_await http::async_write(sock, response, asio::cancel_after(timer, 60s, asio::redirect_error(ec)));
        if (ec)
        {
            orders::log_error("Error writing HTTP response: ", ec);
            co_return;
        }

        // This means we should close the connection, usually because
        // the response indicated the "Connection: close" semantic.
        if (!keep_alive)
        {
            sock.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
            co_return;
        }
    }
}

}  // namespace

asio::awaitable<void> orders::listener(mysql::connection_pool& pool, unsigned short port)
{
    // An object that allows us to accept incoming TCP connections.
    // Since we're in a multi-threaded environment, we create a strand for the acceptor,
    // so all accept handlers are run serialized
    asio::ip::tcp::acceptor acc(asio::make_strand(co_await asio::this_coro::executor));

    // The endpoint where the server will listen. Edit this if you want to
    // change the address or port we bind to.
    asio::ip::tcp::endpoint listening_endpoint(asio::ip::make_address("0.0.0.0"), port);

    // Open the acceptor
    acc.open(listening_endpoint.protocol());

    // Allow address reuse
    acc.set_option(asio::socket_base::reuse_address(true));

    // Bind to the server address
    acc.bind(listening_endpoint);

    // Start listening for connections
    acc.listen(asio::socket_base::max_listen_connections);

    std::cout << "Server listening at " << acc.local_endpoint() << std::endl;

    // Start the acceptor loop
    while (true)
    {
        // Accept a new connection
        auto [ec, sock] = co_await acc.async_accept(asio::as_tuple);

        // If there was an error accepting the connection, exit our loop
        if (ec)
        {
            log_error("Error while accepting connection", ec);
            co_return;
        }

        // TODO: document this
        auto session_logic = [&pool, socket = std::move(sock)]() mutable {
            return run_http_session(std::move(socket), pool);
        };

        // Launch a new session for this connection. Each session gets its
        // own coroutine, so we can get back to listening for new connections.
        asio::co_spawn(
            // Every session gets its own strand. This prevents data races.
            asio::make_strand(co_await asio::this_coro::executor),

            // The actual coroutine
            std::move(session_logic),

            // All errors in the session are handled via error codes or by catching
            // exceptions explicitly. An unhandled exception here means an error.
            // Rethrowing it will propagate the exception, making io_context::run()
            // to throw and terminate the program.
            [](std::exception_ptr ex) {
                if (ex)
                    std::rethrow_exception(ex);
            }
        );
    }
}

//]

#endif

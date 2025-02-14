//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/awaitable.hpp>
#include <boost/pfr/config.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT) && BOOST_PFR_CORE_NAME_ENABLED

//[example_http_server_cpp20_server_cpp
//
// File: server.cpp
//
// This file contains all the boilerplate code to implement a HTTP
// server. Functions here end up invoking handle_request.

#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
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

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>

#include "error.hpp"
#include "handle_request.hpp"
#include "server.hpp"

namespace asio = boost::asio;
namespace http = boost::beast::http;
namespace mysql = boost::mysql;

namespace {

// Runs a single HTTP session until the client closes the connection.
// This coroutine will be spawned on a strand, to prevent data races.
asio::awaitable<void> run_http_session(asio::ip::tcp::socket sock, mysql::connection_pool& pool)
{
    using namespace std::chrono_literals;

    boost::system::error_code ec;

    // A buffer to read incoming client requests
    boost::beast::flat_buffer buff;

    // A timer, to use with asio::cancel_after to implement timeouts.
    // Re-using the same timer multiple times with cancel_after
    // is more efficient than using raw cancel_after,
    // since the timer doesn't need to be re-created for every operation.
    asio::steady_timer timer(co_await asio::this_coro::executor);

    // A HTTP session might involve more than one message if
    // keep-alive semantics are used. Loop until the connection closes.
    while (true)
    {
        // Construct a new parser for each message
        http::request_parser<http::string_body> parser;

        // Apply a reasonable limit to the allowed size
        // of the body in bytes to prevent abuse.
        parser.body_limit(10000);

        // Read a request. redirect_error prevents exceptions from being thrown
        // on error. We use cancel_after to set a timeout for the overall read operation.
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
        // This invokes the business logic, which will need to access MySQL data.
        // Apply a timeout to the overall request handling process.
        auto response = co_await asio::co_spawn(
            // Use the same executor as this coroutine (it will be a strand)
            co_await asio::this_coro::executor,

            // The logic to invoke
            [&] { return orders::handle_request(request, pool); },

            // Completion token. Returns an object that can be co_await'ed
            asio::cancel_after(timer, 30s)
        );

        // Adjust the response, setting fields common to all responses
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

asio::awaitable<void> orders::run_server(mysql::connection_pool& pool, unsigned short port)
{
    // An object that allows us to accept incoming TCP connections
    asio::ip::tcp::acceptor acc(co_await asio::this_coro::executor);

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
        asio::ip::tcp::socket sock = co_await acc.async_accept();

        // Function implementing our session logic.
        // Takes ownership of the socket.
        // Having this as a named variable workarounds a gcc bug
        // (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=107288)
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

            // Callback to run when the coroutine finishes
            [](std::exception_ptr ptr) {
                if (ptr)
                {
                    // For extra safety, log the exception but don't propagate it.
                    // If we failed to anticipate an error condition that ends up raising an exception,
                    // terminate only the affected session, instead of crashing the server.
                    try
                    {
                        std::rethrow_exception(ptr);
                    }
                    catch (const std::exception& exc)
                    {
                        auto guard = lock_cerr();
                        std::cerr << "Uncaught error in a session: " << exc.what() << std::endl;
                    }
                }
            }
        );
    }
}

//]

#endif

//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/static_results.hpp>
#ifdef BOOST_MYSQL_CXX14

//[example_http_server_cpp14_coroutines_server_cpp
//
// File: server.cpp
//
// This file contains all the boilerplate code to implement a HTTP
// server. Functions here end up invoking handle_request.

#include <boost/asio/cancel_after.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>

#include "handle_request.hpp"
#include "server.hpp"

namespace asio = boost::asio;
namespace http = boost::beast::http;
using namespace notes;

namespace {

// Runs a single HTTP session until the client closes the connection
void run_http_session(std::shared_ptr<shared_state> st, asio::ip::tcp::socket sock, asio::yield_context yield)
{
    using namespace std::chrono_literals;

    boost::system::error_code ec;

    // A buffer to read incoming client requests
    boost::beast::flat_buffer buff;

    // A timer, to use with asio::cancel_after to implement timeouts.
    // Re-using the same timer multiple times with cancel_after
    // is more efficient than using raw cancel_after,
    // since the timer doesn't need to be re-created for every operation.
    asio::steady_timer timer(yield.get_executor());

    // A HTTP session might involve more than one message if
    // keep-alive semantics are used. Loop until the connection closes.
    while (true)
    {
        // Construct a new parser for each message
        http::request_parser<http::string_body> parser;

        // Apply a reasonable limit to the allowed size
        // of the body in bytes to prevent abuse.
        parser.body_limit(10000);

        // Read a request. yield[ec] prevents exceptions from being thrown
        // on error. We use cancel_after to set a timeout for the overall read operation.
        http::async_read(sock, buff, parser.get(), asio::cancel_after(60s, yield[ec]));

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
                std::cout << "Error reading HTTP request: " << ec.message() << std::endl;
            }
            return;
        }

        const auto& request = parser.get();

        // Process the request to generate a response.
        // This invokes the business logic, which will need to access MySQL data.
        // Apply a timeout to the overall request handling process.
        auto response = asio::spawn(
            // Use the same executor as this coroutine
            yield.get_executor(),

            // The logic to invoke
            [&](asio::yield_context yield2) { return handle_request(st->pool, request, yield2); },

            // Completion token. Passing yield blocks the current coroutine
            // until handle_request completes.
            asio::cancel_after(timer, 30s, yield)
        );

        // Adjust the response, setting fields common to all responses
        bool keep_alive = response.keep_alive();
        response.version(request.version());
        response.keep_alive(keep_alive);
        response.prepare_payload();

        // Send the response
        http::async_write(sock, response, asio::cancel_after(60s, yield[ec]));
        if (ec)
        {
            std::cout << "Error writing HTTP response: " << ec.message() << std::endl;
            return;
        }

        // This means we should close the connection, usually because
        // the response indicated the "Connection: close" semantic.
        if (!keep_alive)
        {
            sock.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
            return;
        }
    }
}

}  // namespace

void notes::run_server(std::shared_ptr<shared_state> st, unsigned short port, asio::yield_context yield)
{
    // An object that allows us to accept incoming TCP connections
    asio::ip::tcp::acceptor acc(yield.get_executor());

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
        asio::ip::tcp::socket sock = acc.async_accept(yield);

        // Launch a new session for this connection. Each session gets its
        // own coroutine, so we can get back to listening for new connections.
        asio::spawn(
            yield.get_executor(),

            // Function implementing our session logic.
            // Takes ownership of the socket.
            [st, sock = std::move(sock)](asio::yield_context yield2) mutable {
                return run_http_session(std::move(st), std::move(sock), yield2);
            },

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
                        std::cerr << "Uncaught error in a session: " << exc.what() << std::endl;
                    }
                }
            }
        );
    }
}

//]

#endif

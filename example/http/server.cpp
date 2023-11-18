//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "server.hpp"

#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/json/fwd.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <boost/url/parse.hpp>

#include <exception>
#include <iostream>
#include <string>

#include "repository.hpp"
#include "types.hpp"

namespace asio = boost::asio;
namespace mysql = boost::mysql;
namespace http = boost::beast::http;
using boost::mysql::error_code;
using boost::mysql::string_view;
using namespace orders;

// An exception handler for coroutines that rethrows any exception thrown by
// the coroutine. We handle all known error cases with error_code's. If an
// exception is raised, it's something critical, e.g. out of memory.
// Re-raising it in the exception handler will case io_context::run()
// to throw and terminate the program.
static constexpr auto rethrow_handler = [](std::exception_ptr ex) {
    if (ex)
        std::rethrow_exception(ex);
};

static void log_error(error_code ec, const char* msg)
{
    std::cerr << "Error in " << msg << ": " << ec << std::endl;
}

static http::response<http::string_body> error_response(
    const http::request<http::string_body>& req,
    http::status code,
    string_view msg
)
{
    http::response<http::string_body> res;
    res.result(code);
    res.keep_alive(req.keep_alive());
    res.body() = msg;
    res.prepare_payload();
    return res;
}

static http::response<http::string_body> invalid_content_type(const http::request<http::string_body>& req)
{
    return error_response(req, http::status::bad_request, "Invalid content-type");
}

static http::response<http::string_body> invalid_body(const http::request<http::string_body>& req)
{
    return error_response(req, http::status::bad_request, "Invalid body");
}

static http::response<http::string_body> method_not_allowed(const http::request<http::string_body>& req)
{
    return error_response(req, http::status::method_not_allowed, "Method not allowed");
}

static http::response<http::string_body> endpoint_not_found(const http::request<http::string_body>& req)
{
    return error_response(req, http::status::not_found, "The requested resource was not found");
}

static http::response<http::string_body> note_not_found(const http::request<http::string_body>& req)
{
    return error_response(req, http::status::not_found, "The requested note was not found");
}

template <class T>
static http::response<http::string_body> json_response(
    const http::request<http::string_body>& req,
    const T& input
)
{
    http::response<http::string_body> res;
    res.result(http::status::ok);
    res.keep_alive(req.keep_alive());
    res.body() = boost::json::serialize(boost::json::value_from(input));
    res.prepare_payload();
    return res;
}

static bool has_json_content_type(const http::request<http::string_body>& req)
{
    auto it = req.find("Content-Type");
    return it != req.end() && it->value() == "application/json";
}

template <class T>
static boost::system::result<T> parse_json_request(const http::request<http::string_body>& req)
{
    error_code ec;
    auto val = boost::json::parse(req.body(), ec);
    if (ec)
        return ec;
    return boost::json::try_value_to<T>(val);
}

static http::response<http::string_body> handle_request_impl(
    const http::request<http::string_body>& req,
    orders::note_repository& repo,
    boost::asio::yield_context yield
)
{
    // Parse the request target
    auto url = boost::urls::parse_origin_form(req.target());
    if (url.has_error())
        return error_response(req, http::status::bad_request, "Invalid request target");

    // We will be iterating over the target's segments to determine
    // which endpoint we are being requested
    auto segs = url->segments();
    auto segit = segs.begin();
    auto seg = *segit++;

    // All endpoints start with /notes
    if (seg != "notes")
        return endpoint_not_found(req);

    if (segit == segs.end())
    {
        if (req.method() == http::verb::get)
        {
            // GET /notes
            auto res = repo.get_notes(yield);
            return json_response(req, multi_notes_response{std::move(res)});
        }
        else if (req.method() == http::verb::post)
        {
            // POST /notes. This has a JSON body with details, parse it
            if (!has_json_content_type(req))
                return invalid_content_type(req);
            auto args = parse_json_request<create_note_body>(req);
            if (args.has_error())
                return invalid_body(req);

            // Actually create the note
            auto res = repo.create_note(args->title, args->content, yield);

            // Return the newly crated note as response
            return json_response(req, single_note_response{std::move(res)});
        }
        else
        {
            return method_not_allowed(req);
        }
    }
    else
    {
        // The URL has the form /notes/<note-id>. Parse the note ID
        // TODO: could we make this non-throwing?
        auto note_id = std::stoi(*segit++);

        // /notes/<note-id>/<something-else> is not supported
        if (segit != segs.end())
            return endpoint_not_found(req);

        if (req.method() == http::verb::get)
        {
            // GET /notes/<note-id>. Retrieve the note
            auto res = repo.get_note(note_id, yield);

            // Check that we did find it
            if (!res.has_value())
                return note_not_found(req);

            // Return it as response
            return json_response(req, single_note_response{std::move(*res)});
        }
        else if (req.method() == http::verb::put)
        {
            // PUT /notes/<note-id>. This has a JSON body with details. Parse it
            if (!has_json_content_type(req))
                return invalid_content_type(req);
            auto args = parse_json_request<replace_note_request>(req);
            if (args.has_error())
                return invalid_body(req);

            // Perform the update
            auto res = repo.replace_note(note_id, args->title, args->content, yield);

            // Check that it took effect. Otherwise, it's because the note wasn't there
            if (!res.has_value())
                return note_not_found(req);

            // Return the updated note as response
            return json_response(req, single_note_response{std::move(*res)});
        }
        else if (req.method() == http::verb::delete_)
        {
            // DELETE /notes/<note-id>

            // Attempt to delete the note
            bool deleted = repo.delete_note(note_id, yield);

            // Return whether the delete was successful in the response.
            // We don't fail DELETEs for notes that don't exist.
            return json_response(req, delete_note_response{deleted});
        }
        else
        {
            return method_not_allowed(req);
        }
    }
}

static http::response<http::string_body> handle_request(
    const http::request<http::string_body>& req,
    orders::note_repository& repo,
    boost::asio::yield_context yield
)
{
    try
    {
        return handle_request_impl(req, repo, yield);
    }
    catch (const std::exception& err)
    {
        std::cerr << "Uncaught exception: " << err.what() << '\n';
        return error_response(req, http::status::internal_server_error, "Internal error");
    }
}

static void run_http_session(
    boost::asio::ip::tcp::socket&& stream,
    note_repository& repo,
    boost::asio::yield_context yield
)
{
    error_code ec;

    // A buffer to read incoming client requests
    boost::beast::flat_buffer buff;

    while (true)
    {
        // Construct a new parser for each message
        http::request_parser<http::string_body> parser;

        // Apply a reasonable limit to the allowed size
        // of the body in bytes to prevent abuse.
        parser.body_limit(10000);

        // Read a request
        http::async_read(stream, buff, parser.get(), yield[ec]);

        if (ec)
        {
            if (ec == http::error::end_of_stream)
            {
                // This means they closed the connection
                stream.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
            }
            else
            {
                // An unknown error happened
                log_error(ec, "read");
            }
            return;
        }

        // Process the request to generate a response.
        // This invokes the business logic, which will need to access MySQL data
        auto response = handle_request(parser.get(), repo, yield);

        // Determine if we should close the connection
        bool keep_alive = response.keep_alive();

        // Send the response
        http::async_write(stream, response, yield[ec]);
        if (ec)
            return log_error(ec, "write");

        // This means we should close the connection, usually because
        // the response indicated the "Connection: close" semantic.
        if (!keep_alive)
        {
            stream.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
            return;
        }
    }
}

// The actual accept loop, coroutine-based
static void accept_loop(asio::ip::tcp::acceptor acceptor, note_repository& repo, asio::yield_context yield)
{
    error_code ec;

    // We accept connections in an infinite loop. When the io_context is stopped,
    // coroutines are "cancelled" by throwing an internal exception, exiting
    // the loop.
    while (true)
    {
        // Accept a new connection
        auto sock = acceptor.async_accept(yield[ec]);
        if (ec)
            return log_error(ec, "accept");

        // Launch a new session for this connection. Each session gets its
        // own stackful coroutine, so we can get back to listening for new connections.
        boost::asio::spawn(
            sock.get_executor(),
            [&repo, socket = std::move(sock)](boost::asio::yield_context yield) mutable {
                run_http_session(std::move(socket), repo, yield);
            },
            rethrow_handler  // Propagate exceptions to the io_context
        );
    }
}

error_code orders::launch_server(boost::asio::io_context& ctx, unsigned short port, note_repository& repo)
{
    error_code ec;

    // An object that allows us to acept incoming TCP connections
    asio::ip::tcp::acceptor acceptor{ctx};

    boost::asio::ip::tcp::endpoint listening_endpoint(boost::asio::ip::make_address("0.0.0.0"), port);

    // Open the acceptor
    acceptor.open(listening_endpoint.protocol(), ec);
    if (ec)
        return ec;

    // Allow address reuse
    acceptor.set_option(asio::socket_base::reuse_address(true), ec);
    if (ec)
        return ec;

    // Bind to the server address
    acceptor.bind(listening_endpoint, ec);
    if (ec)
        return ec;

    // Start listening for connections
    acceptor.listen(asio::socket_base::max_listen_connections, ec);
    if (ec)
        return ec;

    // Spawn a coroutine that will accept the connections. From this point,
    // everything is handled asynchronously, with stackful coroutines.
    boost::asio::spawn(
        ctx,
        [acceptor = std::move(acceptor), &repo](boost::asio::yield_context yield) mutable {
            accept_loop(std::move(acceptor), repo, yield);
        },
        rethrow_handler  // Propagate exceptions to the io_context
    );

    return error_code();
}
//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "server.hpp"

#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
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
#include <memory>
#include <string>

#include "repository.hpp"
#include "types.hpp"

// This file contains all the boilerplate code to implement a HTTP
// server that provides the REST API described in main.cpp.
// Functions here end up invoking note_repository member functions.

namespace asio = boost::asio;
namespace http = boost::beast::http;
using boost::mysql::error_code;
using boost::mysql::string_view;
using namespace orders;

namespace {

static void log_error(error_code ec, const char* msg)
{
    std::cerr << "Error in " << msg << ": " << ec << std::endl;
}

class request_handler
{
    const http::request<http::string_body>& request_;
    note_repository repo_;

    http::response<http::string_body> error_response(http::status code, string_view msg) const
    {
        http::response<http::string_body> res;
        res.result(code);
        res.keep_alive(request_.keep_alive());
        res.body() = msg;
        res.prepare_payload();
        return res;
    }

    http::response<http::string_body> invalid_content_type() const
    {
        return error_response(http::status::bad_request, "Invalid content-type");
    }

    http::response<http::string_body> invalid_body() const
    {
        return error_response(http::status::bad_request, "Invalid body");
    }

    http::response<http::string_body> method_not_allowed() const
    {
        return error_response(http::status::method_not_allowed, "Method not allowed");
    }

    http::response<http::string_body> endpoint_not_found() const
    {
        return error_response(http::status::not_found, "The requested resource was not found");
    }

    http::response<http::string_body> note_not_found() const
    {
        return error_response(http::status::not_found, "The requested note was not found");
    }

    template <class T>
    http::response<http::string_body> json_response(const T& input) const
    {
        http::response<http::string_body> res;
        res.result(http::status::ok);
        res.keep_alive(request_.keep_alive());
        res.body() = boost::json::serialize(boost::json::value_from(input));
        res.prepare_payload();
        return res;
    }

    bool has_json_content_type() const
    {
        auto it = request_.find("Content-Type");
        return it != request_.end() && it->value() == "application/json";
    }

    template <class T>
    boost::system::result<T> parse_json_request() const
    {
        error_code ec;
        auto val = boost::json::parse(request_.body(), ec);
        if (ec)
            return ec;
        return boost::json::try_value_to<T>(val);
    }

    http::response<http::string_body> handle_request_impl(boost::asio::yield_context yield)
    {
        // Parse the request target
        auto url = boost::urls::parse_origin_form(request_.target());
        if (url.has_error())
            return error_response(http::status::bad_request, "Invalid request target");

        // We will be iterating over the target's segments to determine
        // which endpoint we are being requested
        auto segs = url->segments();
        auto segit = segs.begin();
        auto seg = *segit++;

        // All endpoints start with /notes
        if (seg != "notes")
            return endpoint_not_found();

        if (segit == segs.end())
        {
            if (request_.method() == http::verb::get)
            {
                // GET /notes
                auto res = repo_.get_notes(yield);
                return json_response(multi_notes_response{std::move(res)});
            }
            else if (request_.method() == http::verb::post)
            {
                // POST /notes. This has a JSON body with details, parse it
                if (!has_json_content_type())
                    return invalid_content_type();
                auto args = parse_json_request<note_request_body>();
                if (args.has_error())
                    return invalid_body();

                // Actually create the note
                auto res = repo_.create_note(args->title, args->content, yield);

                // Return the newly crated note as response
                return json_response(single_note_response{std::move(res)});
            }
            else
            {
                return method_not_allowed();
            }
        }
        else
        {
            // The URL has the form /notes/<note-id>. Parse the note ID
            // TODO: could we make this non-throwing?
            auto note_id = std::stoi(*segit++);

            // /notes/<note-id>/<something-else> is not supported
            if (segit != segs.end())
                return endpoint_not_found();

            if (request_.method() == http::verb::get)
            {
                // GET /notes/<note-id>. Retrieve the note
                auto res = repo_.get_note(note_id, yield);

                // Check that we did find it
                if (!res.has_value())
                    return note_not_found();

                // Return it as response
                return json_response(single_note_response{std::move(*res)});
            }
            else if (request_.method() == http::verb::put)
            {
                // PUT /notes/<note-id>. This has a JSON body with details. Parse it
                if (!has_json_content_type())
                    return invalid_content_type();
                auto args = parse_json_request<note_request_body>();
                if (args.has_error())
                    return invalid_body();

                // Perform the update
                auto res = repo_.replace_note(note_id, args->title, args->content, yield);

                // Check that it took effect. Otherwise, it's because the note wasn't there
                if (!res.has_value())
                    return note_not_found();

                // Return the updated note as response
                return json_response(single_note_response{std::move(*res)});
            }
            else if (request_.method() == http::verb::delete_)
            {
                // DELETE /notes/<note-id>

                // Attempt to delete the note
                bool deleted = repo_.delete_note(note_id, yield);

                // Return whether the delete was successful in the response.
                // We don't fail DELETEs for notes that don't exist.
                return json_response(delete_note_response{deleted});
            }
            else
            {
                return method_not_allowed();
            }
        }
    }

public:
    request_handler(const http::request<http::string_body>& req, note_repository repo)
        : request_(req), repo_(repo)
    {
    }

    http::response<http::string_body> handle_request(boost::asio::yield_context yield)
    {
        try
        {
            return handle_request_impl(yield);
        }
        catch (const boost::mysql::error_with_diagnostics& err)
        {
            std::cerr << "Uncaught exception: " << err.what()
                      << "\nServer diagnostics: " << err.get_diagnostics().server_message() << '\n';
            return error_response(http::status::internal_server_error, "Internal error");
        }
        catch (const std::exception& err)
        {
            std::cerr << "Uncaught exception: " << err.what() << '\n';
            return error_response(http::status::internal_server_error, "Internal error");
        }
    }
};

static void run_http_session(
    boost::asio::ip::tcp::socket sock,
    std::shared_ptr<shared_state> st,
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
        http::async_read(sock, buff, parser.get(), yield[ec]);

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
                log_error(ec, "read");
            }
            return;
        }

        // Process the request to generate a response.
        // This invokes the business logic, which will need to access MySQL data
        auto response = request_handler(parser.get(), note_repository(st->pool)).handle_request(yield);

        // Determine if we should close the connection
        bool keep_alive = response.keep_alive();

        // Send the response
        http::async_write(sock, response, yield[ec]);
        if (ec)
            return log_error(ec, "write");

        // This means we should close the connection, usually because
        // the response indicated the "Connection: close" semantic.
        if (!keep_alive)
        {
            sock.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
            return;
        }
    }
}

// Implements the server's accept loop. The server will
// listen for connections until stopped.
static void do_accept(std::shared_ptr<asio::ip::tcp::acceptor> acceptor, std::shared_ptr<shared_state> st)
{
    acceptor->async_accept([st, acceptor](error_code ec, asio::ip::tcp::socket sock) {
        // If there was an error accepting the connection, exit our loop
        if (ec)
            return log_error(ec, "accept");

        // Launch a new session for this connection. Each session gets its
        // own stackful coroutine, so we can get back to listening for new connections.
        boost::asio::spawn(
            sock.get_executor(),
            [st, socket = std::move(sock)](boost::asio::yield_context yield) mutable {
                run_http_session(std::move(socket), st, yield);
            },
            // All errors in the session are handled via error codes or by catching
            // exceptions explicitly. An unhandled exception here means an error.
            // Rethrowing it will propagate the exception, making io_context::run()
            // to throw and terminate the program.
            [](std::exception_ptr ex) {
                if (ex)
                    std::rethrow_exception(ex);
            }
        );

        // Accept a new connection
        do_accept(acceptor, st);
    });
}

}  // namespace

error_code orders::launch_server(boost::asio::io_context& ctx, std::shared_ptr<shared_state> st)
{
    error_code ec;

    // An object that allows us to acept incoming TCP connections
    auto acceptor = std::make_shared<asio::ip::tcp::acceptor>(ctx);

    boost::asio::ip::tcp::endpoint listening_endpoint(boost::asio::ip::make_address("0.0.0.0"), 4000);

    // Open the acceptor
    acceptor->open(listening_endpoint.protocol(), ec);
    if (ec)
        return ec;

    // Allow address reuse
    acceptor->set_option(asio::socket_base::reuse_address(true), ec);
    if (ec)
        return ec;

    // Bind to the server address
    acceptor->bind(listening_endpoint, ec);
    if (ec)
        return ec;

    // Start listening for connections
    acceptor->listen(asio::socket_base::max_listen_connections, ec);
    if (ec)
        return ec;

    // Launch the acceptor loop
    do_accept(std::move(acceptor), std::move(st));

    return error_code();
}
//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/static_results.hpp>
#ifdef BOOST_MYSQL_CXX14

//[example_http_server_cpp14_coroutines_handle_request_cpp
//
// File: handle_request.cpp
//
// This file contains all the boilerplate code to dispatch HTTP
// requests to API endpoints. Functions here end up calling
// note_repository functions.

#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/asio/spawn.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/charconv/from_chars.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <boost/optional/optional.hpp>
#include <boost/url/parse.hpp>

#include <cstdint>
#include <exception>
#include <iostream>
#include <string>

#include "handle_request.hpp"
#include "repository.hpp"
#include "types.hpp"

namespace asio = boost::asio;
namespace mysql = boost::mysql;
namespace http = boost::beast::http;
using namespace notes;

namespace {

// Helper function that logs errors thrown by db_repository
// when an unexpected database error happens
void log_mysql_error(boost::system::error_code ec, const mysql::diagnostics& diag)
{
    // Inserting the error code only prints the number and category. Add the message, too.
    std::cerr << "MySQL error: " << ec << " " << ec.message();

    // client_message() contains client-side generated messages that don't
    // contain user-input. This is usually embedded in exceptions.
    // When working with error codes, we need to log it explicitly
    if (!diag.client_message().empty())
    {
        std::cerr << ": " << diag.client_message();
    }

    // server_message() contains server-side messages, and thus may
    // contain user-supplied input. Printing it is safe.
    if (!diag.server_message().empty())
    {
        std::cerr << ": " << diag.server_message();
    }

    // Done
    std::cerr << std::endl;
}

// Attempts to parse a numeric ID from a string.
// If you're using C++17, you can use std::from_chars, instead
boost::optional<std::int64_t> parse_id(const std::string& from)
{
    std::int64_t id{};
    auto res = boost::charconv::from_chars(from.data(), from.data() + from.size(), id);
    if (res.ec != std::errc{} || res.ptr != from.data() + from.size())
        return {};
    return id;
}

// Helpers to create error responses with a single line of code
http::response<http::string_body> error_response(http::status code, const char* msg)
{
    http::response<http::string_body> res;
    res.result(code);
    res.body() = msg;
    return res;
}

// Like error_response, but always uses a 400 status code
http::response<http::string_body> bad_request(const char* body)
{
    return error_response(http::status::bad_request, body);
}

// Like error_response, but always uses a 500 status code and
// never provides extra information that might help potential attackers.
http::response<http::string_body> internal_server_error()
{
    return error_response(http::status::internal_server_error, "Internal server error");
}

// Creates a response with a serialized JSON body.
// T should be a type with Boost.Describe metadata containing the
// body data to be serialized
template <class T>
http::response<http::string_body> json_response(const T& body)
{
    http::response<http::string_body> res;

    // Set the content-type header
    res.set("Content-Type", "application/json");

    // Serialize the body data into a string and use it as the response body.
    // We use Boost.JSON's automatic serialization feature, which uses Boost.Describe
    // reflection data to generate a serialization function for us.
    res.body() = boost::json::serialize(boost::json::value_from(body));

    // Done
    return res;
}

// Returns true if the request's Content-Type is set to JSON
bool has_json_content_type(const http::request<http::string_body>& req)
{
    auto it = req.find("Content-Type");
    return it != req.end() && it->value() == "application/json";
}

// Attempts to parse a string as a JSON into an object of type T.
// T should be a type with Boost.Describe metadata.
// We use boost::system::result, which may contain a result or an error.
template <class T>
boost::system::result<T> parse_json(boost::mysql::string_view json_string)
{
    // Attempt to parse the request into a json::value.
    // This will fail if the provided body isn't valid JSON.
    boost::system::error_code ec;
    auto val = boost::json::parse(json_string, ec);
    if (ec)
        return ec;

    // Attempt to parse the json::value into a T. This will
    // fail if the provided JSON doesn't match T's shape.
    return boost::json::try_value_to<T>(val);
}

// Contains data associated to an HTTP request.
// To be passed to individual handler functions
struct request_data
{
    // The incoming request
    const http::request<http::string_body>& request;

    // The URL the request is targeting
    boost::urls::url_view target;

    // Connection pool
    mysql::connection_pool& pool;

    note_repository repo() const { return note_repository(pool); }
};

//
// Endpoint handlers. We have a function per method.
// All of our endpoints have /notes as the URL path.
//

// GET /notes: retrieves all the notes.
// The request doesn't have a body.
// The response has a JSON body with multi_notes_response format
//
// GET /notes?id=<note-id>: retrieves a single note.
// The request doesn't have a body.
// The response has a JSON body with single_note_response format
//
// Both endpoints share path and method, so they share handler function
http::response<http::string_body> handle_get(const request_data& input, asio::yield_context yield)
{
    // Parse the query parameter
    auto params_it = input.target.params().find("id");

    // Did the client specify an ID?
    if (params_it == input.target.params().end())
    {
        auto res = input.repo().get_notes(yield);
        return json_response(multi_notes_response{std::move(res)});
    }
    else
    {
        // Parse id
        auto id = parse_id((*params_it).value);
        if (!id.has_value())
            return bad_request("URL parameter 'id' should be a valid integer");

        // Get the note
        auto res = input.repo().get_note(*id, yield);

        // If we didn't find it, return a 404 error
        if (!res.has_value())
            return error_response(http::status::not_found, "The requested note was not found");

        // Return it as response
        return json_response(single_note_response{std::move(*res)});
    }
}

// POST /notes: creates a note.
// The request has a JSON body with note_request_body format.
// The response has a JSON body with single_note_response format.
http::response<http::string_body> handle_post(const request_data& input, asio::yield_context yield)
{
    // Parse the request body
    if (!has_json_content_type(input.request))
        return bad_request("Invalid Content-Type: expected 'application/json'");
    auto args = parse_json<note_request_body>(input.request.body());
    if (args.has_error())
        return bad_request("Invalid JSON");

    // Actually create the note
    auto res = input.repo().create_note(args->title, args->content, yield);

    // Return the newly created note as response
    return json_response(single_note_response{std::move(res)});
}

// PUT /notes?id=<note-id>: replaces a note.
// The request has a JSON body with note_request_body format.
// The response has a JSON body with single_note_response format.
http::response<http::string_body> handle_put(const request_data& input, asio::yield_context yield)
{
    // Parse the query parameter
    auto params_it = input.target.params().find("id");
    if (params_it == input.target.params().end())
        return bad_request("Mandatory URL parameter 'id' not found");
    auto id = parse_id((*params_it).value);
    if (!id.has_value())
        return bad_request("URL parameter 'id' should be a valid integer");

    // Parse the request body
    if (!has_json_content_type(input.request))
        return bad_request("Invalid Content-Type: expected 'application/json'");
    auto args = parse_json<note_request_body>(input.request.body());
    if (args.has_error())
        return bad_request("Invalid JSON");

    // Perform the update
    auto res = input.repo().replace_note(*id, args->title, args->content, yield);

    // Check that it took effect. Otherwise, it's because the note wasn't there
    if (!res.has_value())
        return bad_request("The requested note was not found");

    // Return the updated note as response
    return json_response(single_note_response{std::move(*res)});
}

// DELETE /notes/<note-id>: deletes a note.
// The request doesn't have a body.
// The response has a JSON body with delete_note_response format.
http::response<http::string_body> handle_delete(const request_data& input, asio::yield_context yield)
{
    // Parse the query parameter
    auto params_it = input.target.params().find("id");
    if (params_it == input.target.params().end())
        return bad_request("Mandatory URL parameter 'id' not found");
    auto id = parse_id((*params_it).value);
    if (!id.has_value())
        return bad_request("URL parameter 'id' should be a valid integer");

    // Attempt to delete the note
    bool deleted = input.repo().delete_note(*id, yield);

    // Return whether the delete was successful in the response.
    // We don't fail DELETEs for notes that don't exist.
    return json_response(delete_note_response{deleted});
}

}  // namespace

// External interface
http::response<http::string_body> notes::handle_request(
    mysql::connection_pool& pool,
    const http::request<http::string_body>& request,
    asio::yield_context yield
)
{
    // Parse the request target
    auto target = boost::urls::parse_origin_form(request.target());
    if (!target.has_value())
        return bad_request("Invalid request target");

    // All our endpoints have /notes as path, with different verbs and parameters.
    // Verify that the path matches
    if (target->path() != "/notes")
        return error_response(http::status::not_found, "Endpoint not found");

    // Compose the request_data object
    request_data input{request, *target, pool};

    // Invoke the relevant handler, depending on the method
    try
    {
        switch (input.request.method())
        {
        case http::verb::get: return handle_get(input, yield);
        case http::verb::post: return handle_post(input, yield);
        case http::verb::put: return handle_put(input, yield);
        case http::verb::delete_: return handle_delete(input, yield);
        default: return error_response(http::status::method_not_allowed, "Method not allowed for /notes");
        }
    }
    catch (const mysql::error_with_diagnostics& err)
    {
        // A Boost.MySQL error. This will happen if you don't have connectivity
        // to your database, your schema is incorrect or your credentials are invalid.
        // Log the error, including diagnostics
        log_mysql_error(err.code(), err.get_diagnostics());

        // Never disclose error info to a potential attacker
        return internal_server_error();
    }
    catch (const std::exception& err)
    {
        // Another kind of error. This indicates a programming error or a severe
        // server condition (e.g. out of memory). Same procedure as above.
        std::cerr << "Uncaught exception: " << err.what() << std::endl;
        return internal_server_error();
    }
}

//]

#endif

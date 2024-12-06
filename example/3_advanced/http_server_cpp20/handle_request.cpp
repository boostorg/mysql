//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/static_results.hpp>

//[example_connection_pool_handle_request_cpp
//
// File: handle_request.cpp
//

#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/http/message_fwd.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <boost/optional/optional.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url_view.hpp>
#include <boost/variant2/variant.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

#include "handle_request.hpp"
#include "log_error.hpp"
#include "repository.hpp"
#include "types.hpp"

// This file contains all the boilerplate code to dispatch HTTP
// requests to API endpoints. Functions here end up calling
// note_repository fuctions.

namespace asio = boost::asio;
namespace http = boost::beast::http;
namespace mysql = boost::mysql;

namespace {

// Attempts to parse a numeric ID from a string
static std::optional<std::int64_t> parse_id(std::string_view from)
{
    std::int64_t id{};
    auto res = std::from_chars(from.data(), from.data() + from.size(), id);
    if (res.ec != std::errc{} || res.ptr != from.data() + from.size())
        return std::nullopt;
    return id;
}

// Encapsulates the logic required to match a HTTP request
// to an API endpoint, call the relevant note_repository function,
// and return an HTTP response.
class request_handler
{
public:
    // The HTTP request we're handling. Requests are small in size,
    // so we use http::request<http::string_body>
    const http::request<http::string_body>& request_;
    boost::urls::url_view target;

    // The repository to access MySQL
    orders::db_repository repo_;

    // Creates an error response
    http::response<http::string_body> error_response(http::status code, std::string_view msg) const
    {
        http::response<http::string_body> res;
        res.result(code);
        res.body() = msg;
        return res;
    }

    // TODO
    http::response<http::string_body> bad_request(std::string body) const
    {
        return error_response(http::status::bad_request, std::move(body));
    }

    // Used when the request's Content-Type header doesn't match what we expect
    http::response<http::string_body> invalid_content_type() const
    {
        return error_response(http::status::bad_request, "Invalid content-type");
    }

    // Used when the request body didn't match the format we expect
    http::response<http::string_body> invalid_body() const
    {
        return error_response(http::status::bad_request, "Invalid body");
    }

    // Used when the request's method didn't match the ones allowed by the endpoint
    http::response<http::string_body> method_not_allowed() const
    {
        return error_response(http::status::method_not_allowed, "Method not allowed");
    }

    // Used when the user requested a note (e.g. using GET /note/<id> or PUT /note/<id>)
    // but the note doesn't exist
    http::response<http::string_body> not_found(std::string body = "The requested resource was not found")
        const
    {
        return error_response(http::status::not_found, std::move(body));
    }

    // Creates a response with a serialized JSON body.
    // T should be a type with Boost.Describe metadata containing the
    // body data to be serialized
    template <class T>
    http::response<http::string_body> json_response(const T& body) const
    {
        http::response<http::string_body> res;

        // Set the content-type header
        res.set("Content-Type", "application/json");

        // Set the keep-alive option
        res.keep_alive(request_.keep_alive());

        // Serialize the body data into a string and use it as the response body.
        // We use Boost.JSON's automatic serialization feature, which uses Boost.Describe
        // reflection data to generate a serialization function for us.
        res.body() = boost::json::serialize(boost::json::value_from(body));

        // Adjust the content-length header
        res.prepare_payload();

        // Done
        return res;
    }

    // Returns true if the request's Content-Type is set to JSON
    bool has_json_content_type() const
    {
        auto it = request_.find("Content-Type");
        return it != request_.end() && it->value() == "application/json";
    }

    // Attempts to parse the request body as a JSON into an object of type T.
    // T should be a type with Boost.Describe metadata.
    // We use boost::system::result, which may contain a result or an error.
    template <class T>
    boost::system::result<T> parse_json_request() const
    {
        boost::system::error_code ec;

        // Attempt to parse the request into a json::value.
        // This will fail if the provided body isn't valid JSON.
        auto val = boost::json::parse(request_.body(), ec);
        if (ec)
            return ec;

        // Attempt to parse the json::value into a T. This will
        // fail if the provided JSON doesn't match T's shape.
        return boost::json::try_value_to<T>(val);
    }

    // asio::awaitable<http::response<http::string_body>> handle_request_impl()
    // {
    //     // Parse the request target. We use Boost.Url to do this.
    //     auto url = boost::urls::parse_origin_form(request_.target());
    //     if (url.has_error())
    //         co_return error_response(http::status::bad_request, "Invalid request target");

    //     // We will be iterating over the target's segments to determine
    //     // which endpoint we are being requested
    //     auto url_params = url->params();
    //     auto segs = url->segments();
    //     auto segit = segs.begin();
    //     auto seg = *segit++;

    //     // Endpoints starting with /products
    //     if (seg == "products" && segit == segs.end())
    //     {
    //         if (request_.method() == http::verb::get)
    //         {
    //             // Invoke the database logic
    //             // vector<product> (string search)
    //         }
    //         else
    //         {
    //             co_return method_not_allowed();
    //         }
    //     }
    //     // Endpoints starting with /orders
    //     else if (seg == "orders")
    //     {
    //         if (segit == segs.end())
    //         {
    //             if (request_.method() ==)
    //         }
    //     }

    //     // All endpoints start with /notes
    //     if (seg != "notes")
    //         co_return endpoint_not_found();

    //     if (segit == segs.end())
    //     {
    //         if (request_.method() == http::verb::get)
    //         {
    //             // GET /notes: retrieves all the notes.
    //             // The request doesn't have a body.
    //             // The response has a JSON body with multi_notes_response format
    //             auto res = repo_.get_notes(yield);
    //             return json_response(multi_notes_response{std::move(res)});
    //         }
    //         else if (request_.method() == http::verb::post)
    //         {
    //             // POST /notes: creates a note.
    //             // The request has a JSON body with note_request_body format.
    //             // The response has a JSON body with single_note_response format.

    //             // Parse the request body
    //             if (!has_json_content_type())
    //                 return invalid_content_type();
    //             auto args = parse_json_request<note_request_body>();
    //             if (args.has_error())
    //                 return invalid_body();

    //             // Actually create the note
    //             auto res = repo_.create_note(args->title, args->content, yield);

    //             // Return the newly crated note as response
    //             return json_response(single_note_response{std::move(res)});
    //         }
    //         else
    //         {
    //             return method_not_allowed();
    //         }
    //     }
    //     else
    //     {
    //         // The URL has the form /notes/<note-id>. Parse the note ID.
    //         auto note_id = parse_id(*segit++);
    //         if (!note_id.has_value())
    //         {
    //             return error_response(
    //                 http::status::bad_request,
    //                 "Invalid note_id specified in request target"
    //             );
    //         }

    //         // /notes/<note-id>/<something-else> is not a valid endpoint
    //         if (segit != segs.end())
    //             return endpoint_not_found();

    //         if (request_.method() == http::verb::get)
    //         {
    //             // GET /notes/<note-id>: retrieves a single note.
    //             // The request doesn't have a body.
    //             // The response has a JSON body with single_note_response format

    //             // Get the note
    //             auto res = repo_.get_note(*note_id, yield);

    //             // If we didn't find it, return a 404 error
    //             if (!res.has_value())
    //                 return note_not_found();

    //             // Return it as response
    //             return json_response(single_note_response{std::move(*res)});
    //         }
    //         else if (request_.method() == http::verb::put)
    //         {
    //             // PUT /notes/<note-id>: replaces a note.
    //             // The request has a JSON body with note_request_body format.
    //             // The response has a JSON body with single_note_response format.

    //             // Parse the JSON body
    //             if (!has_json_content_type())
    //                 return invalid_content_type();
    //             auto args = parse_json_request<note_request_body>();
    //             if (args.has_error())
    //                 return invalid_body();

    //             // Perform the update
    //             auto res = repo_.replace_note(*note_id, args->title, args->content, yield);

    //             // Check that it took effect. Otherwise, it's because the note wasn't there
    //             if (!res.has_value())
    //                 return note_not_found();

    //             // Return the updated note as response
    //             return json_response(single_note_response{std::move(*res)});
    //         }
    //         else if (request_.method() == http::verb::delete_)
    //         {
    //             // DELETE /notes/<note-id>: deletes a note.
    //             // The request doesn't have a body.
    //             // The response has a JSON body with delete_note_response format.

    //             // Attempt to delete the note
    //             bool deleted = repo_.delete_note(*note_id, yield);

    //             // Return whether the delete was successful in the response.
    //             // We don't fail DELETEs for notes that don't exist.
    //             return json_response(delete_note_response{deleted});
    //         }
    //         else
    //         {
    //             return method_not_allowed();
    //         }
    //     }
    // }

    // Constructor
    request_handler(const http::request<http::string_body>& req, mysql::connection_pool& pool) noexcept
        : request_(req), repo_(pool)
    {
    }
};

// GET /products: search for available products
asio::awaitable<http::response<http::string_body>> handle_get_products(request_handler& handler)
{
    // Parse the query parameter
    auto params_it = handler.target.params().find("search");
    if (params_it == handler.target.params().end())
        co_return handler.bad_request("Missing mandatory query parameter: 'search'");
    auto search = (*params_it).value;

    // Invoke the database logic
    std::vector<orders::product> products = co_await handler.repo_.get_products(search);

    // Return the response
    co_return handler.json_response(products);
}

asio::awaitable<http::response<http::string_body>> handle_get_orders(request_handler& handler)
{
    // Parse the query parameter
    auto params_it = handler.target.params().find("id");

    if (params_it == handler.target.params().end())
    {
        // If the query parameter is not present, return all orders
        // Invoke the database logic
        std::vector<orders::order> orders = co_await handler.repo_.get_orders();

        // Return the response
        co_return handler.json_response(orders);
    }
    else
    {
        // Otherwise, query by ID
        // Parse the query parameter
        auto order_id = parse_id((*params_it).value);
        if (!order_id.has_value())
            co_return handler.bad_request("id should be a valid integer");

        // Invoke the database logic
        std::optional<orders::order_with_items> order = co_await handler.repo_.get_order_by_id(*order_id);

        // Return the response
        if (!order.has_value())
            co_return handler.not_found("Order not found");
        co_return handler.json_response(*order);
    }
}

asio::awaitable<http::response<http::string_body>> handle_create_order(request_handler& handler)
{
    // Invoke the database logic
    orders::order_with_items order = co_await handler.repo_.create_order();

    // Return the response
    co_return handler.json_response(order);
}

asio::awaitable<http::response<http::string_body>> handle_add_item(request_handler& handler)
{
    // TODO
}

struct http_endpoint
{
    http::verb method;
    asio::awaitable<http::response<http::string_body>> (*handler)(request_handler&);
};

const std::unordered_multimap<std::string_view, http_endpoint> endpoint_map{
    {"/products", {http::verb::get, &handle_get_products} },
    {"/orders",   {http::verb::get, &handle_get_orders}   },
    {"/orders",   {http::verb::post, &handle_create_order}}
};

}  // namespace

// External interface
asio::awaitable<http::response<http::string_body>> orders::handle_request(
    const http::request<http::string_body>& request,
    mysql::connection_pool& pool
)
{
    request_handler handler(pool);

    // Try to find an endpoint
    auto it = endpoint_map.find(handler.target.path());
    if (it == endpoint_map.end())
    {
        co_return handler.endpoint_not_found();
    }

    // Match the verb
    auto it2 = std::find_if(

    )

    try
    {
        // Attempt to handle the request. We use cancel_after to set
        // a timeout to the overall operation
        return asio::spawn(
            yield.get_executor(),
            [this](asio::yield_context yield2) { return handle_request_impl(yield2); },
            asio::cancel_after(std::chrono::seconds(30), yield)
        );
    }
    catch (const boost::mysql::error_with_diagnostics& err)
    {
        // A Boost.MySQL error. This will happen if you don't have connectivity
        // to your database, your schema is incorrect or your credentials are invalid.
        // Log the error, including diagnostics, and return a generic 500
        log_error(
            "Uncaught exception: ",
            err.what(),
            "\nServer diagnostics: ",
            err.get_diagnostics().server_message()
        );
        return error_response(http::status::internal_server_error, "Internal error");
    }
    catch (const std::exception& err)
    {
        // Another kind of error. This indicates a programming error or a severe
        // server condition (e.g. out of memory). Same procedure as above.
        log_error("Uncaught exception: ", err.what());
        return error_response(http::status::internal_server_error, "Internal error");
    }
}

//]

#endif

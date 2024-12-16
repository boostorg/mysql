//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/awaitable.hpp>
#include <boost/pfr/config.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT) && BOOST_PFR_CORE_NAME_ENABLED

//[example_connection_pool_handle_request_cpp
//
// File: handle_request.cpp
//

#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/static_results.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/http/message_fwd.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/json/error.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <boost/optional/optional.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/system/result.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url_view.hpp>
#include <boost/variant2/variant.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

#include "error.hpp"
#include "handle_request.hpp"
#include "repository.hpp"
#include "types.hpp"

// This file contains all the boilerplate code to dispatch HTTP
// requests to API endpoints. Functions here end up calling
// note_repository fuctions.

namespace asio = boost::asio;
namespace http = boost::beast::http;
namespace mysql = boost::mysql;
using boost::system::result;

namespace {

void log_mysql_error(boost::system::error_code ec, const mysql::diagnostics& diag)
{
    // Lock std::cerr, to avoid race conditions
    auto guard = orders::lock_cerr();

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

// Attempts to parse a numeric ID from a string
std::optional<std::int64_t> parse_id(std::string_view from)
{
    std::int64_t id{};
    auto res = std::from_chars(from.data(), from.data() + from.size(), id);
    if (res.ec != std::errc{} || res.ptr != from.data() + from.size())
        return std::nullopt;
    return id;
}

// Creates an error response
http::response<http::string_body> error_response(http::status code, std::string_view msg)
{
    http::response<http::string_body> res;
    res.result(code);
    res.body() = msg;
    return res;
}

// TODO
http::response<http::string_body> bad_request(std::string body)
{
    return error_response(http::status::bad_request, std::move(body));
}

// Used when the user requested a note (e.g. using GET /note/<id> or PUT /note/<id>)
// but the note doesn't exist
http::response<http::string_body> not_found(std::string body = "The requested resource was not found")
{
    return error_response(http::status::not_found, std::move(body));
}

http::response<http::string_body> unprocessable_entity(std::string body)
{
    return error_response(http::status::unprocessable_entity, std::move(body));
}

http::response<http::string_body> internal_server_error()
{
    return error_response(http::status::internal_server_error, {});
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

// Attempts to parse a string as a JSON into an object of type T.
// T should be a type with Boost.Describe metadata.
// We use boost::system::result, which may contain a result or an error.
template <class T>
result<T> parse_json(std::string_view json_string)
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

http::response<http::string_body> response_from_db_error(boost::system::error_code ec)
{
    if (ec.category() == orders::get_orders_category())
    {
        switch (static_cast<orders::errc>(ec.value()))
        {
        case orders::errc::not_found: return not_found("The referenced entity does not exist");
        case orders::errc::product_not_found:
            return unprocessable_entity("The referenced product does not exist");
        case orders::errc::order_invalid_status:
            return unprocessable_entity(
                "The referenced order doesn't have the status required by the operation"
            );
        default: return internal_server_error();
        }
    }
    else
    {
        return internal_server_error();
    }
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

    orders::db_repository repo() const { return orders::db_repository(pool); }
};

// GET /products: search for available products
asio::awaitable<http::response<http::string_body>> handle_get_products(const request_data& input)
{
    // Parse the query parameter
    auto params_it = input.target.params().find("search");
    if (params_it == input.target.params().end())
        co_return bad_request("Missing mandatory query parameter: 'search'");
    auto search = (*params_it).value;

    // Invoke the database logic
    std::vector<orders::product> products = co_await input.repo().get_products(search);

    // Return the response
    co_return json_response(products);
}

asio::awaitable<http::response<http::string_body>> handle_get_orders(const request_data& input)
{
    // Parse the query parameter
    auto params_it = input.target.params().find("id");

    if (params_it == input.target.params().end())
    {
        // If the query parameter is not present, return all orders
        // Invoke the database logic
        std::vector<orders::order> orders = co_await input.repo().get_orders();

        // Return the response
        co_return json_response(orders);
    }
    else
    {
        // Otherwise, query by ID
        // Parse the query parameter
        auto order_id = parse_id((*params_it).value);
        if (!order_id.has_value())
            co_return bad_request("URL parameter 'id' should be a valid integer");

        // Invoke the database logic
        result<orders::order_with_items> order = co_await input.repo().get_order_by_id(*order_id);
        if (order.has_error())
            co_return response_from_db_error(order.error());

        // Return the response
        co_return json_response(*order);
    }
}

asio::awaitable<http::response<http::string_body>> handle_create_order(const request_data& input)
{
    // Invoke the database logic
    orders::order_with_items order = co_await input.repo().create_order();

    // Return the response
    co_return json_response(order);
}

asio::awaitable<http::response<http::string_body>> handle_add_order_item(const request_data& input)
{
    // Check that the request has the appropriate content type
    auto it = input.request.find("Content-Type");
    if (it == input.request.end() || it->value() != "application/json")
        co_return bad_request("Invalid Content-Type: expected 'application/json'");

    // Parse the request body
    auto req = parse_json<orders::add_order_item_request>(input.request.body());
    if (req.has_error())
        co_return bad_request("Invalid JSON body");

    // Invoke the database logic
    result<orders::order_with_items> res = co_await input.repo()
                                               .add_order_item(req->order_id, req->product_id, req->quantity);
    if (res.has_error())
        co_return response_from_db_error(res.error());

    // Return the response
    co_return json_response(*res);
}

asio::awaitable<http::response<http::string_body>> handle_remove_order_item(const request_data& input)
{
    // Parse the query parameter
    auto params_it = input.target.params().find("id");
    if (params_it == input.target.params().end())
        co_return bad_request("Mandatory URL parameter 'id' not found");
    auto id = parse_id((*params_it).value);
    if (!id.has_value())
        co_return bad_request("URL parameter 'id' should be a valid integer");

    // Invoke the database logic
    result<orders::order_with_items> res = co_await input.repo().remove_order_item(*id);
    if (res.has_error())
        co_return response_from_db_error(res.error());

    // Return the response
    co_return json_response(*res);
}

asio::awaitable<http::response<http::string_body>> handle_checkout_order(const request_data& input)
{
    // Parse the query parameter
    auto params_it = input.target.params().find("id");
    if (params_it == input.target.params().end())
        co_return bad_request("Mandatory URL parameter 'id' not found");
    auto id = parse_id((*params_it).value);
    if (!id.has_value())
        co_return bad_request("URL parameter 'id' should be a valid integer");

    // Invoke the database logic
    result<orders::order_with_items> res = co_await input.repo().checkout_order(*id);
    if (res.has_error())
        co_return response_from_db_error(res.error());

    // Return the response
    co_return json_response(*res);
}

asio::awaitable<http::response<http::string_body>> handle_complete_order(const request_data& input)
{
    // Parse the query parameter
    auto params_it = input.target.params().find("id");
    if (params_it == input.target.params().end())
        co_return bad_request("Mandatory URL parameter 'id' not found");
    auto id = parse_id((*params_it).value);
    if (!id.has_value())
        co_return bad_request("URL parameter 'id' should be a valid integer");

    // Invoke the database logic
    result<orders::order_with_items> res = co_await input.repo().complete_order(*id);
    if (res.has_error())
        co_return response_from_db_error(res.error());

    // Return the response
    co_return json_response(*res);
}

struct http_endpoint
{
    http::verb method;
    asio::awaitable<http::response<http::string_body>> (*handler)(const request_data&);
};

const std::unordered_multimap<std::string_view, http_endpoint> endpoints{
    {"/products",        {http::verb::get, &handle_get_products}         },
    {"/orders",          {http::verb::get, &handle_get_orders}           },
    {"/orders",          {http::verb::post, &handle_create_order}        },
    {"/orders/items",    {http::verb::post, &handle_add_order_item}      },
    {"/orders/items",    {http::verb::delete_, &handle_remove_order_item}},
    {"/orders/checkout", {http::verb::post, &handle_checkout_order}      },
    {"/orders/complete", {http::verb::post, &handle_complete_order}      },
};

}  // namespace

// External interface
asio::awaitable<http::response<http::string_body>> orders::handle_request(
    const http::request<http::string_body>& request,
    mysql::connection_pool& pool
)
{
    // Parse the request target
    auto target = boost::urls::parse_origin_form(request.target());
    if (!target.has_value())
        co_return bad_request("Invalid request target");

    // Try to find an endpoint
    auto [it1, it2] = endpoints.equal_range(target->path());
    if (it1 == endpoints.end())
        co_return not_found("The request endpoint does not exist");

    // Match the verb
    auto it3 = std::find_if(it1, it2, [&request](const std::pair<std::string_view, http_endpoint>& ep) {
        return ep.second.method == request.method();
    });
    if (it3 == endpoints.end())
        co_return error_response(http::status::method_not_allowed, "Unsupported HTTP method");

    // Compose the data struct (TODO)
    request_data h{request, *target, pool};

    // Invoke the handler
    try
    {
        // Attempt to handle the request
        co_return co_await it3->second.handler(h);
    }
    catch (const mysql::error_with_diagnostics& err)
    {
        // A Boost.MySQL error. This will happen if you don't have connectivity
        // to your database, your schema is incorrect or your credentials are invalid.
        // Log the error, including diagnostics
        log_mysql_error(err.code(), err.get_diagnostics());

        // Never disclose error info to a potential attacker
        co_return internal_server_error();
    }
    catch (const std::exception& err)
    {
        // Another kind of error. This indicates a programming error or a severe
        // server condition (e.g. out of memory). Same procedure as above.
        {
            auto guard = orders::lock_cerr();
            std::cerr << "Uncaught exception: " << err.what() << std::endl;
        }
        co_return internal_server_error();
    }
}

//]

#endif

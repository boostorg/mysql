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

#include "error.hpp"
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
using boost::system::result;

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

    http::response<http::string_body> internal_server_error() const
    {
        return error_response(http::status::internal_server_error, {});
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

        // Serialize the body data into a string and use it as the response body.
        // We use Boost.JSON's automatic serialization feature, which uses Boost.Describe
        // reflection data to generate a serialization function for us.
        res.body() = boost::json::serialize(boost::json::value_from(body));

        // Done
        return res;
    }

    // Attempts to parse the request body as a JSON into an object of type T.
    // T should be a type with Boost.Describe metadata.
    // We use boost::system::result, which may contain a result or an error.
    template <class T>
    result<T> parse_json_request() const
    {
        // Check that the request has the appropriate content type
        auto it = request_.find("Content-Type");
        if (it == request_.end() || it->value() != "application/json")
        {
            return orders::errc::content_type_not_json;
        }

        // Attempt to parse the request into a json::value.
        // This will fail if the provided body isn't valid JSON.
        boost::system::error_code ec;
        auto val = boost::json::parse(request_.body(), ec);
        if (ec)
            return ec;

        // Attempt to parse the json::value into a T. This will
        // fail if the provided JSON doesn't match T's shape.
        return boost::json::try_value_to<T>(val);
    }

    http::response<http::string_body> response_from_db_error(boost::system::error_code ec) const
    {
        if (ec.category() == orders::get_orders_category())
        {
            switch (static_cast<orders::errc>(ec.value()))
            {
            case orders::errc::not_found: return not_found("The referenced entity does not exist");
            case orders::errc::product_not_found: return bad_request("The referenced product does not exist");
            case orders::errc::order_not_editable: return bad_request("The referenced order can't be edited");
            case orders::errc::order_not_pending_payment:
                return bad_request("The referenced order should be pending payment, but is not");
            default: return internal_server_error();
            }
        }
        else
        {
            return internal_server_error();
        }
    }

    http::response<http::string_body> response_from_json_error(boost::system::error_code ec) const
    {
        if (ec == orders::errc::content_type_not_json)
        {
            return bad_request("Invalid Content-Type: expected 'application/json'");
        }
        else
        {
            return bad_request("Invalid JSON");
        }
    }

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
            co_return handler.bad_request("URL parameter 'id' should be a valid integer");

        // Invoke the database logic
        result<orders::order_with_items> order = co_await handler.repo_.get_order_by_id(*order_id);
        if (order.has_error())
            co_return handler.response_from_db_error(order.error());

        // Return the response
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

asio::awaitable<http::response<http::string_body>> handle_add_order_item(request_handler& handler)
{
    // Parse the request body
    auto req = handler.parse_json_request<orders::add_order_item_request>();
    if (req.has_error())
        co_return handler.response_from_json_error(req.error());

    // Invoke the database logic
    result<orders::order_with_items> res = co_await handler.repo_
                                               .add_order_item(req->order_id, req->product_id, req->quantity);
    if (res.has_error())
        co_return handler.response_from_db_error(res.error());

    // Return the response
    co_return handler.json_response(*res);
}

// TODO: reduce duplication
asio::awaitable<http::response<http::string_body>> handle_remove_order_item(request_handler& handler)
{
    // Parse the query parameter
    auto params_it = handler.target.params().find("id");
    if (params_it == handler.target.params().end())
        co_return handler.bad_request("Mandatory URL parameter 'id' not found");
    auto id = parse_id((*params_it).value);
    if (!id.has_value())
        co_return handler.bad_request("URL parameter 'id' should be a valid integer");

    // Invoke the database logic
    result<orders::order_with_items> res = co_await handler.repo_.remove_order_item(*id);
    if (res.has_error())
        co_return handler.response_from_db_error(res.error());

    // Return the response
    co_return handler.json_response(*res);
}

asio::awaitable<http::response<http::string_body>> handle_checkout_order(request_handler& handler)
{
    // Parse the query parameter
    auto params_it = handler.target.params().find("id");
    if (params_it == handler.target.params().end())
        co_return handler.bad_request("Mandatory URL parameter 'id' not found");
    auto id = parse_id((*params_it).value);
    if (!id.has_value())
        co_return handler.bad_request("URL parameter 'id' should be a valid integer");

    // Invoke the database logic
    result<orders::order_with_items> res = co_await handler.repo_.checkout_order(*id);
    if (res.has_error())
        co_return handler.response_from_db_error(res.error());

    // Return the response
    co_return handler.json_response(*res);
}

asio::awaitable<http::response<http::string_body>> handle_complete_order(request_handler& handler)
{
    // Parse the query parameter
    auto params_it = handler.target.params().find("id");
    if (params_it == handler.target.params().end())
        co_return handler.bad_request("Mandatory URL parameter 'id' not found");
    auto id = parse_id((*params_it).value);
    if (!id.has_value())
        co_return handler.bad_request("URL parameter 'id' should be a valid integer");

    // Invoke the database logic
    result<orders::order_with_items> res = co_await handler.repo_.complete_order(*id);
    if (res.has_error())
        co_return handler.response_from_db_error(res.error());

    // Return the response
    co_return handler.json_response(*res);
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

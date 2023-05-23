//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//------------------------------------------------------------------------------
//
// Example: HTTP server, coroutine
//
//------------------------------------------------------------------------------

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/version.hpp>
#include <boost/config.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <boost/lexical_cast/try_lexical_convert.hpp>
#include <boost/optional/optional.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/url/parse.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <tuple>
#include <vector>

#include "dba.hpp"
#include "json_body.hpp"

namespace beast = boost::beast;  // from <boost/beast.hpp>
namespace http = beast::http;    // from <boost/beast/http.hpp>
namespace net = boost::asio;     // from <boost/asio.hpp>
namespace urls = boost::urls;
namespace json = boost::json;
using boost::core::string_view;
using boost::system::error_code;
using tcp = boost::asio::ip::tcp;  // from <boost/asio/ip/tcp.hpp>
using string_request = http::request<http::string_body>;
using string_response = http::response<http::string_body>;

string_response bad_request(const string_request& req, string_view why)
{
    string_response res{http::status::bad_request, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = std::string(why);
    res.prepare_payload();
    return res;
};

string_response not_found(const string_request& req)
{
    http::response<http::string_body> res{http::status::not_found, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "The resource '" + std::string(req.target()) + "' was not found.";
    res.prepare_payload();
    return res;
};

string_response internal_server_error(const string_request& req)
{
    http::response<http::string_body> res{http::status::internal_server_error, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "Internal server error\n";
    res.prepare_payload();
    return res;
};

string_response internal_server_error(const string_request& req, string_view what, error_code ec)
{
    std::cerr << "Internal server error for " << req.method() << " " << req.target() << ": " << what
              << ", error_code: " << ec << std::endl;
    return internal_server_error(req);
}

http::response<json_body> json_response(
    const string_request& req,
    json::value&& obj,
    http::status code = http::status::ok
)
{
    http::response<json_body> res{code, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "application/json");
    res.keep_alive(req.keep_alive());
    res.body() = std::move(obj);
    res.prepare_payload();
    return res;
}

http::response<http::empty_body> empty_response(const string_request& req)
{
    http::response<http::empty_body> res{http::status::no_content, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.keep_alive(req.keep_alive());
    res.prepare_payload();
    return res;
}

string_response error_code_response(const string_request& req, error_code ec)
{
    if (ec == dba::make_error_code(dba::errc::not_found))
        return not_found(req);
    else if (ec == dba::make_error_code(dba::errc::referenced_entity_not_found))
        return bad_request(req, "A referenced entity was not found");
    else if (ec == dba::make_error_code(dba::errc::order_wrong_status))
        return bad_request(req, "The given order is in an incorrect status");
    else
        return internal_server_error(req, "DB error", ec);
}

// Report a failure
void fail(beast::error_code ec, char const* what) { std::cerr << what << ": " << ec.message() << "\n"; }

boost::optional<std::int64_t> parse_id(string_view from)
{
    std::int64_t res;
    bool ok = boost::conversion::try_lexical_convert(from.data(), from.size(), res);
    if (!ok)
        return {};
    return res;
}

class server
{
    tcp::endpoint http_ep_;
    net::io_context ioc_;

public:
    server(tcp::endpoint http_ep) : http_ep_(http_ep) {}

    http::message_generator handle_get_products(
        net::yield_context yield,
        const string_request& req,
        string_view search
    )
    {
        // DBA
        auto dbares = dba::get_products(search, yield);
        if (dbares.err)
            return error_code_response(req, dbares.err);

        // Generate a JSON response. TODO: can we make this use a faster memory resource?
        json::array arr;
        for (const auto& prod : dbares.products)
        {
            arr.push_back(json::object({
                {"id",         prod.id        },
                {"short_name", prod.short_name},
                {"descr",      prod.descr     },
                {"price",      prod.price     }
            }));
        }
        json::value res;
        res.emplace_object()["products"] = std::move(arr);

        return json_response(req, std::move(res));
    }

    http::message_generator handle_create_order(net::yield_context yield, const string_request& req)
    {
        // DBA
        auto dbares = dba::create_order(yield);
        if (dbares.err)
            return error_code_response(req, dbares.err);

        // Generate a JSON response
        json::value res;
        res.emplace_object()["order"] = json::object({
            {"id",     dbares.id},
            {"status", "draft"  }
        });
        return json_response(req, std::move(res), http::status::created);
    }

    http::message_generator handle_get_orders(net::yield_context yield, const string_request& req)
    {
        // DBA
        auto dbares = dba::get_orders(yield);
        if (dbares.err)
            return error_code_response(req, dbares.err);

        // Generate a JSON response
        json::array arr;
        arr.reserve(dbares.orders.size());
        for (const auto& order : dbares.orders)
        {
            arr.push_back(json::object{
                {"id",     order.id    },
                {"status", order.status}
            });
        }
        json::value res;
        res.emplace_object()["orders"] = std::move(arr);
        return json_response(req, std::move(res));
    }

    http::message_generator handle_get_order(
        net::yield_context yield,
        const string_request& req,
        std::int64_t order_id
    )
    {
        // DBA
        auto dbares = dba::get_order(order_id, yield);
        if (dbares.err)
            return error_code_response(req, dbares.err);

        // Generate a JSON response
        json::array line_items;
        line_items.reserve(dbares.line_items.size());
        for (const auto& item : dbares.line_items)
        {
            line_items.push_back(json::object{
                {"id",       item.id      },
                {"quantity", item.quantity},
                {"price",    item.price   },
            });
        }
        json::value res{
            {"order", {}}
        };
        auto& order = res.get_object()["order"].get_object();
        order["id"] = order_id;
        order["status"] = dbares.status;
        order["line_items"] = std::move(line_items);
        return json_response(req, std::move(res));
    }

    http::message_generator handle_add_line_item(
        net::yield_context yield,
        const string_request& req,
        std::int64_t order_id
    )
    {
        error_code ec;

        // Parse the request body. TODO: we could do better and use a json_body
        auto it = req.find("content-type");
        if (it == req.end())
            return bad_request(req, "Missing Content-Type header");
        if (it->value() != "application/json")
            return bad_request(req, "Incorrect Content-type: should be application/json");
        auto body = json::parse(req.body(), ec);
        if (ec)
            return bad_request(req, "Invalid json");
        const auto* obj = body.if_object();
        if (!obj)
            return bad_request(req, "JSON root should be an object");
        auto objit = obj->find("product_id");
        if (objit == obj->end())
            return bad_request(req, "Missing mandatory propery product_id");
        const auto* product_id = objit->value().if_int64();
        if (!product_id)
            return bad_request(req, "product_id should be an int64");
        objit = obj->find("quantity");
        if (objit == obj->end())
            return bad_request(req, "Missing mandatory property quantity");
        const auto* quantity = objit->value().if_int64();
        if (!quantity)
            return bad_request(req, "quantity should be an int64");
        if (*quantity <= 0)
            return bad_request(req, "quantity should be a positive number");

        // DBA
        auto dbares = dba::add_line_item(order_id, *product_id, *quantity, yield);
        if (dbares.err)
            return error_code_response(req, dbares.err);

        // Return the newly created item
        json::value res;
        res.emplace_object()["line_item"] = json::object({
            {"id",         dbares.id  },
            {"product_id", *product_id},
            {"quantity",   *quantity  },
        });
        return json_response(req, std::move(res), http::status::created);
    }

    http::message_generator handle_remove_line_item(
        net::yield_context yield,
        const string_request& req,
        std::int64_t order_id,
        std::int64_t line_item_id
    )
    {
        // DBA
        auto dbares = dba::remove_line_item(order_id, line_item_id, yield);
        if (dbares.err)
            return error_code_response(req, dbares.err);

        // Return response
        return empty_response(req);
    }

    http::message_generator handle_checkout_order(
        net::yield_context yield,
        const string_request& req,
        std::int64_t order_id
    )
    {
        // DBA
        auto dbares = dba::checkout_order(order_id, yield);
        if (dbares.err)
            return error_code_response(req, dbares.err);

        // Return response
        return json_response(req, json::value(json::object()));
    }

    http::message_generator handle_complete_order(
        net::yield_context yield,
        const string_request& req,
        std::int64_t order_id
    )
    {
        // DBA
        auto dbares = dba::complete_order(order_id, yield);
        if (dbares.err)
            return error_code_response(req, dbares.err);

        // Return response
        return json_response(req, json::value(json::object()));
    }

    http::message_generator handle_request(net::yield_context yield, const string_request& req)
    {
        // Parse the URL to serve
        urls::result<urls::url_view> target_url = urls::parse_origin_form(req.target());
        if (!target_url)
            return bad_request(req, "Illegal request-target");
        auto params = target_url->params();
        auto segments = target_url->segments();

        // Match the URL with an endpoint
        // TODO: OPTIONS for preflights
        if (segments.empty())
            return not_found(req);
        auto segment_it = segments.begin();
        auto seg = *segment_it;

        if (seg == "products")
        {
            ++segment_it;

            // Path is /products
            if (segment_it == segments.end())
            {
                if (req.method() == http::verb::get)
                {
                    auto it = params.find("search");
                    std::string search = it == params.end() ? "" : (*it).value;
                    return handle_get_products(yield, req, search);
                }
                else
                    return bad_request(req, "Illegal method");
            }
        }
        else if (seg == "orders")
        {
            ++segment_it;

            // Path is /orders
            if (segment_it == segments.end())
            {
                if (req.method() == http::verb::get)
                    return handle_get_orders(yield, req);
                else if (req.method() == http::verb::post)
                    return handle_create_order(yield, req);
                else
                    return bad_request(req, "Illegal method");
            }

            auto order_id = parse_id(*segment_it);
            if (!order_id)
                return bad_request(req, "order_id should be an int64");
            ++segment_it;

            // Path is /orders/<order-id>
            if (segment_it == segments.end())
            {
                if (req.method() == http::verb::get)
                    return handle_get_order(yield, req, *order_id);
                else
                    return bad_request(req, "Illegal method");
            }

            seg = *segment_it;
            ++segment_it;

            // Path is /orders/<order-id>/checkout
            if (seg == "checkout" && segment_it == segments.end())
            {
                if (req.method() == http::verb::post)
                    return handle_checkout_order(yield, req, *order_id);
                else
                    return bad_request(req, "Illegal method");
            }

            // Path is /orders/<order-id>/complete
            if (seg == "complete" && segment_it == segments.end())
            {
                if (req.method() == http::verb::post)
                    return handle_complete_order(yield, req, *order_id);
                else
                    return bad_request(req, "Illegal method");
            }

            if (seg == "line-items")
            {
                // Path is /orders/<order-id>/line-items
                if (segment_it == segments.end())
                {
                    if (req.method() == http::verb::post)
                        return handle_add_line_item(yield, req, *order_id);
                    else
                        return bad_request(req, "Illegal method");
                }

                auto line_item_id = parse_id(*segment_it);
                if (!line_item_id)
                    return bad_request(req, "line_item_id should be an int64");
                ++segment_it;

                // Path is /orders/<order-id>/line-items/<line-item-id>
                if (segment_it == segments.end())
                {
                    if (req.method() == http::verb::delete_)
                        return handle_remove_line_item(yield, req, *order_id, *line_item_id);
                    else
                        return bad_request(req, "Illegal method");
                }
            }
        }

        // No URL matched, return a 404
        return not_found(req);
    }

    // Accepts incoming connections and launches the sessions
    void listen(net::yield_context yield)
    {
        beast::error_code ec;

        // Open the acceptor
        tcp::acceptor acceptor(ioc_);
        // tcp::acceptor acceptor(net::make_strand(ioc_));
        acceptor.open(http_ep_.protocol(), ec);
        if (ec)
            return fail(ec, "open");

        // Allow address reuse
        acceptor.set_option(net::socket_base::reuse_address(true), ec);
        if (ec)
            return fail(ec, "set_option");

        // Bind to the server address
        acceptor.bind(http_ep_, ec);
        if (ec)
            return fail(ec, "bind");

        // Start listening for connections
        acceptor.listen(net::socket_base::max_listen_connections, ec);
        if (ec)
            return fail(ec, "listen");

        for (;;)
        {
            tcp::socket socket(ioc_);
            // tcp::socket socket(net::make_strand(ioc_));
            acceptor.async_accept(socket, yield[ec]);
            if (ec)
                fail(ec, "accept");
            else
                boost::asio::spawn(
                    acceptor.get_executor(),
                    std::bind(
                        &server::do_session,
                        this,
                        beast::tcp_stream(std::move(socket)),
                        std::placeholders::_1
                    ),
                    // we ignore the result of the session,
                    // most errors are handled with error_code
                    boost::asio::detached
                );
        }
    }

    // Handles an HTTP server connection
    void do_session(beast::tcp_stream& stream, net::yield_context yield)
    {
        error_code ec;

        // This buffer is required to persist across reads
        beast::flat_buffer buffer;

        // This lambda is used to send messages
        for (;;)
        {
            // Set the timeout.
            stream.expires_after(std::chrono::seconds(30));

            // Read a request
            http::request<http::string_body> req;
            http::async_read(stream, buffer, req, yield[ec]);
            if (ec == http::error::end_of_stream)
                break;
            if (ec)
                return fail(ec, "read");

            // Handle the request
            http::message_generator msg = handle_request(yield, req);

            // Determine if we should close the connection
            bool keep_alive = msg.keep_alive();

            // Send the response
            beast::async_write(stream, std::move(msg), yield[ec]);

            if (ec)
                return fail(ec, "write");

            if (!keep_alive)
            {
                // This means we should close the connection, usually because
                // the response indicated the "Connection: close" semantic.
                break;
            }
        }

        // Send a TCP shutdown
        stream.socket().shutdown(tcp::socket::shutdown_send, ec);

        // At this point the connection is closed gracefully
    }

    void run()
    {
        // Spawn the listener coroutine
        boost::asio::spawn(
            ioc_,
            [this](net::yield_context yield) { listen(yield); },
            // on completion, spawn will call this function
            [](std::exception_ptr ex) {
                // if an exception occurred in the coroutine,
                // it's something critical, e.g. out of memory
                // we capture normal errors in the ec
                // so we just rethrow the exception here,
                // which will cause `ioc.run()` to throw
                if (ex)
                    std::rethrow_exception(ex);
            }
        );

        ioc_.run();
    }
};

int main(int argc, char* argv[])
{
    // Check command line arguments.
    if (argc != 4)
    {
        std::cerr << "Usage: http-server-coro <address> <port>\n"
                  << "Example:\n"
                  << "    http-server-coro 0.0.0.0 8080\n";
        return EXIT_FAILURE;
    }
    auto const address = net::ip::make_address(argv[1]);
    auto const port = static_cast<unsigned short>(std::atoi(argv[2]));

    server serv(tcp::endpoint(address, port));
    serv.run();

    return EXIT_SUCCESS;
}

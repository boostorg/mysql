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

#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_categories.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/tcp_ssl.hpp>
#include <boost/mysql/throw_on_error.hpp>

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
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <boost/lexical_cast/try_lexical_convert.hpp>
#include <boost/optional/optional.hpp>
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

#include "json_body.hpp"

namespace beast = boost::beast;  // from <boost/beast.hpp>
namespace http = beast::http;    // from <boost/beast/http.hpp>
namespace net = boost::asio;     // from <boost/asio.hpp>
namespace mysql = boost::mysql;
namespace urls = boost::urls;
namespace json = boost::json;
using tcp = boost::asio::ip::tcp;  // from <boost/asio/ip/tcp.hpp>
using boost::mysql::error_code;
using boost::mysql::string_view;
using string_request = http::request<http::string_body>;
using string_response = http::response<http::string_body>;

class connection_pool
{
    net::any_io_executor exec_;
    net::ssl::context ssl_ctx{net::ssl::context::tls_client};

public:
    connection_pool(net::any_io_executor e) : exec_(std::move(e)) {}
    mysql::tcp_ssl_connection async_get_connection(boost::mysql::diagnostics& diag, net::yield_context yield)
    {
        mysql::tcp_ssl_connection conn(exec_, ssl_ctx);
        conn.async_connect(
            net::ip::tcp::endpoint(net::ip::address_v4::loopback(), 3306),
            mysql::handshake_params("orders_user", "orders_password", "boost_mysql_order_management"),
            diag,
            yield
        );
        return conn;
    }
    // TODO: closing connections
};

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

string_response internal_server_error(
    const string_request& req,
    string_view what,
    error_code ec,
    const mysql::diagnostics& diag
)
{
    std::cerr << "Internal server error for " << req.method() << " " << req.target() << ": " << what
              << ", error_code: " << ec << ", diagnostics: " << diag.server_message() << std::endl;
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
    int num_threads_;
    tcp::endpoint http_ep_;
    net::io_context ioc_;
    connection_pool db_pool_;
    std::vector<std::thread> threads_;

public:
    server(int threads, tcp::endpoint http_ep)
        : num_threads_(threads), http_ep_(http_ep), ioc_(threads), db_pool_(ioc_.get_executor())
    {
    }

    http::message_generator handle_get_products(
        net::yield_context yield,
        const string_request& req,
        string_view search
    )
    {
        mysql::diagnostics diag;
        error_code ec;

        // Get a connection to MySQL
        auto conn = db_pool_.async_get_connection(diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "obtaining MySQL connection from pool", ec, diag);

        // Issue the query
        mysql::results result;
        if (!search.empty())
        {
            auto stmt = conn.async_prepare_statement(
                "SELECT id, short_name, descr, price "
                "FROM products "
                "WHERE MATCH(short_name, descr) AGAINST(p_search) "
                "LIMIT 5",
                diag,
                yield[ec]
            );
            if (ec)
                return internal_server_error(req, "preparing statement", ec, diag);

            conn.async_execute(stmt.bind(search), result, diag, yield[ec]);
            if (ec)
                return internal_server_error(req, "executing statement", ec, diag);
        }
        else
        {
            conn.async_execute(
                "SELECT id, short_name, descr, price "
                "FROM products "
                "LIMIT 5",
                result,
                diag,
                yield[ec]
            );
            if (ec)
                return internal_server_error(req, "executing query", ec, diag);
        }

        // Generate a JSON response. TODO: can we make this use a faster memory resource?
        json::array arr;
        for (auto rv : result.rows())
        {
            arr.push_back(json::object({
                {"id",         rv.at(0).as_int64() },
                {"short_name", rv.at(1).as_string()},
                {"descr",      rv.at(2).as_string()},
                {"price",      rv.at(3).as_int64() }
            }));
        }
        json::value res;
        res.emplace_object()["products"] = std::move(arr);

        return json_response(req, std::move(res));
    }

    http::message_generator handle_create_order(net::yield_context yield, const string_request& req)
    {
        mysql::diagnostics diag;
        error_code ec;

        // Get a connection to MySQL
        auto conn = db_pool_.async_get_connection(diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "obtaining MySQL connection from pool", ec, diag);

        // Issue the query
        mysql::results result;
        conn.async_execute("INSERT INTO orders VALUES ()", result, diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "executing query", ec, diag);

        // Generate a JSON response
        json::value res;
        res.emplace_object()["order"] = json::object({
            {"id",     result.last_insert_id()},
            {"status", "draft"                }
        });
        return json_response(req, std::move(res), http::status::created);
    }

    http::message_generator handle_get_orders(net::yield_context yield, const string_request& req)
    {
        mysql::diagnostics diag;
        error_code ec;

        // Get a connection to MySQL
        auto conn = db_pool_.async_get_connection(diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "obtaining MySQL connection from pool", ec, diag);

        // Issue the query
        mysql::results result;
        conn.async_execute("SELECT id, `status` FROM orders", result, diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "executing query", ec, diag);

        // Generate a JSON response
        json::array arr;
        arr.reserve(result.rows().size());
        for (auto rv : result.rows())
        {
            arr.push_back(json::object{
                {"id",     rv.at(0).as_int64() },
                {"status", rv.at(1).as_string()}
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
        mysql::diagnostics diag;
        error_code ec;

        // Get a connection to MySQL
        auto conn = db_pool_.async_get_connection(diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "obtaining MySQL connection from pool", ec, diag);

        // Prepare the statement
        auto stmt = conn.async_prepare_statement(
            "SELECT "
            "  ord.status AS order_status, "
            "  item.id AS item_id, "
            "  item.quantity AS item_quantity, "
            "  prod.price AS item_price "
            "FROM orders ord "
            "LEFT JOIN order_items item ON ord.id = item.order_id "
            "LEFT JOIN products prod ON item.product_id = prod.id "
            "WHERE ord.id = ?",
            diag,
            yield[ec]
        );
        if (ec)
            return internal_server_error(req, "preparing statement", ec, diag);

        // Execute it
        mysql::results result;
        conn.async_execute(stmt.bind(order_id), result, diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "executing statement", ec, diag);

        // If no orders were matched, issue a 404
        if (result.rows().empty())
            return not_found(req);

        // Generate a JSON response
        json::array line_items;
        if (!result.rows()[0][1].is_null())
        {
            // if item_id is NULL, the order exists but has no items
            line_items.reserve(result.rows().size());
            for (auto item : result.rows())
            {
                line_items.push_back(json::object{
                    {"id",       item.at(1).as_int64()},
                    {"quantity", item.at(2).as_int64()},
                    {"price",    item.at(3).as_int64()},
                });
            }
        }
        json::value res{
            {"order", {}}
        };
        auto& order = res.get_object()["order"].get_object();
        order["id"] = order_id;
        order["status"] = result.rows().front().at(0).as_string();
        order["line_items"] = std::move(line_items);
        return json_response(req, std::move(res));
    }

    http::message_generator handle_add_line_item(
        net::yield_context yield,
        const string_request& req,
        std::int64_t order_id
    )
    {
        mysql::diagnostics diag;
        error_code ec;
        mysql::results result;

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
            return bad_request(req, "Missing mandatory propery quantity");
        const auto* quantity = objit->value().if_int64();
        if (!quantity)
            return bad_request(req, "quantity should be an int64");
        if (*quantity <= 0)
            return bad_request(req, "quantity should be a positive number");

        // Get a connection to MySQL
        auto conn = db_pool_.async_get_connection(diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "obtaining MySQL connection from pool", ec, diag);

        // We need a transaction here
        conn.async_execute("START TRANSACTION", result, diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "starting transaction", ec, diag);

        // Get the order
        auto stmt = conn.async_prepare_statement("SELECT status FROM orders WHERE id = ?", diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "preparing select statement", ec, diag);
        conn.async_execute(stmt.bind(order_id), result, diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "executing select statement", ec, diag);

        // Check preconditions
        if (result.rows().empty())
            return not_found(req);
        if (result.rows()[0][0].as_string() != "draft")
            return bad_request(req, "Order is not in an editable state");

        // Insert the line item. This can fail with a foreign key constraint check if the product doesn't
        // exist
        stmt = conn.async_prepare_statement(
            "INSERT INTO order_items (order_id, product_id, quantity) values (?, ?, ?)",
            diag,
            yield[ec]
        );
        if (ec)
            return internal_server_error(req, "preparing update statement", ec, diag);
        conn.async_execute(stmt.bind(order_id, *product_id, *quantity), result, diag, yield[ec]);
        if (ec)
        {
            if (ec == mysql::common_server_errc::er_no_referenced_row_2 ||
                ec == mysql::common_server_errc::er_no_referenced_row)
            {
                return bad_request(req, "The given product does not exist");
            }
            else
            {
                return internal_server_error(req, "executing update statement", ec, diag);
            }
        }
        std::int64_t item_id = result.last_insert_id();

        // Commit
        conn.async_execute("COMMIT", result, diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "committing transaction", ec, diag);

        // Return the newly created item
        json::value res;
        res.emplace_object()["line_item"] = json::object({
            {"id",         item_id    },
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
        mysql::diagnostics diag;
        error_code ec;
        mysql::results result;

        // Get a connection to MySQL
        auto conn = db_pool_.async_get_connection(diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "obtaining MySQL connection from pool", ec, diag);

        // We need a transaction here
        conn.async_execute("START TRANSACTION", result, diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "starting transaction", ec, diag);

        // Get the order
        auto stmt = conn.async_prepare_statement("SELECT status FROM orders WHERE id = ?", diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "preparing select statement", ec, diag);
        conn.async_execute(stmt.bind(order_id), result, diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "executing select statement", ec, diag);

        // Check preconditions
        if (result.rows().empty())
            return not_found(req);
        if (result.rows()[0][0].as_string() != "draft")
            return bad_request(req, "Order is not in an editable state");

        // Delete the item
        stmt = conn.async_prepare_statement(
            "DELETE FROM order_items WHERE order_id = ? AND id = ?",
            diag,
            yield[ec]
        );
        if (ec)
            return internal_server_error(req, "preparing delete statement", ec, diag);
        conn.async_execute(stmt.bind(order_id, line_item_id), result, diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "executing delete statement", ec, diag);
        if (result.affected_rows() == 0u)
            return not_found(req);  // we could also return 204 here

        // Commit
        conn.async_execute("COMMIT", result, diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "committing transaction", ec, diag);

        return empty_response(req);
    }

    http::message_generator handle_checkout_order(
        net::yield_context yield,
        const string_request& req,
        std::int64_t order_id
    )
    {
        mysql::diagnostics diag;
        error_code ec;
        mysql::results result;

        // Get a connection to MySQL
        auto conn = db_pool_.async_get_connection(diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "obtaining MySQL connection from pool", ec, diag);

        // We need a transaction here
        conn.async_execute("START TRANSACTION", result, diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "starting transaction", ec, diag);

        // Get the order
        auto stmt = conn.async_prepare_statement("SELECT status FROM orders WHERE id = ?", diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "preparing select statement", ec, diag);
        conn.async_execute(stmt.bind(order_id), result, diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "executing select statement", ec, diag);

        // Check preconditions
        if (result.rows().empty())
            return not_found(req);
        if (result.rows()[0][0].as_string() != "draft")
            return bad_request(req, "Order is not in an editable state");

        // Update the order status. In the real world, we would generate a payment_intent or similar
        // through a payment gateway
        stmt = conn.async_prepare_statement(
            "UPDATE orders SET `status` = 'pending_payment' WHERE id = ?",
            diag,
            yield[ec]
        );
        if (ec)
            return internal_server_error(req, "preparing update statement", ec, diag);
        conn.async_execute(stmt.bind(order_id), result, diag, yield[ec]);

        // TODO: retrieve the order

        // Commit
        conn.async_execute("COMMIT", result, diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "committing transaction", ec, diag);

        return json_response(req, json::value(json::object()));
    }

    http::message_generator handle_complete_order(
        net::yield_context yield,
        const string_request& req,
        std::int64_t order_id
    )
    {
        mysql::diagnostics diag;
        error_code ec;
        mysql::results result;

        // Get a connection to MySQL
        auto conn = db_pool_.async_get_connection(diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "obtaining MySQL connection from pool", ec, diag);

        // We need a transaction here
        conn.async_execute("START TRANSACTION", result, diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "starting transaction", ec, diag);

        // Get the order
        auto stmt = conn.async_prepare_statement("SELECT status FROM orders WHERE id = ?", diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "preparing select statement", ec, diag);
        conn.async_execute(stmt.bind(order_id), result, diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "executing select statement", ec, diag);

        // Check preconditions
        if (result.rows().empty())
            return not_found(req);
        if (result.rows()[0][0].as_string() != "pending_payment")
            return bad_request(req, "Order should be in pending_payment state");

        // Update the order status
        stmt = conn.async_prepare_statement(
            "UPDATE orders SET `status` = 'complete' WHERE id = ?",
            diag,
            yield[ec]
        );
        if (ec)
            return internal_server_error(req, "preparing update statement", ec, diag);
        conn.async_execute(stmt.bind(order_id), result, diag, yield[ec]);

        // TODO: retrieve the order

        // Commit
        conn.async_execute("COMMIT", result, diag, yield[ec]);
        if (ec)
            return internal_server_error(req, "committing transaction", ec, diag);

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

        // Create the requested threads and make them run the io_context
        threads_.reserve(num_threads_ - 1);
        for (auto i = num_threads_ - 1; i > 0; --i)
            threads_.emplace_back([this] { ioc_.run(); });
        ioc_.run();
    }
};

int main(int argc, char* argv[])
{
    // Check command line arguments.
    if (argc != 4)
    {
        std::cerr << "Usage: http-server-coro <address> <port> <threads>\n"
                  << "Example:\n"
                  << "    http-server-coro 0.0.0.0 8080 1\n";
        return EXIT_FAILURE;
    }
    auto const address = net::ip::make_address(argv[1]);
    auto const port = static_cast<unsigned short>(std::atoi(argv[2]));
    auto const threads = std::max<int>(1, std::atoi(argv[3]));

    server serv(threads, tcp::endpoint(address, port));
    serv.run();

    return EXIT_SUCCESS;
}

//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/error_code.hpp>
#include <boost/mysql/tcp_ssl.hpp>

#include "dba.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/system/error_category.hpp>

using namespace boost::mysql;
namespace net = boost::asio;

namespace {

class dba_category final : public boost::system::error_category
{
public:
    const char* name() const noexcept final override { return "mysql.mysql-server"; }
    std::string message(int) const final override { return "not-implemented"; }
};

struct conn_wrapper
{
    net::ssl::context ssl_ctx{net::ssl::context::tls_client};
    tcp_ssl_connection conn;

    conn_wrapper(net::any_io_executor exec) : conn(std::move(exec), ssl_ctx) {}
};

conn_wrapper async_get_connection(diagnostics& diag, net::yield_context yield)
{
    conn_wrapper res(yield.get_executor());
    res.conn.async_connect(
        net::ip::tcp::endpoint(net::ip::address_v4::loopback(), 3306),
        handshake_params("orders_user", "orders_password", "boost_mysql_order_management"),
        diag,
        yield
    );
    return res;
}

}  // namespace

error_code dba::make_error_code(errc ec)
{
    static dba_category cat;
    return error_code(static_cast<int>(ec), cat);
}

dba::get_products_result dba::get_products(string_view search, yield_context yield)
{
    diagnostics diag;
    error_code ec;

    // Get a connection to MySQL
    auto connblock = async_get_connection(diag, yield[ec]);
    if (ec)
        return ec;
    auto& conn = connblock.conn;

    // Issue the query
    results result;
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
            return ec;

        conn.async_execute(stmt.bind(search), result, diag, yield[ec]);
        if (ec)
            return ec;
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
            return ec;
    }

    // Generate response
    get_products_result res;
    for (auto rv : result.rows())
    {
        res.products.push_back({
            rv.at(0).as_int64(),
            rv.at(1).as_string(),
            rv.at(2).as_string(),
            rv.at(3).as_int64(),
        });
    }
    return res;
}

dba::create_order_result dba::create_order(yield_context yield)
{
    diagnostics diag;
    error_code ec;

    // Get a connection to MySQL
    auto connblock = async_get_connection(diag, yield[ec]);
    if (ec)
        return ec;
    auto& conn = connblock.conn;

    // Issue the query
    results result;
    conn.async_execute("INSERT INTO orders VALUES ()", result, diag, yield[ec]);
    if (ec)
        return ec;

    // Return the response
    return create_order_result(static_cast<std::int64_t>(result.last_insert_id()));
}

dba::get_orders_result dba::get_orders(yield_context yield)
{
    diagnostics diag;
    error_code ec;

    // Get a connection to MySQL
    auto connblock = async_get_connection(diag, yield[ec]);
    if (ec)
        return ec;
    auto& conn = connblock.conn;

    // Issue the query
    results result;
    conn.async_execute("SELECT id, `status` FROM orders", result, diag, yield[ec]);
    if (ec)
        return ec;

    // Generate the response
    get_orders_result res;
    res.orders.reserve(result.rows().size());
    for (auto rv : result.rows())
    {
        res.orders.push_back({
            rv.at(0).as_int64(),  // id
            rv.at(1).as_string()  // status
        });
    }
    return res;
}

dba::get_order_result dba::get_order(std::int64_t order_id, yield_context yield)
{
    diagnostics diag;
    error_code ec;

    // Get a connection to MySQL
    auto connblock = async_get_connection(diag, yield[ec]);
    if (ec)
        return ec;
    auto& conn = connblock.conn;

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
        return ec;

    // Execute it
    results result;
    conn.async_execute(stmt.bind(order_id), result, diag, yield[ec]);
    if (ec)
        return ec;

    // If no orders were matched, issue a 404
    if (result.rows().empty())
        return make_error_code(dba::errc::not_found);

    // Generate a response
    get_order_result res(result.rows().front().at(0).as_string());
    if (!result.rows()[0][1].is_null())
    {
        // if item_id is NULL, the order exists but has no items
        res.line_items.reserve(result.rows().size());
        for (auto item : result.rows())
        {
            res.line_items.push_back({
                item.at(1).as_int64(),  // id
                item.at(2).as_int64(),  // quantity
                item.at(3).as_int64(),  // price
            });
        }
    }
    return res;
}

dba::add_line_item_result dba::add_line_item(
    std::int64_t order_id,
    std::int64_t product_id,
    std::int64_t quantity,
    yield_context yield
)
{
    diagnostics diag;
    error_code ec;
    results result;

    // Get a connection to MySQL
    auto connblock = async_get_connection(diag, yield[ec]);
    if (ec)
        return ec;
    auto& conn = connblock.conn;

    // We need a transaction here
    conn.async_execute("START TRANSACTION", result, diag, yield[ec]);
    if (ec)
        return ec;

    // Get the order
    auto stmt = conn.async_prepare_statement("SELECT status FROM orders WHERE id = ?", diag, yield[ec]);
    if (ec)
        return ec;
    conn.async_execute(stmt.bind(order_id), result, diag, yield[ec]);
    if (ec)
        return ec;

    // Check preconditions
    if (result.rows().empty())
        return make_error_code(dba::errc::not_found);
    if (result.rows()[0][0].as_string() != "draft")
        return make_error_code(dba::errc::order_wrong_status);

    // Insert the line item. This can fail with a foreign key constraint check if the product doesn't
    // exist
    stmt = conn.async_prepare_statement(
        "INSERT INTO order_items (order_id, product_id, quantity) values (?, ?, ?)",
        diag,
        yield[ec]
    );
    if (ec)
        return ec;
    conn.async_execute(stmt.bind(order_id, product_id, quantity), result, diag, yield[ec]);
    if (ec)
    {
        if (ec == common_server_errc::er_no_referenced_row_2 ||
            ec == common_server_errc::er_no_referenced_row)
        {
            return make_error_code(dba::errc::referenced_entity_not_found);
        }
        else
        {
            return ec;
        }
    }
    std::int64_t item_id = result.last_insert_id();

    // Commit
    conn.async_execute("COMMIT", result, diag, yield[ec]);
    if (ec)
        return ec;

    // Return the newly created item
    return add_line_item_result(item_id);
}

dba::remove_line_item_result dba::remove_line_item(
    std::int64_t order_id,
    std::int64_t line_item_id,
    yield_context yield
)
{
    diagnostics diag;
    error_code ec;
    results result;

    // Get a connection to MySQL
    auto connblock = async_get_connection(diag, yield[ec]);
    if (ec)
        return ec;
    auto& conn = connblock.conn;

    // We need a transaction here
    conn.async_execute("START TRANSACTION", result, diag, yield[ec]);
    if (ec)
        return ec;

    // Get the order
    auto stmt = conn.async_prepare_statement("SELECT status FROM orders WHERE id = ?", diag, yield[ec]);
    if (ec)
        return ec;
    conn.async_execute(stmt.bind(order_id), result, diag, yield[ec]);
    if (ec)
        return ec;

    // Check preconditions
    if (result.rows().empty())
        return make_error_code(dba::errc::not_found);
    if (result.rows()[0][0].as_string() != "draft")
        return make_error_code(dba::errc::order_wrong_status);

    // Delete the item
    stmt = conn.async_prepare_statement(
        "DELETE FROM order_items WHERE order_id = ? AND id = ?",
        diag,
        yield[ec]
    );
    if (ec)
        return ec;
    conn.async_execute(stmt.bind(order_id, line_item_id), result, diag, yield[ec]);
    if (ec)
        return ec;
    if (result.affected_rows() == 0u)
        return make_error_code(dba::errc::not_found);

    // Commit
    conn.async_execute("COMMIT", result, diag, yield[ec]);
    if (ec)
        return ec;

    return remove_line_item_result();
}

dba::checkout_order_result dba::checkout_order(std::int64_t order_id, yield_context yield)
{
    diagnostics diag;
    error_code ec;
    results result;

    // Get a connection to MySQL
    auto connblock = async_get_connection(diag, yield[ec]);
    if (ec)
        return ec;
    auto& conn = connblock.conn;

    // We need a transaction here
    conn.async_execute("START TRANSACTION", result, diag, yield[ec]);
    if (ec)
        return ec;

    // Get the order
    auto stmt = conn.async_prepare_statement("SELECT status FROM orders WHERE id = ?", diag, yield[ec]);
    if (ec)
        return ec;
    conn.async_execute(stmt.bind(order_id), result, diag, yield[ec]);
    if (ec)
        return ec;

    // Check preconditions
    if (result.rows().empty())
        return make_error_code(dba::errc::not_found);
    if (result.rows()[0][0].as_string() != "draft")
        return make_error_code(dba::errc::order_wrong_status);

    // Update the order status. In the real world, we would generate a payment_intent or similar
    // through a payment gateway
    stmt = conn.async_prepare_statement(
        "UPDATE orders SET `status` = 'pending_payment' WHERE id = ?",
        diag,
        yield[ec]
    );
    if (ec)
        return ec;
    conn.async_execute(stmt.bind(order_id), result, diag, yield[ec]);

    // TODO: retrieve the order

    // Commit
    conn.async_execute("COMMIT", result, diag, yield[ec]);
    if (ec)
        return ec;

    return checkout_order_result();
}

dba::complete_order_result dba::complete_order(std::int64_t order_id, yield_context yield)
{
    diagnostics diag;
    error_code ec;
    results result;

    // Get a connection to MySQL
    auto connblock = async_get_connection(diag, yield[ec]);
    if (ec)
        return ec;
    auto& conn = connblock.conn;

    // We need a transaction here
    conn.async_execute("START TRANSACTION", result, diag, yield[ec]);
    if (ec)
        return ec;

    // Get the order
    auto stmt = conn.async_prepare_statement("SELECT status FROM orders WHERE id = ?", diag, yield[ec]);
    if (ec)
        return ec;
    conn.async_execute(stmt.bind(order_id), result, diag, yield[ec]);
    if (ec)
        return ec;

    // Check preconditions
    if (result.rows().empty())
        return make_error_code(dba::errc::not_found);
    if (result.rows()[0][0].as_string() != "pending_payment")
        return make_error_code(dba::errc::order_wrong_status);

    // Update the order status
    stmt = conn.async_prepare_statement(
        "UPDATE orders SET `status` = 'complete' WHERE id = ?",
        diag,
        yield[ec]
    );
    if (ec)
        return ec;
    conn.async_execute(stmt.bind(order_id), result, diag, yield[ec]);

    // TODO: retrieve the order

    // Commit
    conn.async_execute("COMMIT", result, diag, yield[ec]);
    if (ec)
        return ec;

    return complete_order_result();
}
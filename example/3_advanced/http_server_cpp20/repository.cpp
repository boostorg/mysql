//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/awaitable.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)

//[example_connection_pool_repository_cpp
//
// File: repository.cpp
//

#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/static_results.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/with_diagnostics.hpp>
#include <boost/mysql/with_params.hpp>

#include <boost/asio/awaitable.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <tuple>

#include "repository.hpp"
#include "types.hpp"

namespace mysql = boost::mysql;
namespace asio = boost::asio;
using namespace orders;

/** Database tables:

CREATE TABLE products (
    id INT PRIMARY KEY AUTO_INCREMENT,
    short_name VARCHAR(100) NOT NULL,
    descr TEXT,
    price INT NOT NULL,
    FULLTEXT(short_name, descr)
);

CREATE TABLE orders(
    id INT PRIMARY KEY AUTO_INCREMENT,
    `status` ENUM('draft', 'pending_payment', 'complete') NOT NULL DEFAULT 'draft'
);

CREATE TABLE order_items(
    id INT PRIMARY KEY AUTO_INCREMENT,
    order_id INT NOT NULL,
    product_id INT NOT NULL,
    quantity INT NOT NULL,
    FOREIGN KEY (order_id) REFERENCES orders(id),
    FOREIGN KEY (product_id) REFERENCES products(id)
);

*/

asio::awaitable<std::vector<product>> db_repository::get_products(std::string_view search)
{
    // Get a connection from the pool
    auto conn = co_await pool_.async_get_connection();

    // Get the products using the MySQL built-in full-text search feature.
    // Look for the query string in the short_name and descr fields.
    // Parse the query results into product struct instances
    mysql::static_results<product> res;
    co_await conn->async_execute(
        mysql::with_params(
            "SELECT id, short_name, descr, price FROM products "
            "WHERE MATCH(short_name, descr) AGAINST({}) "
            "LIMIT 10",
            search
        ),
        res
    );

    // By default, connections are reset after they are returned to the pool
    // (by using any_connection::async_reset_connection). This will reset any
    // session state we changed while we were using the connection
    // (e.g. it will deallocate any statements we prepared).
    // We did nothing to mutate session state, so we can tell the pool to skip
    // this step, providing a minor performance gain.
    // We use pooled_connection::return_without_reset to do this.
    // If an exception was raised, the connection would be reset, for safety.
    conn.return_without_reset();

    // Return the result
    co_return std::vector<product>{res.rows().begin(), res.rows().end()};
}

asio::awaitable<std::vector<order>> db_repository::get_orders()
{
    // Get a connection from the pool
    auto conn = co_await pool_.async_get_connection();

    // Get all the orders.
    // Parse the result into order structs.
    mysql::static_results<order> res;
    co_await conn->async_execute("SELECT id, status FROM orders", res);

    // We didn't mutate session state, so we can skip resetting the connection
    conn.return_without_reset();

    // Return the result
    co_return std::vector<order>{res.rows().begin(), res.rows().end()};
}

asio::awaitable<std::optional<order_with_items>> db_repository::get_order_by_id(std::int64_t id)
{
    // Get a connection from the pool
    auto conn = co_await pool_.async_get_connection();

    // Get a single order and all its associated items.
    // The transaction ensures atomicity between the two SELECTs.
    // We issued 4 queries, so we get 4 resultsets back.
    // Ignore the 1st and 4th, and parse the other two into order and order_item structs
    mysql::static_results<std::tuple<>, order, order_item, std::tuple<>> result;
    co_await conn->async_execute(
        mysql::with_params(
            "START TRANSACTION READ ONLY;"
            "SELECT id, status FROM orders WHERE id = {0};"
            "SELECT id, product_id, quantity FROM order_items WHERE order_id = {0};"
            "COMMIT",
            id
        ),
        result
    );

    // We didn't mutate session state
    conn.return_without_reset();

    // result.rows<N> returns the rows for the N-th resultset, as a span
    auto orders = result.rows<1>();
    auto order_items = result.rows<2>();

    // Did we find the order we're looking for?
    if (orders.empty())
        co_return std::nullopt;
    const order& ord = orders[0];

    // If we did, compose the result
    co_return order_with_items{
        ord.id,
        ord.status,
        {order_items.begin(), order_items.end()}
    };
}

asio::awaitable<order_with_items> db_repository::create_order()
{
    // Get a connection from the pool
    auto conn = co_await pool_.async_get_connection();

    // Create the new order.
    // Orders are created empty, with all fields defaulted.
    // MySQL does not have an INSERT ... RETURNING statement, so we use
    // a transaction with an INSERT and a SELECT to create the order
    // and retrieve it atomically.
    // This yields 4 resultsets, one per SQL statement.
    // Ignore all except the SELECT, and parse it into an order struct.
    mysql::static_results<std::tuple<>, std::tuple<>, order, std::tuple<>> result;
    co_await conn->async_execute(
        "START TRANSACTION;"
        "INSERT INTO orders () VALUES ();"
        "SELECT id, status FROM orders WHERE id = LAST_INSERT_ID();"
        "COMMIT",
        result
    );

    // We didn't mutate session state
    conn.return_without_reset();

    // This must always yield one row. Return it.
    co_return result.rows<2>().front();
}

// TODO: we should probably use system::result to communicate what happened
asio::awaitable<std::optional<order_with_items>> db_repository::add_order_item(
    std::int64_t order_id,
    std::int64_t product_id,
    std::int64_t quantity
)
{
    // Get a connection from the pool
    auto conn = co_await pool_.async_get_connection();

    // Retrieve the order and the product.
    // SELECT ... FOR SHARE places a shared lock on the retrieved rows,
    // so they're not modified by other transactions while we use them.
    // For the product, we only need to check that it does exist,
    // so we get its ID and parse the returned rows into a std::tuple.
    mysql::static_results<std::tuple<>, order, std::tuple<std::int64_t>> result1;
    co_await conn->async_execute(
        mysql::with_params(
            "START TRANSACTION;"
            "SELECT id, status FROM orders WHERE id = {} FOR SHARE;"
            "SELECT id FROM products WHERE id = {} FOR SHARE",
            order_id,
            product_id
        ),
        result1
    );

    // Check that the order exists
    if (result1.rows<1>().empty())
    {
        // Not found. We did mutate session state by opening a transaction,
        // so we can't use return_without_reset
        co_return std::nullopt;
    }
    const order& ord = result1.rows<1>().front();

    // Verify that the order is editable.
    // Using SELECT ... FOR SHARE prevents race conditions with this check.
    if (ord.status != status_draft)
    {
        co_return std::nullopt;
    }

    // Check that the product exists
    if (result1.rows<2>().empty())
    {
        co_return std::nullopt;
    }

    // Insert the new item and retrieve all the items associated to this order
    mysql::static_results<std::tuple<>, order_item, std::tuple<>> result2;
    co_await conn->async_execute(
        mysql::with_params(
            "INSERT INTO order_items (order_id, product_id, quantity) VALUES ({0}, {1}, {2});"
            "SELECT id, product_id, quantity FROM order_items WHERE order_id = {0};"
            "COMMIT",
            order_id,
            product_id,
            quantity
        ),
        result2
    );

    // Compose the return value
    co_return order_with_items{
        ord.id,
        ord.status,
        {result2.rows<1>().begin(), result2.rows<1>().end()}
    };
}

asio::awaitable<std::optional<order_with_items>> db_repository::remove_order_item(std::int64_t item_id)
{
    // Get a connection from the pool
    auto conn = co_await pool_.async_get_connection();

    // Delete the item and retrieve the updated order.
    // The DELETE checks that the order exists and is editable.
    mysql::static_results<std::tuple<>, std::tuple<>, order, order_item, std::tuple<>> result;
    co_await conn->async_execute(
        mysql::with_params(
            "START TRANSACTION;"
            "DELETE it FROM order_items it"
            "  JOIN orders ord ON (it.order_id = ord.id)"
            "  WHERE it.id = {0} AND ord.status = 'draft';"
            "SELECT ord.id AS id, status FROM orders ord"
            "  JOIN order_items it ON (it.order_id = ord.id)"
            "  WHERE it.id = {0};"
            "SELECT id, product_id, quantity FROM order_items"
            "  WHERE order_id = (SELECT order_id FROM order_items WHERE id = {0});"
            "COMMIT",
            item_id
        ),
        result
    );

    // Check that the order exists
    if (result.rows<2>().empty())
    {
        // Not found. We did mutate session state by opening a transaction,
        // so we can't use return_without_reset
        co_return std::nullopt;
    }
    const order& ord = result.rows<2>().front();

    // Check that the item was deleted
    if (result.affected_rows<1>() == 0u)
    {
        // Nothing was deleted
        co_return std::nullopt;
    }

    // Compose the return value
    co_return order_with_items{
        ord.id,
        ord.status,
        {result.rows<3>().begin(), result.rows<3>().end()}
    };
}

// Helper function to implement checkout_order and complete_order
static asio::awaitable<std::optional<order_with_items>> change_order_status(
    mysql::connection_pool& pool,
    std::int64_t order_id,
    std::string_view original_status,  // The status that the order should have
    std::string_view target_status     // The status to transition the order to
)
{
    // Get a connection from the pool
    auto conn = co_await pool.async_get_connection();

    // Retrieve the order and lock it.
    // FOR UPDATE places an exclusive lock on the order,
    // preventing other concurrent transactions (including the ones
    // related to adding/removing items) from changing the order
    mysql::static_results<std::tuple<>, std::tuple<std::string>> result1;
    co_await conn->async_execute(
        mysql::with_params(
            "START TRANSACTION;"
            "SELECT status FROM orders WHERE id = {} FOR UPDATE;",
            order_id
        ),
        result1
    );

    // Check that the order exists
    if (result1.rows<1>().empty())
    {
        co_return std::nullopt;
    }

    // Check that the order is in the expected status
    if (std::get<0>(result1.rows<1>().front()) != original_status)
    {
        co_return std::nullopt;
    }

    // Update the order and retrieve the order details
    mysql::static_results<std::tuple<>, order_item, std::tuple<>> result2;
    co_await conn->async_execute(
        mysql::with_params(
            "UPDATE orders SET status = {1} WHERE id = {0};"
            "SELECT id, product_id, quantity FROM order_items WHERE order_id = {0};"
            "COMMIT",
            order_id,
            target_status
        ),
        result2
    );

    // Compose the return value
    co_return order_with_items{
        order_id,
        std::string(target_status),
        {result2.rows<1>().begin(), result2.rows<1>().end()}
    };
}

asio::awaitable<std::optional<order_with_items>> db_repository::checkout_order(std::int64_t id)
{
    return change_order_status(pool_, id, status_draft, status_pending_payment);
}

asio::awaitable<std::optional<order_with_items>> db_repository::complete_order(std::int64_t id)
{
    return change_order_status(pool_, id, status_pending_payment, status_complete);
}

//]

#endif

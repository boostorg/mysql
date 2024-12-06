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
#include <tuple>
#include <utility>

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
            "SELECT id, status FROM order WHERE id = {} FOR SHARE;"
            "SELECT id FROM product WHERE id = {} FOR SHARE",
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

asio::awaitable<std::optional<order_with_items>> db_repository::remove_order_item(std::int64_t item_id) {}

asio::awaitable<std::optional<order_with_items>> db_repository::checkout_order(std::int64_t id) {}

asio::awaitable<std::optional<order_with_items>> db_repository::complete_order(std::int64_t id) {}

std::vector<note_t> note_repository::get_notes(boost::asio::yield_context yield)
{
    // Get a fresh connection from the pool. This returns a pooled_connection object,
    // which is a proxy to an any_connection object. Connections are returned to the
    // pool when the proxy object is destroyed.
    // with_diagnostics ensures that thrown exceptions include diagnostic information
    mysql::pooled_connection conn = pool_.async_get_connection(with_diagnostics(yield));

    // Execute the query to retrieve all notes. We use the static interface to
    // parse results directly into static_results.
    mysql::static_results<note_t> result;
    conn->async_execute("SELECT id, title, content FROM notes", result, with_diagnostics(yield));

    // By default, connections are reset after they are returned to the pool
    // (by using any_connection::async_reset_connection). This will reset any
    // session state we changed while we were using the connection
    // (e.g. it will deallocate any statements we prepared).
    // We did nothing to mutate session state, so we can tell the pool to skip
    // this step, providing a minor performance gain.
    // We use pooled_connection::return_without_reset to do this.
    conn.return_without_reset();

    // Move note_t objects into the result vector to save allocations
    return std::vector<note_t>(
        std::make_move_iterator(result.rows().begin()),
        std::make_move_iterator(result.rows().end())
    );

    // If an exception is thrown, pooled_connection's destructor will
    // return the connection automatically to the pool.
}

optional<note_t> note_repository::get_note(std::int64_t note_id, boost::asio::yield_context yield)
{
    // Get a fresh connection from the pool. This returns a pooled_connection object,
    // which is a proxy to an any_connection object. Connections are returned to the
    // pool when the proxy object is destroyed.
    mysql::pooled_connection conn = pool_.async_get_connection(with_diagnostics(yield));

    // When executed, with_params expands a query client-side before sending it to the server.
    // Placeholders are marked with {}
    mysql::static_results<note_t> result;
    conn->async_execute(
        mysql::with_params("SELECT id, title, content FROM notes WHERE id = {}", note_id),
        result,
        with_diagnostics(yield)
    );

    // We did nothing to mutate session state, so we can skip reset
    conn.return_without_reset();

    // An empty results object indicates that no note was found
    if (result.rows().empty())
        return {};
    else
        return std::move(result.rows()[0]);
}

note_t note_repository::create_note(string_view title, string_view content, boost::asio::yield_context yield)
{
    // Get a fresh connection from the pool. This returns a pooled_connection object,
    // which is a proxy to an any_connection object. Connections are returned to the
    // pool when the proxy object is destroyed.
    mysql::pooled_connection conn = pool_.async_get_connection(with_diagnostics(yield));

    // We will use statements in this function for the sake of example.
    // We don't need to deallocate the statement explicitly,
    // since the pool takes care of it after the connection is returned.
    // You can also use with_params instead of statements.
    mysql::statement stmt = conn->async_prepare_statement(
        "INSERT INTO notes (title, content) VALUES (?, ?)",
        with_diagnostics(yield)
    );

    // Execute the statement. The statement won't produce any rows,
    // so we can use static_results<std::tuple<>>
    mysql::static_results<std::tuple<>> result;
    conn->async_execute(stmt.bind(title, content), result, with_diagnostics(yield));

    // MySQL reports last_insert_id as a uint64_t regardless of the actual ID type.
    // Given our table definition, this cast is safe
    auto new_id = static_cast<std::int64_t>(result.last_insert_id());

    return note_t{new_id, title, content};

    // There's no need to return the connection explicitly to the pool,
    // pooled_connection's destructor takes care of it.
}

optional<note_t> note_repository::replace_note(
    std::int64_t note_id,
    string_view title,
    string_view content,
    boost::asio::yield_context yield
)
{
    // Get a fresh connection from the pool. This returns a pooled_connection object,
    // which is a proxy to an any_connection object. Connections are returned to the
    // pool when the proxy object is destroyed.
    mysql::pooled_connection conn = pool_.async_get_connection(with_diagnostics(yield));

    // Expand and execute the query.
    // It won't produce any rows, so we can use static_results<std::tuple<>>
    mysql::static_results<std::tuple<>> empty_result;
    conn->async_execute(
        mysql::with_params(
            "UPDATE notes SET title = {}, content = {} WHERE id = {}",
            title,
            content,
            note_id
        ),
        empty_result,
        with_diagnostics(yield)
    );

    // We didn't mutate session state, so we can skip reset
    conn.return_without_reset();

    // No affected rows means that the note doesn't exist
    if (empty_result.affected_rows() == 0u)
        return {};

    return note_t{note_id, title, content};
}

bool note_repository::delete_note(std::int64_t note_id, boost::asio::yield_context yield)
{
    // Get a fresh connection from the pool. This returns a pooled_connection object,
    // which is a proxy to an any_connection object. Connections are returned to the
    // pool when the proxy object is destroyed.
    mysql::pooled_connection conn = pool_.async_get_connection(with_diagnostics(yield));

    // Expand and execute the query.
    // It won't produce any rows, so we can use static_results<std::tuple<>>
    mysql::static_results<std::tuple<>> empty_result;
    conn->async_execute(
        mysql::with_params("DELETE FROM notes WHERE id = {}", note_id),
        empty_result,
        with_diagnostics(yield)
    );

    // We didn't mutate session state, so we can skip reset
    conn.return_without_reset();

    // No affected rows means that the note didn't exist
    return empty_result.affected_rows() != 0u;
}

//]

#endif

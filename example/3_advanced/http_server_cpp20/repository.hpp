//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_EXAMPLE_3_ADVANCED_HTTP_SERVER_CPP20_REPOSITORY_HPP
#define BOOST_MYSQL_EXAMPLE_3_ADVANCED_HTTP_SERVER_CPP20_REPOSITORY_HPP

//[example_http_server_cpp20_repository_hpp
//
// File: repository.hpp
//

#include <boost/mysql/connection_pool.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/system/result.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace orders {

// Encapsulates database logic.
// If the database is unavailable, these functions throw.
// Additionally, functions that may fail depending on the supplied input
// return boost::system::result<T>, avoiding exceptions in common cases.
class db_repository
{
    boost::mysql::connection_pool& pool_;

public:
    // Constructor (this is a cheap-to-construct object)
    db_repository(boost::mysql::connection_pool& pool) noexcept : pool_(pool) {}

    // Retrieves products using a full-text search
    boost::asio::awaitable<std::vector<product>> get_products(std::string_view search);

    // Retrieves all the orders in the database
    boost::asio::awaitable<std::vector<order>> get_orders();

    // Retrieves an order by ID.
    // Returns an error if the ID doesn't match any order.
    boost::asio::awaitable<boost::system::result<order_with_items>> get_order_by_id(std::int64_t id);

    // Creates an empty order. Returns the created order.
    boost::asio::awaitable<order_with_items> create_order();

    // Adds an item to an order. Retrieves the updated order.
    // Returns an error if the ID doesn't match any order, the order
    // is not editable, or the product_id doesn't match any product
    boost::asio::awaitable<boost::system::result<order_with_items>> add_order_item(
        std::int64_t order_id,
        std::int64_t product_id,
        std::int64_t quantity
    );

    // Removes an item from an order. Retrieves the updated order.
    // Returns an error if the ID doesn't match any order item
    // or the order is not editable.
    boost::asio::awaitable<boost::system::result<order_with_items>> remove_order_item(std::int64_t item_id);

    // Checks an order out, transitioning it to the pending_payment status.
    // Returns an error if the ID doesn't match any order
    // or the order is not editable.
    boost::asio::awaitable<boost::system::result<order_with_items>> checkout_order(std::int64_t id);

    // Completes an order, transitioning it to the complete status.
    // Returns an error if the ID doesn't match any order
    // or the order is not checked out.
    boost::asio::awaitable<boost::system::result<order_with_items>> complete_order(std::int64_t id);
};

}  // namespace orders

//]

#endif

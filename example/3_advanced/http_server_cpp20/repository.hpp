//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_EXAMPLE_3_ADVANCED_CONNECTION_POOL_REPOSITORY_HPP
#define BOOST_MYSQL_EXAMPLE_3_ADVANCED_CONNECTION_POOL_REPOSITORY_HPP

//[example_connection_pool_repository_hpp
//
// File: repository.hpp
//

#include <boost/mysql/connection_pool.hpp>

#include <boost/asio/awaitable.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace orders {

// A lightweight wrapper around a connection_pool that allows
// creating, updating, retrieving and deleting notes in MySQL.
// This class encapsulates the database logic.
// All operations are async, and use stackful coroutines (boost::asio::yield_context).
// If the database can't be contacted, or unexpected database errors are found,
// an exception of type boost::mysql::error_with_diagnostics is thrown.
class db_repository
{
    boost::mysql::connection_pool& pool_;

public:
    // Constructor (this is a cheap-to-construct object)
    db_repository(boost::mysql::connection_pool& pool) noexcept : pool_(pool) {}

    // Retrieves all notes present in the database
    boost::asio::awaitable<std::vector<product>> get_products(std::string_view search);
    boost::asio::awaitable<std::vector<order>> get_orders();
    boost::asio::awaitable<std::optional<order_with_items>> get_order_by_id(std::int64_t id);
    boost::asio::awaitable<order_with_items> create_order();
    boost::asio::awaitable<std::optional<order_with_items>> add_order_item(
        std::int64_t order_id,
        std::int64_t product_id,
        std::int64_t quantity
    );
    boost::asio::awaitable<std::optional<order_with_items>> remove_order_item(std::int64_t item_id);
    boost::asio::awaitable<std::optional<order_with_items>> checkout_order(std::int64_t id);
    boost::asio::awaitable<std::optional<order_with_items>> complete_order(std::int64_t id);
};

}  // namespace orders

//]

#endif

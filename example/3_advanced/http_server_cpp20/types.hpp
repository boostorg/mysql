//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_EXAMPLE_3_ADVANCED_HTTP_SERVER_CPP20_TYPES_HPP
#define BOOST_MYSQL_EXAMPLE_3_ADVANCED_HTTP_SERVER_CPP20_TYPES_HPP

//[example_http_server_cpp20_types_hpp
//
// File: types.hpp
//
// Contains type definitions used in the REST API and database code.
// We use Boost.Describe (BOOST_DESCRIBE_STRUCT) to add reflection
// capabilities to our types. This allows using Boost.MySQL
// static interface (i.e. static_results<T>) to parse query results,
// and Boost.JSON automatic serialization/deserialization.

#include <boost/describe/class.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace orders {

// A product object, as defined in the database and in the GET /products endpoint
struct product
{
    // The unique database ID of the object.
    std::int64_t id;

    // The product's display name
    std::string short_name;

    // The product's description
    std::optional<std::string> descr;

    // The product's price, in dollar cents
    std::int64_t price;
};
BOOST_DESCRIBE_STRUCT(product, (), (id, short_name, descr, price))

// An order object, as defined in the database and in some REST endpoints.
// This object does not include the items associated to the order.
struct order
{
    // The unique database ID of the object.
    std::int64_t id;

    // The order status. One of "draft", "pending_payment" or "complete".
    std::string status;
};
BOOST_DESCRIBE_STRUCT(order, (), (id, status))

// Constants for the order::status member
inline constexpr std::string_view status_draft = "draft";
inline constexpr std::string_view status_pending_payment = "pending_payment";
inline constexpr std::string_view status_complete = "complete";

// An order item object, as defined in the database and in some REST endpoints.
// Does not include the order_id database field.
struct order_item
{
    // The unique database ID of the object.
    std::int64_t id;

    // The ID of the product that this order item represents
    std::int64_t product_id;

    // The number of units of the product that this item represents.
    // For instance, if product_id=2 and quantity=3,
    // the user wants to buy 3 units of the product with ID 2.
    std::int64_t quantity;
};
BOOST_DESCRIBE_STRUCT(order_item, (), (id, product_id, quantity))

// An order object, with its associated order items.
// Used in some REST endpoints.
struct order_with_items
{
    // The unique database ID of the object.
    std::int64_t id;

    // The order status. One of "draft", "pending_payment" or "complete".
    std::string status;

    // The items associated to this order.
    std::vector<order_item> items;
};
BOOST_DESCRIBE_STRUCT(order_with_items, (), (id, status, items))

// REST request for POST /orders/items
struct add_order_item_request
{
    // Identifies the order to which the item should be added.
    std::int64_t order_id;

    // Identifies the product that should be added to the order.
    std::int64_t product_id;

    // The number of units of the above product that should be added to the order.
    std::int64_t quantity;
};
BOOST_DESCRIBE_STRUCT(add_order_item_request, (), (order_id, product_id, quantity))

}  // namespace orders

//]

#endif

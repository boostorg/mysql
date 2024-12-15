//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_EXAMPLE_3_ADVANCED_HTTP_SERVER_CPP20_TYPES_HPP
#define BOOST_MYSQL_EXAMPLE_3_ADVANCED_HTTP_SERVER_CPP20_TYPES_HPP

//[example_http_cpp20_types_hpp
//
// File: types.hpp
//

#include <boost/describe/class.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// TODO: review
// Contains type definitions used in the REST API and database code.
// We use Boost.Describe (BOOST_DESCRIBE_STRUCT) to add reflection
// capabilities to our types. This allows using Boost.MySQL
// static interface (i.e. static_results<T>) to parse query results,
// and Boost.JSON automatic serialization/deserialization.

namespace orders {

struct product
{
    // The unique database ID of the object.
    std::int64_t id;

    // The product's display name
    std::string short_name;

    // The product's description
    std::string descr;

    // The product's price, in dollar cents
    std::int64_t price;
};
BOOST_DESCRIBE_STRUCT(product, (), (id, short_name, descr, price))

struct order
{
    std::int64_t id;
    std::string status;
};
BOOST_DESCRIBE_STRUCT(order, (), (id, status))

inline constexpr std::string_view status_draft = "draft";
inline constexpr std::string_view status_pending_payment = "pending_payment";
inline constexpr std::string_view status_complete = "complete";

struct order_item
{
    std::int64_t id;
    std::int64_t product_id;
    std::int64_t quantity;
};
BOOST_DESCRIBE_STRUCT(order_item, (), (id, product_id, quantity))

struct order_with_items
{
    std::int64_t id;
    std::string status;
    std::vector<order_item> items;
};
BOOST_DESCRIBE_STRUCT(order_with_items, (), (id, status, items))

//
// REST API requests.
//

//
// REST API responses.
//

}  // namespace orders

//]

#endif

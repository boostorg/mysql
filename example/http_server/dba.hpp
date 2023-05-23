//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_EXAMPLE_HTTP_SERVER_DBA_HPP
#define BOOST_MYSQL_EXAMPLE_HTTP_SERVER_DBA_HPP

#include <boost/asio/spawn.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/system/error_code.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace dba {

using boost::asio::yield_context;
using boost::core::string_view;
using boost::system::error_code;

enum class errc
{
    not_found,
    referenced_entity_not_found,
    order_wrong_status
};
error_code make_error_code(dba::errc ec);

// get products
struct get_products_result
{
    struct product
    {
        std::int64_t id;
        std::string short_name;
        std::string descr;
        std::int64_t price;
    };

    get_products_result(error_code ec) : err(ec) {}
    get_products_result() = default;

    error_code err;
    std::vector<product> products;
};
get_products_result get_products(string_view search, yield_context yield);

// create order
struct create_order_result
{
    create_order_result(error_code ec) : err(ec) {}
    explicit create_order_result(std::int64_t id) : id(id) {}

    error_code err;
    std::int64_t id;
};
create_order_result create_order(yield_context yield);

// get orders
struct get_orders_result
{
    struct order
    {
        std::int64_t id;
        std::string status;
    };

    get_orders_result(error_code ec) : err(ec) {}
    get_orders_result() = default;

    error_code err;
    std::vector<order> orders;
};
get_orders_result get_orders(yield_context yield);

// get order
struct get_order_result
{
    struct item
    {
        std::int64_t id;
        std::int64_t quantity;
        std::int64_t price;
    };

    get_order_result(error_code ec) : err(ec) {}
    explicit get_order_result(std::string status) : status(std::move(status)) {}

    error_code err;
    std::string status;
    std::vector<item> line_items;
};
get_order_result get_order(std::int64_t order_id, yield_context yield);

// add line item
struct add_line_item_result
{
    error_code err;
    std::int64_t id;

    add_line_item_result(error_code ec) : err(ec) {}
    explicit add_line_item_result(std::int64_t id) : id(id) {}
};
add_line_item_result add_line_item(
    std::int64_t order_id,
    std::int64_t product_id,
    std::int64_t quantity,
    yield_context yield
);

// remove line item
struct remove_line_item_result
{
    error_code err;

    remove_line_item_result(error_code ec) : err(ec) {}
    remove_line_item_result() = default;
};
remove_line_item_result remove_line_item(std::int64_t order_id, std::int64_t item_id, yield_context yield);

// checkout order
struct checkout_order_result
{
    error_code err;

    checkout_order_result(error_code ec) : err(ec) {}
    checkout_order_result() = default;
};
checkout_order_result checkout_order(std::int64_t order_id, yield_context yield);

// complete order
struct complete_order_result
{
    error_code err;

    complete_order_result(error_code ec) : err(ec) {}
    complete_order_result() = default;
};
complete_order_result complete_order(std::int64_t order_id, yield_context yield);

}  // namespace dba

#endif

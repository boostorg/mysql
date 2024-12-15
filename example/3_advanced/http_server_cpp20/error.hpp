//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_EXAMPLE_3_ADVANCED_HTTP_SERVER_CPP20_ERROR_HPP
#define BOOST_MYSQL_EXAMPLE_3_ADVANCED_HTTP_SERVER_CPP20_ERROR_HPP

//[example_http_server_cpp20_error_hpp
//
// File: error.hpp
//
// Contains an errc enumeration and the required pieces to
// use it with Boost.System error codes.
// We use this indirectly in the DB repository class,
// when using the error codes in boost::system::result.

#include <boost/system/error_category.hpp>

#include <mutex>
#include <string_view>
#include <type_traits>

namespace orders {

// Error code enum for errors originated within our application
enum class errc
{
    not_found,                  // couldn't retrieve or modify a certain resource because it doesn't exist
    order_not_editable,         // an operation requires an order to be editable, but it's not
    order_not_pending_payment,  // an operation requires an order to be pending payment, but it's not
    product_not_found,          // a product referenced by a request doesn't exist
};

// The error category for errc
const boost::system::error_category& get_orders_category();

// Allows constructing error_code from errc
inline boost::system::error_code make_error_code(errc v)
{
    return boost::system::error_code(static_cast<int>(v), get_orders_category());
}

// In multi-threaded programs, using std::cerr without any locking
// can result in interleaved output.
// Locks a mutex guarding std::cerr to prevent this.
// All uses of std::cerr should respect this.
std::unique_lock<std::mutex> lock_cerr();

// A helper function for the common case where we want to log an error code
void log_error(std::string_view header, boost::system::error_code ec);

}  // namespace orders

// Allows constructing error_code from errc
namespace boost {
namespace system {

template <>
struct is_error_code_enum<orders::errc> : std::true_type
{
};

}  // namespace system
}  // namespace boost

//]

#endif
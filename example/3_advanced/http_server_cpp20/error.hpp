//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
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
// use it with boost::system::error_code.
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
    not_found,             // couldn't retrieve or modify a certain resource because it doesn't exist
    order_invalid_status,  // an operation found an order in a status != the one expected (e.g. not editable)
    product_not_found,     // a product referenced by a request doesn't exist
};

// To use errc with boost::system::error_code, we need
// to define an error category (see the cpp file).
const boost::system::error_category& get_orders_category();

// Called when constructing an error_code from an errc value.
inline boost::system::error_code make_error_code(errc v)
{
    // Roughly, an error_code is an int and a category defining what the int means.
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

// This specialization is required to construct error_code's from errc values
template <>
struct boost::system::is_error_code_enum<orders::errc> : std::true_type
{
};

//]

#endif

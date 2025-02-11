//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/awaitable.hpp>
#include <boost/pfr/config.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT) && BOOST_PFR_CORE_NAME_ENABLED

//[example_http_server_cpp20_error_cpp

#include <boost/system/error_category.hpp>

#include <iostream>
#include <mutex>

#include "error.hpp"

namespace {

// Converts an orders::errc to string
const char* error_to_string(orders::errc value)
{
    switch (value)
    {
    case orders::errc::not_found: return "not_found";
    case orders::errc::order_invalid_status: return "order_invalid_status";
    case orders::errc::product_not_found: return "product_not_found";
    default: return "<unknown orders::errc>";
    }
}

// The category to be returned by get_orders_category
class orders_category final : public boost::system::error_category
{
public:
    // Identifies the error category. Used when converting error_codes to string
    const char* name() const noexcept final override { return "orders"; }

    // Given a numeric error belonging to this category, convert it to a string
    std::string message(int ev) const final override
    {
        return error_to_string(static_cast<orders::errc>(ev));
    }
};

// The error category
static const orders_category g_category;

// The std::mutex that guards std::cerr
static std::mutex g_cerr_mutex;

}  // namespace

//
// External interface
//
const boost::system::error_category& orders::get_orders_category() { return g_category; }

std::unique_lock<std::mutex> orders::lock_cerr() { return std::unique_lock{g_cerr_mutex}; }

void orders::log_error(std::string_view header, boost::system::error_code ec)
{
    // Lock the mutex
    auto guard = lock_cerr();

    // Logging the error code prints the number and category. Add the message, too
    std::cerr << header << ": " << ec << " " << ec.message() << std::endl;
}

//]

#endif

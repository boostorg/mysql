//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_http_server_cpp20_error_cpp

#include <boost/system/detail/error_category.hpp>

#include "error.hpp"

namespace {

// Converts an orders::errc to string
const char* error_to_string(orders::errc value)
{
    switch (value)
    {
    case orders::errc::not_found: return "not_found";
    case orders::errc::order_not_editable: return "order_not_editable";
    case orders::errc::order_not_pending_payment: return "order_not_pending_payment";
    case orders::errc::product_not_found: return "product_not_found";
    default: return "<unknown orders::errc>";
    }
}

// Custom category for orders::errc
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

static const orders_category cat;

}  // namespace

const boost::system::error_category& orders::get_orders_category() { return cat; }

//]
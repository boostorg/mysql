//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_SRC_PROTOCOL_BINARY_SERIALIZATION_HPP
#define BOOST_MYSQL_SRC_PROTOCOL_BINARY_SERIALIZATION_HPP

#include <boost/mysql/field_view.hpp>

#include "protocol/serialization.hpp"

namespace boost {
namespace mysql {
namespace detail {

std::size_t get_size(field_view input) noexcept;
void serialize(serialization_context& ctx, field_view input) noexcept;

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif

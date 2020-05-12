//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_BINARY_SERIALIZATION_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_BINARY_SERIALIZATION_HPP

#include "boost/mysql/value.hpp"
#include "boost/mysql/detail/protocol/serialization.hpp"

namespace boost {
namespace mysql {
namespace detail {

inline std::size_t get_binary_value_size(
    const serialization_context& ctx,
    const value& input
) noexcept;

inline void serialize_binary_value(
    serialization_context& ctx,
    const value& input
) noexcept;


} // detail
} // mysql
} // boost

#include "boost/mysql/detail/protocol/impl/binary_serialization.ipp"

#endif

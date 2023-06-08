//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_PROTOCOL_PRINTING_HPP
#define BOOST_MYSQL_TEST_UNIT_PROTOCOL_PRINTING_HPP

#include <ostream>

#include "protocol/basic_types.hpp"
#include "protocol/serialization.hpp"

namespace boost {
namespace mysql {
namespace detail {

// deserialize_errc
inline std::ostream& operator<<(std::ostream& os, deserialize_errc v)
{
    switch (v)
    {
    case deserialize_errc::ok: return os << "ok";
    case deserialize_errc::incomplete_message: return os << "incomplete_message";
    case deserialize_errc::protocol_value_error: return os << "protocol_value_error";
    case deserialize_errc::server_unsupported: return os << "server_unsupported";
    default: return os << "unknown";
    }
}

// int3
inline bool operator==(int3 lhs, int3 rhs) noexcept { return lhs.value == rhs.value; }
inline std::ostream& operator<<(std::ostream& os, int3 value) { return os << value.value; }

// int_lenenc
inline bool operator==(int_lenenc lhs, int_lenenc rhs) noexcept { return lhs.value == rhs.value; }
inline std::ostream& operator<<(std::ostream& os, int_lenenc value) { return os << value.value; }

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif

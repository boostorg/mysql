//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_SERIALIZATION_HELPERS_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_SERIALIZATION_HELPERS_HPP

#include <boost/mysql/impl/internal/protocol/impl/serialization_context.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>

#include <cstdint>
#include <vector>

namespace boost {
namespace mysql {
namespace test {

template <class Fn>
std::vector<std::uint8_t> serialize_to_vector(const Fn& serialize_fn)
{
    std::vector<std::uint8_t> buff;
    detail::serialization_context ctx(buff, detail::disable_framing);
    serialize_fn(ctx);
    return buff;
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif

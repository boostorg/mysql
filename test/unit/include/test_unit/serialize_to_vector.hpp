//
// Copyright (c) 2019-2026 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_SERIALIZE_TO_VECTOR_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_SERIALIZE_TO_VECTOR_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/mysql/impl/internal/protocol/impl/serialization_context.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>

#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <vector>

namespace boost {
namespace mysql {
namespace test {

template <class Fn>
std::vector<std::uint8_t> serialize_to_vector(const Fn& serialize_fn)
{
    std::vector<std::uint8_t> buff;
    detail::buffer_writer writer{buff};
    detail::serialization_context ctx(writer, static_cast<std::size_t>(-1), detail::disable_framing);
    serialize_fn(ctx);
    BOOST_TEST(ctx.error() == error_code());
    // Strip vector size to actual size
    buff.resize(writer.size());
    return buff;
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif

//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_QUERY_FRAME_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_QUERY_FRAME_HPP

#include <boost/mysql/string_view.hpp>

#include <boost/config.hpp>

#include <cstdint>
#include <vector>

#include "test_common/buffer_concat.hpp"
#include "test_unit/create_frame.hpp"

namespace boost {
namespace mysql {
namespace test {

// GCC raises a spurious warning here
#if BOOST_GCC >= 110000
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overread"
#endif
inline std::vector<std::uint8_t> create_query_body(string_view sql)
{
    return buffer_builder()
        .add({0x03})
        .add({reinterpret_cast<const std::uint8_t*>(sql.data()), sql.size()})
        .build();
}
#if BOOST_GCC >= 110000
#pragma GCC diagnostic pop
#endif

inline std::vector<std::uint8_t> create_query_frame(std::uint8_t seqnum, string_view sql)
{
    return create_frame(seqnum, create_query_body(sql));
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif

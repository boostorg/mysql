//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_TEST_FORMAT_SQL_FORMAT_COMMON_HPP
#define BOOST_MYSQL_TEST_UNIT_TEST_FORMAT_SQL_FORMAT_COMMON_HPP

#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/core/span.hpp>

#include <string>
#include <vector>

#include "test_unit/custom_allocator.hpp"

namespace boost {
namespace mysql {
namespace test {

using string_with_alloc = std::basic_string<char, std::char_traits<char>, custom_allocator<char>>;
using blob_with_alloc = std::vector<unsigned char, custom_allocator<unsigned char>>;

}  // namespace test
}  // namespace mysql
}  // namespace boost

// Field with a custom formatter
namespace custom {

struct condition
{
    boost::mysql::string_view name;
    int value;
};

}  // namespace custom

namespace boost {
namespace mysql {

template <>
struct formatter<custom::condition>
{
    static void format(const custom::condition& v, format_context_base& ctx)
    {
        ctx.append_value(identifier(v.name)).append_raw("=").append_value(v.value);
    }
};

}  // namespace mysql
}  // namespace boost

#endif

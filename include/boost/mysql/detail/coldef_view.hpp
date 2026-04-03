//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_COLDEF_VIEW_HPP
#define BOOST_MYSQL_DETAIL_COLDEF_VIEW_HPP

#include <boost/mysql/column_type.hpp>

#include <cstdint>
#include <string_view>

namespace boost {
namespace mysql {
namespace detail {

struct coldef_view
{
    std::string_view database;
    std::string_view table;
    std::string_view org_table;
    std::string_view name;
    std::string_view org_name;
    std::uint16_t collation_id;
    std::uint32_t column_length;  // maximum length of the field
    column_type type;
    std::uint16_t flags;
    std::uint8_t decimals;  // max shown decimal digits. 0x00 for int/static strings; 0x1f for
                            // dynamic strings, double, float
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif

//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_FIELD_KIND_HPP
#define BOOST_MYSQL_FIELD_KIND_HPP

#include <iosfwd>

namespace boost {
namespace mysql {

enum class field_kind
{
    // Order here is important
    null = 0,
    int64,
    uint64,
    string,
    float_,
    double_,
    date,
    datetime,
    time
};

inline std::ostream& operator<<(std::ostream& os, field_kind v);

} // mysql
} // boost

#include <boost/mysql/impl/field_kind.ipp>

#endif

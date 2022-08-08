//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_ROW_VIEW_HPP
#define BOOST_MYSQL_IMPL_ROW_VIEW_HPP

#pragma once

#include <boost/mysql/row_view.hpp>
#include <algorithm>
#include <ostream>

inline bool boost::mysql::operator==(
    const row_view& lhs,
    const row_view& rhs
) noexcept
{
    return lhs.size() == rhs.size() &&
        std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

inline std::ostream& boost::mysql::operator<<(
    std::ostream& os,
    const row_view& value
)
{
    os << '{';
    if (!value.empty())
    {
        os << value[0];
        for (auto it = std::next(value.begin()); it != value.end(); ++it)
        {
            os << ", " << *it;
        }
    }
    return os << '}';
}

#endif

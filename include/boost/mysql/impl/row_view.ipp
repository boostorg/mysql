//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_ROW_VIEW_IPP
#define BOOST_MYSQL_IMPL_ROW_VIEW_IPP

#pragma once

#include <boost/mysql/row_view.hpp>

#include <cstddef>
#include <ostream>
#include <stdexcept>

boost::mysql::field_view boost::mysql::row_view::at(std::size_t i) const
{
    if (i >= size_)
        throw std::out_of_range("row_view::at");
    return fields_[i];
}

inline bool boost::mysql::operator==(const row_view& lhs, const row_view& rhs) noexcept
{
    if (lhs.size() != rhs.size())
        return false;
    for (std::size_t i = 0; i < lhs.size(); ++i)
    {
        if (lhs[i] != rhs[i])
            return false;
    }
    return true;
}

inline std::ostream& boost::mysql::operator<<(std::ostream& os, const row_view& value)
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

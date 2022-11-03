//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_ROWS_VIEW_IPP
#define BOOST_MYSQL_IMPL_ROWS_VIEW_IPP

#pragma once

#include <boost/mysql/rows_view.hpp>

#include <cassert>
#include <stdexcept>

boost::mysql::row_view boost::mysql::rows_view::operator[](std::size_t i) const noexcept
{
    assert(i < size());
    return row_view(fields_ + num_columns_ * i, num_columns_);
}

boost::mysql::row_view boost::mysql::rows_view::at(std::size_t i) const
{
    if (i >= size())
    {
        throw std::out_of_range("rows_view::at");
    }
    return (*this)[i];
}

inline bool boost::mysql::rows_view::operator==(const rows_view& rhs) const noexcept
{
    if (num_fields_ != rhs.num_fields_ || num_columns_ != rhs.num_columns_)
        return false;
    for (std::size_t i = 0; i < num_fields_; ++i)
    {
        if (fields_[i] != rhs.fields_[i])
            return false;
    }
    return true;
}

#endif

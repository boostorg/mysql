//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_ROWS_IPP
#define BOOST_MYSQL_IMPL_ROWS_IPP

#pragma once

#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/rows.hpp>
#include <cstddef>
#include <stdexcept>

boost::mysql::rows::rows(
    const rows_view& view
) :
    detail::row_base(view.fields_, view.num_values_),
    num_columns_(view.num_columns_)
{
    copy_strings();
}

boost::mysql::rows& boost::mysql::rows::operator=(
    const rows_view& rhs
)
{
    assign(rhs.fields_, rhs.num_values_);
    num_columns_ = rhs.num_columns_;
    return *this;
}

boost::mysql::row_view boost::mysql::rows::at(
    std::size_t i
) const
{
    if (i >= size())
    {
        throw std::out_of_range("rows::at");
    }
    return (*this)[i];
}

boost::mysql::row_view boost::mysql::rows::operator[](
    std::size_t i
) const noexcept
{
    return row_view(fields_.data() + num_columns_ * i, num_columns_);
}

#endif

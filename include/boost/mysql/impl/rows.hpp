//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_ROWS_HPP
#define BOOST_MYSQL_IMPL_ROWS_HPP

#pragma once

#include <boost/mysql/rows.hpp>
#include <boost/mysql/rows_view.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>
#include <boost/mysql/detail/auxiliar/rows_iterator.hpp>

#include <boost/assert.hpp>
#include <boost/throw_exception.hpp>

#include <cstddef>
#include <stdexcept>

boost::mysql::rows& boost::mysql::rows::operator=(const rows_view& rhs)
{
    impl_.assign(rhs.fields_, rhs.num_fields_);
    num_columns_ = rhs.num_columns_;
    return *this;
}

boost::mysql::row_view boost::mysql::rows::at(std::size_t i) const
{
    if (i >= size())
    {
        BOOST_THROW_EXCEPTION(std::out_of_range("rows::at"));
    }
    return detail::row_slice(impl_.fields().data(), num_columns_, i);
}

boost::mysql::row_view boost::mysql::rows::operator[](std::size_t i) const noexcept
{
    BOOST_ASSERT(i < size());
    return detail::row_slice(impl_.fields().data(), num_columns_, i);
}

#endif

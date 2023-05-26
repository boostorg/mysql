//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_ROWS_IPP
#define BOOST_MYSQL_IMPL_ROWS_IPP

#pragma once

#include <boost/mysql/rows.hpp>
#include <boost/mysql/rows_view.hpp>

#include <boost/throw_exception.hpp>

#include <stdexcept>

boost::mysql::row_view boost::mysql::rows::at(std::size_t i) const
{
    if (i >= size())
    {
        BOOST_THROW_EXCEPTION(std::out_of_range("rows::at"));
    }
    return detail::row_slice(impl_.fields().data(), num_columns_, i);
}

#endif

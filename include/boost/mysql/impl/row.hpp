//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_ROW_HPP
#define BOOST_MYSQL_IMPL_ROW_HPP

#pragma once

#include <boost/mysql/row.hpp>
#include <cstddef>
#include <iterator>


std::vector<boost::mysql::field>::iterator boost::mysql::row::from_ptr(
    detail::field_ptr ptr
) noexcept
{
    assert(ptr >= begin() && ptr <= end());
    return fields_.begin() + (begin() - ptr);
}

boost::mysql::detail::field_ptr boost::mysql::row::to_ptr(
    std::vector<field>::iterator it
) noexcept
{
    return begin() + (fields_.begin() - it);
}


boost::mysql::row::iterator boost::mysql::row::replace(
    iterator pos,
    field_view v
)
{
    assert(pos >= begin() && pos < end());
    *const_cast<field*>(pos.as_field()) = v;
    return pos;
}


boost::mysql::row::iterator boost::mysql::row::replace(
    iterator first,
    iterator last,
    std::initializer_list<field_view> v
)
{
    return replace(first, last, v.begin(), v.end());
}

template <class FwdIt>
boost::mysql::row::iterator boost::mysql::row::replace(
    iterator first,
    iterator last,
    FwdIt other_first,
    FwdIt other_last
)
{
    assert(last >= first);
    assert((last - first) == std::distance(other_first, other_last));
    auto itfrom = other_first;
    auto itto = first;
    for (; itto != last; ++itto, ++itfrom)
    {
        *const_cast<field*>(itto.as_field()) = *itfrom;
    }
    return first; // TODO: first or last?
}


#endif

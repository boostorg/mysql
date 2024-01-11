//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_DATE_IPP
#define BOOST_MYSQL_IMPL_DATE_IPP

#pragma once

#include <boost/mysql/date.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>

#include <cstddef>
#include <cstdio>
#include <ostream>

std::size_t boost::mysql::date::impl_t::to_string(span<char, 32> output) const noexcept
{
    // Worst-case output is 14 chars, extra space just in case
    int res = snprintf(
        output.data(),
        output.size(),
        "%04u-%02u-%02u",
        static_cast<unsigned>(year),
        static_cast<unsigned>(month),
        static_cast<unsigned>(day)
    );
    BOOST_ASSERT(res > 0);
    return static_cast<std::size_t>(res);
}

std::ostream& boost::mysql::operator<<(std::ostream& os, const date& value)
{
    char buffer[32]{};
    std::size_t sz = detail::access::get_impl(value).to_string(buffer);
    return os << string_view(buffer, sz);
}

#endif
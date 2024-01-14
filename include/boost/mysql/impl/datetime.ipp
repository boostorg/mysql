//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_DATETIME_IPP
#define BOOST_MYSQL_IMPL_DATETIME_IPP

#pragma once

#include <boost/mysql/datetime.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>

#include <cstddef>
#include <cstdio>
#include <ostream>

std::size_t boost::mysql::datetime::impl_t::to_string(span<char, 64> output) const noexcept
{
    // Worst-case output is 37 chars, extra space just in case
    int res = snprintf(
        output.data(),
        output.size(),
        "%04u-%02u-%02u %02d:%02u:%02u.%06u",
        static_cast<unsigned>(year),
        static_cast<unsigned>(month),
        static_cast<unsigned>(day),
        static_cast<unsigned>(hour),
        static_cast<unsigned>(minute),
        static_cast<unsigned>(second),
        static_cast<unsigned>(microsecond)
    );
    BOOST_ASSERT(res > 0 && res <= 64);
    return static_cast<std::size_t>(res);
}

std::ostream& boost::mysql::operator<<(std::ostream& os, const datetime& value)
{
    char buffer[64]{};
    std::size_t sz = detail::access::get_impl(value).to_string(buffer);
    os << string_view(buffer, sz);
    return os;
}

#endif
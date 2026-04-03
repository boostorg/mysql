//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/date.hpp>

#include <cstddef>
#include <ostream>
#include <string_view>

#include "mycosql_internal/dt_to_string.hpp"

std::ostream& boost::mysql::operator<<(std::ostream& os, const date& value)
{
    char buffer[32]{};
    std::size_t sz = detail::date_to_string(value.year(), value.month(), value.day(), buffer);
    return os << std::string_view(buffer, sz);
}

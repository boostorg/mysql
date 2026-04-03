//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/datetime.hpp>

#include <cstddef>
#include <ostream>
#include <string_view>

#include "mycosql_internal/dt_to_string.hpp"

std::ostream& boost::mysql::operator<<(std::ostream& os, const datetime& value)
{
    char buffer[64]{};
    std::size_t sz = detail::datetime_to_string(
        value.year(),
        value.month(),
        value.day(),
        value.hour(),
        value.minute(),
        value.second(),
        value.microsecond(),
        buffer
    );
    os << std::string_view(buffer, sz);
    return os;
}

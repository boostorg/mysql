//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_INCLUDE_TEST_COMMON_VALIDATE_STRING_CONTAINS_HPP
#define BOOST_MYSQL_TEST_COMMON_INCLUDE_TEST_COMMON_VALIDATE_STRING_CONTAINS_HPP

#include <boost/assert/source_location.hpp>
#include <boost/core/lightweight_test.hpp>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace boost {
namespace mysql {
namespace test {

inline void validate_string_contains(
    std::string value,
    const std::vector<std::string>& to_check,
    boost::source_location loc = BOOST_CURRENT_LOCATION
)
{
    std::transform(value.begin(), value.end(), value.begin(), [](char c) {
        return static_cast<char>(tolower(c));
    });
    for (const auto& elm : to_check)
    {
        if (!BOOST_TEST(value.find(elm) != std::string::npos))
        {
            std::cerr << "   Substring '" << elm << "' not found in '" << value << "'\n"
                      << "   Called from " << loc << "\n";
        }
    }
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif

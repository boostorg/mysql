//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_INCLUDE_TEST_COMMON_VALIDATE_STRING_CONTAINS_HPP
#define BOOST_MYSQL_TEST_COMMON_INCLUDE_TEST_COMMON_VALIDATE_STRING_CONTAINS_HPP

#include <boost/assert/source_location.hpp>

#include <string>
#include <vector>

namespace boost {
namespace mysql {
namespace test {

void validate_string_contains(
    std::string value,
    const std::vector<std::string>& to_check,
    boost::source_location loc = BOOST_CURRENT_LOCATION
);

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif

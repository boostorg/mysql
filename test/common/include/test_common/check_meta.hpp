//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_INCLUDE_TEST_COMMON_CHECK_META_HPP
#define BOOST_MYSQL_TEST_COMMON_INCLUDE_TEST_COMMON_CHECK_META_HPP

// This is a lighter check than integ tests' metadata_validator

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/metadata_collection_view.hpp>

#include <string_view>
#include <vector>

namespace boost {
namespace mysql {
namespace test {

void check_meta(metadata_collection_view meta, const std::vector<column_type>& expected_types);

void check_meta(
    metadata_collection_view meta,
    const std::vector<std::pair<column_type, std::string_view>>& expected
);

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif

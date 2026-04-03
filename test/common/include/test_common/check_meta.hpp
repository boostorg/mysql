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
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_collection_view.hpp>

#include <boost/core/lightweight_test.hpp>

#include <ranges>
#include <string_view>

namespace boost {
namespace mysql {
namespace test {

inline void check_meta(metadata_collection_view meta, const std::vector<column_type>& expected_types)
{
    auto types = meta | std::ranges::views::transform([](const metadata& m) { return m.type(); });
    BOOST_TEST_ALL_EQ(types.begin(), types.end(), expected_types.begin(), expected_types.end());
}

inline void check_meta(
    metadata_collection_view meta,
    const std::vector<std::pair<column_type, std::string_view>>& expected
)
{
    auto types = meta | std::ranges::views::transform([](const metadata& m) { return m.type(); });
    auto expected_types = expected |
                          std::ranges::views::transform(
                              [](const std::pair<column_type, std::string_view>& p) { return p.first; }
                          );
    BOOST_TEST_ALL_EQ(types.begin(), types.end(), expected_types.begin(), expected_types.end());

    auto names = meta | std::ranges::views::transform([](const metadata& m) { return m.column_name(); });
    auto expected_names = expected |
                          std::ranges::views::transform(
                              [](const std::pair<column_type, std::string_view>& p) { return p.second; }
                          );
    BOOST_TEST_ALL_EQ(names.begin(), names.end(), expected_names.begin(), expected_names.end());
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif

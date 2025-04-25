//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/impl/internal/protocol/capabilities.hpp>

#include <boost/test/unit_test.hpp>

#include "test_unit/printing.hpp"

using namespace boost::mysql;
using detail::capabilities;
using detail::has_capabilities;

namespace {

BOOST_AUTO_TEST_SUITE(test_capabilities)

BOOST_AUTO_TEST_CASE(operator_or)
{
    // Two != flags
    BOOST_TEST((capabilities::long_password | capabilities::long_flag) == static_cast<capabilities>(5));

    // Same flag
    BOOST_TEST((capabilities::long_flag | capabilities::long_flag) == capabilities::long_flag);

    // Big values
    BOOST_TEST(
        (capabilities::long_password | capabilities::remember_options) ==
        static_cast<capabilities>(1 | (1 << 31))
    );
}

BOOST_AUTO_TEST_CASE(operator_and)
{
    // Single flag present
    BOOST_TEST((static_cast<capabilities>(5) & capabilities::long_password) == capabilities::long_password);

    // Single flag absent
    BOOST_TEST((static_cast<capabilities>(5) & capabilities::odbc) == capabilities{});

    // Multiple flags
    BOOST_TEST(
        (static_cast<capabilities>(11) & static_cast<capabilities>(67)) == static_cast<capabilities>(3)
    );

    // Big values
    BOOST_TEST(
        (static_cast<capabilities>(0xffffffff) & capabilities::remember_options) ==
        capabilities::remember_options
    );
}

BOOST_AUTO_TEST_CASE(has_capabilities_)
{
    constexpr auto search = capabilities::connect_with_db | capabilities::ssl | capabilities::compress;

    // No capabilities present
    BOOST_TEST(!has_capabilities(capabilities{}, search));

    // Some present, but not all
    BOOST_TEST(!has_capabilities(capabilities::connect_with_db | capabilities::compress, search));

    // Some present, but not all. Some unrelated are present
    BOOST_TEST(!has_capabilities(
        capabilities::connect_with_db | capabilities::compress | capabilities::long_flag,
        search
    ));

    // Only the requested ones are present
    BOOST_TEST(has_capabilities(search, search));

    // Has the requested ones, plus extra ones
    BOOST_TEST(has_capabilities(static_cast<capabilities>(0xffffffff), search));

    // Searching for only one capability works
    BOOST_TEST(
        has_capabilities(capabilities::connect_with_db | capabilities::compress, capabilities::compress)
    );
    BOOST_TEST(
        !has_capabilities(capabilities::connect_with_db | capabilities::compress, capabilities::long_flag)
    );

    // Searching for the empty set always returns true
    BOOST_TEST(has_capabilities(capabilities::connect_with_db | capabilities::compress, capabilities{}));
    BOOST_TEST(has_capabilities(static_cast<capabilities>(0xffffffff), capabilities{}));
}

BOOST_AUTO_TEST_SUITE_END()  // test_capabilities

}  // namespace
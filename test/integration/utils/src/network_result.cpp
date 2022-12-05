//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/test/unit_test.hpp>

#include "network_result.hpp"
#include "test_common.hpp"


using namespace boost::mysql::test;

// network_result_base
using boost::mysql::test::network_result_base;

static const char* get_message(const boost::optional<boost::mysql::error_info>& info) noexcept
{
    return info ? info->message().c_str() : "<unavailable>";
}

void network_result_base::validate_no_error() const
{
    BOOST_TEST_REQUIRE(
        err == error_code(),
        "with error_info= " << get_message(info) << ", error_code=" << err.message()
    );
    if (info)
    {
        BOOST_TEST(*info == error_info());
    }
}

void network_result_base::validate_any_error(const std::vector<std::string>& expected_msg) const
{
    BOOST_TEST_REQUIRE(err != error_code(), "with error_info= " << get_message(info));
    if (info)
    {
        validate_string_contains(info->message(), expected_msg);
    }
}

void network_result_base::validate_error(
    boost::mysql::error_code expected_errc,
    const std::vector<std::string>& expected_msg
) const
{
    BOOST_TEST_REQUIRE(err == expected_errc, "with error_info= " << get_message(info));
    if (info)
    {
        validate_string_contains(info->message(), expected_msg);
    }
}
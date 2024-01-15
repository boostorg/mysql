//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/string_view.hpp>
#include <boost/mysql/time.hpp>

#include <boost/mysql/impl/internal/dt_to_string.hpp>

#include <boost/core/span.hpp>
#include <boost/test/tools/old/interface.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>

#include <cstddef>

#include "test_common/create_basic.hpp"
#include "test_common/stringize.hpp"

using namespace boost::mysql;
using test::maket;

// date_to_string and datetime_to_string are tested in date.cpp and datetime.cpp
BOOST_AUTO_TEST_SUITE(test_time_to_string)

// Having the buffer in dynamic memory helps detect overruns
static std::string invoke_to_string(::boost::mysql::time t)
{
    std::string res(64, '\0');
    std::size_t sz = detail::time_to_string(t, boost::span<char, 64>(&res[0], 64));
    res.resize(sz);
    return res;
}

// C++ min and max should still be formattable. The actual values are not specified
BOOST_AUTO_TEST_CASE(minmax)
{
    BOOST_CHECK_NO_THROW(invoke_to_string(::boost::mysql::time::min()));
    BOOST_CHECK_NO_THROW(invoke_to_string(::boost::mysql::time::max()));
}

BOOST_AUTO_TEST_CASE(coverage)
{
    // We will list the possibilities for each component (hours, minutes, days...) and will
    // take the Cartessian product of all them
    struct component_value
    {
        const char* name;
        int v;
        const char* repr;
    };

    constexpr component_value sign_values[] = {
        {"positive", 1,  "" },
        {"negative", -1, "-"}
    };

    constexpr component_value hours_values[] = {
        {"zero",      0,   "00" },
        {"onedigit",  5,   "05" },
        {"twodigits", 23,  "23" },
        {"max",       838, "838"}
    };

    constexpr component_value mins_secs_values[] = {
        {"zero",      0,  "00"},
        {"onedigit",  5,  "05"},
        {"twodigits", 59, "59"}
    };

    constexpr component_value micros_values[] = {
        {"zero",      0,      "000000"},
        {"onedigit",  5,      "000005"},
        {"twodigits", 50,     "000050"},
        {"max",       999999, "999999"},
    };

    // clang-format off
    for (const auto& sign : sign_values)
    {
    for (const auto& hours : hours_values)
    {
    for (const auto& mins : mins_secs_values)
    {
    for (const auto& secs : mins_secs_values)
    {
    for (const auto& micros : micros_values)
    {
        BOOST_TEST_CONTEXT(
            "sign=" << sign.name << ", hours=" << hours.name <<  ", mins=" << mins.name <<
            ", secs=" << secs.name << "micros=" << micros.name
        )
        {
            // Input value
            auto t = sign.v * test::maket(hours.v, mins.v, secs.v, micros.v);
            if (sign.v == -1 && t == test::maket(0, 0, 0))
            {
                // This case makes no sense, as negative zero is represented as zero
                continue;
            }

            // Expected value
            auto expected = test::stringize(
                sign.repr, hours.repr, ':', mins.repr, ':', secs.repr, '.', micros.repr
            );

            // Call the function
            auto actual = invoke_to_string(t);

            // Check
            BOOST_TEST(actual == expected);
            
        }
    }
    }
    }
    }
    }
    // clang-format on
}

BOOST_AUTO_TEST_SUITE_END()

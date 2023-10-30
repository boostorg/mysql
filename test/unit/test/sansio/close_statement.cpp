//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/mysql/impl/internal/sansio/close_statement.hpp>

#include <boost/test/unit_test.hpp>

#include "test_common/assert_buffer_equals.hpp"
#include "test_unit/algo_test.hpp"
#include "test_unit/create_frame.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;

BOOST_AUTO_TEST_SUITE(test_close_statement)

constexpr std::uint8_t expected_message[] = {0x19, 0x03, 0x00, 0x00, 0x00};

struct fixture : algo_fixture_base
{
    detail::close_statement_algo algo{
        st,
        {&diag, 3}
    };
};

BOOST_AUTO_TEST_CASE(success)
{
    // Setup
    fixture fix;

    // Run the algo
    algo_test().expect_write(create_frame(0, expected_message)).check(fix);
}

BOOST_AUTO_TEST_CASE(error_network)
{
    algo_test().expect_write(create_frame(0, expected_message)).check_network_errors<fixture>();
}

BOOST_AUTO_TEST_SUITE_END()

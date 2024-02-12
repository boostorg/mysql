//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/impl/internal/sansio/reset_connection.hpp>

#include <boost/test/unit_test.hpp>

#include "test_common/assert_buffer_equals.hpp"
#include "test_common/create_diagnostics.hpp"
#include "test_unit/algo_test.hpp"
#include "test_unit/create_err.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;

BOOST_AUTO_TEST_SUITE(test_reset_connection)

struct fixture : algo_fixture_base
{
    detail::reset_connection_algo algo{st, {&diag}};
};

BOOST_AUTO_TEST_CASE(success)
{
    // Setup
    fixture fix;
    fix.st.current_charset = utf8mb4_charset;

    // Run the algo
    algo_test()
        .expect_write(create_frame(0, {0x1f}))
        .expect_read(create_ok_frame(1, ok_builder().build()))
        .check(fix);

    // The OK packet was processed correctly. The charset was reset
    BOOST_TEST(fix.st.backslash_escapes);
    BOOST_TEST(fix.st.charset_ptr() == nullptr);
}

BOOST_AUTO_TEST_CASE(success_no_backslash_escapes)
{
    // Setup
    fixture fix;

    // Run the algo
    algo_test()
        .expect_write(create_frame(0, {0x1f}))
        .expect_read(create_ok_frame(1, ok_builder().no_backslash_escapes(true).build()))
        .check(fix);

    // The OK packet was processed correctly.
    // It's OK to run this algo without a known charset
    BOOST_TEST(!fix.st.backslash_escapes);
    BOOST_TEST(fix.st.charset_ptr() == nullptr);
}

BOOST_AUTO_TEST_CASE(error_network)
{
    // This covers errors in read and write
    algo_test()
        .expect_write(create_frame(0, {0x1f}))
        .expect_read(create_ok_frame(1, ok_builder().build()))
        .check_network_errors<fixture>();
}

BOOST_AUTO_TEST_CASE(error_response)
{
    // Setup
    fixture fix;
    fix.st.current_charset = utf8mb4_charset;

    // Run the algo
    algo_test()
        .expect_write(create_frame(0, {0x1f}))
        .expect_read(err_builder()
                         .seqnum(1)
                         .code(common_server_errc::er_bad_db_error)
                         .message("my_message")
                         .build_frame())
        .check(fix, common_server_errc::er_bad_db_error, create_server_diag("my_message"));

    // The charset was not updated
    BOOST_TEST(fix.st.charset_ptr()->name == "utf8mb4");
}

BOOST_AUTO_TEST_SUITE_END()

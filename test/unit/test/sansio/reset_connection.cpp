//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
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
#include <boost/mysql/impl/internal/sansio/run_pipeline.hpp>

#include <boost/test/unit_test.hpp>

#include "test_common/create_diagnostics.hpp"
#include "test_common/printing.hpp"
#include "test_unit/algo_test.hpp"
#include "test_unit/create_err.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"
#include "test_unit/printing.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;

BOOST_AUTO_TEST_SUITE(test_reset_connection)

//
// read_reset_connection_response_algo
//
struct read_response_fixture : algo_fixture_base
{
    detail::read_reset_connection_response_algo algo{11};
};

BOOST_AUTO_TEST_CASE(read_response_success)
{
    // Setup
    read_response_fixture fix;
    fix.st.current_charset = utf8mb4_charset;

    // Run the algo
    algo_test()
        .expect_read(create_ok_frame(11, ok_builder().build()))
        .will_set_current_charset(character_set{})  // the charset was reset
        .check(fix);
}

BOOST_AUTO_TEST_CASE(read_response_success_no_backslash_escapes)
{
    // Setup
    read_response_fixture fix;

    // Run the algo
    algo_test()
        .expect_read(create_ok_frame(11, ok_builder().no_backslash_escapes(true).build()))
        .will_set_backslash_escapes(false)          // OK packet processed
        .will_set_current_charset(character_set{})  // charset was reset
        .check(fix);
}

BOOST_AUTO_TEST_CASE(read_response_error_network)
{
    algo_test()
        .expect_read(create_ok_frame(11, ok_builder().build()))
        .check_network_errors<read_response_fixture>();
}

BOOST_AUTO_TEST_CASE(read_response_error_packet)
{
    // Setup
    read_response_fixture fix;
    fix.st.current_charset = utf8mb4_charset;

    // Run the algo. The character set is not updated.
    algo_test()
        .expect_read(err_builder()
                         .seqnum(11)
                         .code(common_server_errc::er_bad_db_error)
                         .message("my_message")
                         .build_frame())
        .check(fix, common_server_errc::er_bad_db_error, create_server_diag("my_message"));
}

//
// setup_reset_connection_pipeline: running a pipeline with these parameters
// has the intended effect
//
struct reset_conn_fixture : algo_fixture_base
{
    detail::run_pipeline_algo algo{detail::setup_reset_connection_pipeline(st)};
};

BOOST_AUTO_TEST_CASE(success)
{
    // Setup
    reset_conn_fixture fix;
    fix.st.current_charset = utf8mb4_charset;

    // Run the algo
    algo_test()
        .expect_write(create_frame(0, {0x1f}))
        .expect_read(create_ok_frame(1, ok_builder().build()))
        .will_set_current_charset(character_set{})  // charset was reset
        .check(fix);
}

BOOST_AUTO_TEST_CASE(reset_conn_error_network)
{
    // This covers errors in read and write
    algo_test()
        .expect_write(create_frame(0, {0x1f}))
        .expect_read(create_ok_frame(1, ok_builder().build()))
        .check_network_errors<reset_conn_fixture>();
}

BOOST_AUTO_TEST_CASE(reset_conn_error_response)
{
    // Setup
    reset_conn_fixture fix;
    fix.st.current_charset = utf8mb4_charset;

    // Run the algo. The current charset was not updated
    algo_test()
        .expect_write(create_frame(0, {0x1f}))
        .expect_read(err_builder()
                         .seqnum(1)
                         .code(common_server_errc::er_bad_db_error)
                         .message("my_message")
                         .build_frame())
        .check(fix, common_server_errc::er_bad_db_error, create_server_diag("my_message"));
}

// Connection status checked correctly
BOOST_AUTO_TEST_CASE(reset_conn_error_invalid_connection_status)
{
    struct
    {
        detail::connection_status status;
        error_code expected_err;
    } test_cases[] = {
        {detail::connection_status::not_connected,             client_errc::not_connected            },
        {detail::connection_status::engaged_in_multi_function, client_errc::engaged_in_multi_function},
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.status)
        {
            // Setup
            reset_conn_fixture fix;
            fix.st.status = tc.status;

            // Run the algo
            algo_test().check(fix, tc.expected_err);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/mysql/impl/internal/sansio/close_statement.hpp>
#include <boost/mysql/impl/internal/sansio/run_pipeline.hpp>

#include <boost/test/unit_test.hpp>

#include <cstdint>

#include "test_common/buffer_concat.hpp"
#include "test_common/create_diagnostics.hpp"
#include "test_unit/algo_test.hpp"
#include "test_unit/create_err.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"
#include "test_unit/printing.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;

// setup_close_statement_pipeline: running a pipeline with these parameters
// has the intended effect
BOOST_AUTO_TEST_SUITE(test_close_statement)

// A close_statement request pipelined with a ping (frame headers included)
static std::vector<std::uint8_t> expected_request()
{
    return concat(create_frame(0, {0x19, 0x03, 0x00, 0x00, 0x00}), create_frame(0, {0x0e}));
}

struct fixture : algo_fixture_base
{
    detail::run_pipeline_algo algo{detail::setup_close_statement_pipeline(st, {3})};
};

BOOST_AUTO_TEST_CASE(success)
{
    // Setup
    fixture fix;

    // Run the algo
    algo_test()
        .expect_write(expected_request())                       // requests
        .expect_read(create_ok_frame(1, ok_builder().build()))  // response to the ping request
        .check(fix);
}

BOOST_AUTO_TEST_CASE(success_no_backslash_escapes)
{
    // Setup
    fixture fix;

    // Run the algo
    algo_test()
        .expect_write(expected_request())  // requests
        .expect_read(create_ok_frame(1, ok_builder().no_backslash_escapes(true).build())
        )  // response to the ping request
        .will_set_backslash_escapes(false)
        .check(fix);
}

BOOST_AUTO_TEST_CASE(error_network)
{
    algo_test()
        .expect_write(expected_request())                       // requests
        .expect_read(create_ok_frame(1, ok_builder().build()))  // response to the ping request
        .check_network_errors<fixture>();
}

BOOST_AUTO_TEST_CASE(error_response)
{
    // Setup
    fixture fix;

    // Run the test
    algo_test()
        .expect_write(expected_request())  // Ping request
        .expect_read(err_builder()
                         .seqnum(1)
                         .code(common_server_errc::er_bad_db_error)
                         .message("my_message")
                         .build_frame())  // Error response
        .check(fix, common_server_errc::er_bad_db_error, create_server_diag("my_message"));
}

// Connection status checked correctly
BOOST_AUTO_TEST_CASE(error_invalid_connection_status)
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
            fixture fix;
            fix.st.status = tc.status;

            // Run the algo
            algo_test().check(fix, tc.expected_err);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

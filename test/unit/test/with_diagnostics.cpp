//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/with_diagnostics.hpp>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/consign.hpp>
#include <boost/test/unit_test.hpp>

#include <exception>

#include "test_common/create_diagnostics.hpp"
#include "test_common/printing.hpp"
#include "test_common/tracker_executor.hpp"
#include "test_unit/create_err.hpp"
#include "test_unit/test_any_connection.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
namespace asio = boost::asio;

BOOST_AUTO_TEST_SUITE(test_with_diagnostics)

// Validate that an exception_ptr contains the expected values
void check_exception(std::exception_ptr exc, error_code expected_ec, const diagnostics& expected_diag)
{
    BOOST_CHECK_EXCEPTION(
        std::rethrow_exception(exc),
        error_with_diagnostics,
        [&](const error_with_diagnostics& e) {
            BOOST_TEST(e.code() == expected_ec);
            BOOST_TEST(e.get_diagnostics() == expected_diag);
            return true;
        }
    );
}

BOOST_AUTO_TEST_CASE(associated_properties)
{
    // Uses intermediate_handler internally, so we just perform a sanity check here
    // Setup
    auto ex_result = create_tracker_executor(global_context_executor());
    auto conn = create_test_any_connection();
    get_stream(conn).add_bytes(err_builder()
                                   .code(common_server_errc::er_no_such_user)
                                   .message("Invalid user")
                                   .seqnum(1)
                                   .build_frame());
    bool called = false;
    auto check_fn = [&](std::exception_ptr exc) {
        called = true;
        BOOST_TEST(current_executor_id() == ex_result.executor_id);
        check_exception(exc, common_server_errc::er_no_such_user, create_server_diag("Invalid user"));
    };

    // Call the op
    conn.async_reset_connection(with_diagnostics(asio::bind_executor(ex_result.ex, check_fn)));
    run_global_context();

    // Sanity check
    BOOST_TEST(called);
}

// Edge case: if a diagnostics* gets passed as an argument
// to consign(), we don't mess things up
BOOST_AUTO_TEST_CASE(several_diagnostics_args)
{
    // Setup
    auto conn = create_test_any_connection();
    get_stream(conn).add_bytes(err_builder()
                                   .code(common_server_errc::er_no_such_user)
                                   .message("Invalid user")
                                   .seqnum(1)
                                   .build_frame());
    diagnostics other_diag;
    bool called = false;

    // Execute
    conn.async_reset_connection(asio::consign(
        with_diagnostics([&](std::exception_ptr exc) {
            called = true;
            check_exception(exc, common_server_errc::er_no_such_user, create_server_diag("Invalid user"));
        }),
        &other_diag
    ));
    run_global_context();

    // Sanity check
    BOOST_TEST(called);
    BOOST_TEST(other_diag == diagnostics());  // unmodified
}

BOOST_AUTO_TEST_SUITE_END()

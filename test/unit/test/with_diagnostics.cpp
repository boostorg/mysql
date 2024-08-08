//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/with_diagnostics.hpp>

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

// Edge case: a completion token happens to pass
// an additional diagnostics* as an initiation arg
BOOST_AUTO_TEST_CASE(several_diagnostics_args)
{
    // Setup
    auto conn = create_test_any_connection();
    get_stream(conn).add_bytes(
        err_builder().code(common_server_errc::er_no_such_user).message("Invalid user").seqnum(1).build_body()
    );
    results r;
    diagnostics other_diag;

    // Execute
    std::exception_ptr exc;
    conn.async_execute(
        "SELECT 1",
        r,
        asio::consign(with_diagnostics([&](std::exception_ptr exc_arg) { exc = exc_arg; }), &other_diag)
    );
    run_global_context();

    // Check results
    BOOST_CHECK_EXCEPTION(
        std::rethrow_exception(exc),
        error_with_diagnostics,
        [](const error_with_diagnostics& e) {
            BOOST_TEST(e.code() == common_server_errc::er_no_such_user);
            BOOST_TEST(e.get_diagnostics() == create_server_diag("Invalid user"));
            return true;
        }
    );
}

BOOST_AUTO_TEST_SUITE_END()

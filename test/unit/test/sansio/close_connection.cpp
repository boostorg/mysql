//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/impl/internal/sansio/close_connection.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

#include <boost/asio/error.hpp>
#include <boost/test/unit_test.hpp>

#include <vector>

#include "test_unit/algo_test.hpp"
#include "test_unit/create_frame.hpp"

namespace asio = boost::asio;
using namespace boost::mysql::test;
using namespace boost::mysql;
using detail::connection_status;

namespace {

BOOST_AUTO_TEST_SUITE(test_close_connection)

struct fixture : algo_fixture_base
{
    detail::close_connection_algo algo{{}};
};

// A serialized quit request
std::vector<std::uint8_t> expected_request() { return create_frame(0, {0x01}); }

BOOST_AUTO_TEST_CASE(success)
{
    // Setup
    fixture fix;

    // Run the algo
    algo_test()
        .expect_write(expected_request())
        .expect_close()
        .will_set_status(connection_status::not_connected)
        .check(fix);
}

// If we're using TLS, close calls quit, which shuts it down
BOOST_AUTO_TEST_CASE(success_tls)
{
    // Setup
    fixture fix;
    fix.st.tls_active = true;

    // Run the algo
    algo_test()
        .expect_write(expected_request())
        .expect_ssl_shutdown()
        .expect_close()
        .will_set_status(connection_status::not_connected)
        .will_set_tls_active(false)
        .check(fix);
}

// If the session hasn't been established, or has been already torn down, close is a no-op
BOOST_AUTO_TEST_CASE(success_multi_function)
{
    // Setup
    fixture fix;
    fix.st.status = connection_status::engaged_in_multi_function;

    // Run the algo
    algo_test()
        .expect_write(expected_request())
        .expect_close()
        .will_set_status(connection_status::not_connected)
        .check(fix);
}

// Close runs normally even if the connection is engaged in a multi-function operation
BOOST_AUTO_TEST_CASE(not_connected)
{
    // Setup
    fixture fix;
    fix.st.status = connection_status::not_connected;

    // Run the algo
    algo_test().check(fix);
}

BOOST_AUTO_TEST_CASE(error_close)
{
    // Setup
    fixture fix;

    // Run the algo
    algo_test()
        .expect_write(expected_request())
        .expect_close(asio::error::network_reset)
        .will_set_status(connection_status::not_connected)  // State change happens even if close fails
        .check(fix, asio::error::network_reset);
}

BOOST_AUTO_TEST_CASE(error_quit)
{
    // Setup
    fixture fix;

    // Run the algo
    algo_test()
        .expect_write(expected_request(), asio::error::network_reset)
        .expect_close()                                     // close is issued even if quit fails
        .will_set_status(connection_status::not_connected)  // state change happens even if quit fails
        .check(fix, asio::error::network_reset);            // error code is propagated
}

BOOST_AUTO_TEST_CASE(error_quit_close)
{
    // Setup
    fixture fix;

    // Run the algo
    algo_test()
        .expect_write(expected_request(), asio::error::network_reset)
        .expect_close(asio::error::shut_down)
        .will_set_status(connection_status::not_connected)  // state change happens even if quit fails
        .check(fix, asio::error::network_reset);            // the 1st error code wins
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

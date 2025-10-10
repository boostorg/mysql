//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/quit_connection.hpp>

#include <boost/asio/error.hpp>
#include <boost/test/unit_test.hpp>

#include <vector>

#include "test_unit/algo_test.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/printing.hpp"

namespace asio = boost::asio;
using namespace boost::mysql::test;
using namespace boost::mysql;
using detail::connection_status;

namespace {

BOOST_AUTO_TEST_SUITE(test_quit_connection)

struct fixture : algo_fixture_base
{
    detail::quit_connection_algo algo{{}};
};

// A serialized quit request
std::vector<std::uint8_t> expected_request() { return create_frame(0, {0x01}); }

BOOST_AUTO_TEST_CASE(plaintext_success)
{
    // Setup
    fixture fix;

    // Run the algo
    algo_test().expect_write(expected_request()).will_set_status(connection_status::not_connected).check(fix);
}

BOOST_AUTO_TEST_CASE(plaintext_error_network)
{
    // Setup
    fixture fix;

    // Run the algo. State change happens even if the quit request fails
    algo_test()
        .expect_write(expected_request(), asio::error::network_reset)
        .will_set_status(connection_status::not_connected)
        .check(fix, asio::error::network_reset);
}

BOOST_AUTO_TEST_CASE(ssl_success)
{
    // Setup
    fixture fix;
    fix.st.tls_active = true;

    // Run the algo
    algo_test()
        .expect_write(expected_request())
        .expect_ssl_shutdown()
        .will_set_status(connection_status::not_connected)
        .will_set_tls_active(false)
        .check(fix);
}

BOOST_AUTO_TEST_CASE(ssl_error_quit)
{
    // Setup
    fixture fix;
    fix.st.tls_active = true;

    // Run the algo
    algo_test()
        .expect_write(expected_request(), asio::error::network_reset)
        .will_set_status(connection_status::not_connected)
        .will_set_tls_active(false)
        .check(fix, asio::error::network_reset);
}

BOOST_AUTO_TEST_CASE(ssl_error_shutdown)
{
    // Setup
    fixture fix;
    fix.st.tls_active = true;

    // Run the algo
    algo_test()
        .expect_write(expected_request())
        .expect_ssl_shutdown(asio::error::network_reset)
        .will_set_status(connection_status::not_connected)
        .will_set_tls_active(false)
        .check(fix);  // SSL shutdown errors ignored
}

// quit runs regardless of the session status we have
BOOST_AUTO_TEST_CASE(status_ignored)
{
    constexpr connection_status test_status[] = {
        connection_status::not_connected,
        connection_status::engaged_in_multi_function
    };

    for (const auto status : test_status)
    {
        BOOST_TEST_CONTEXT(status)
        {
            // Setup
            fixture fix;
            fix.st.status = connection_status::not_connected;

            // Run the algo
            algo_test().expect_write(expected_request()).check(fix);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

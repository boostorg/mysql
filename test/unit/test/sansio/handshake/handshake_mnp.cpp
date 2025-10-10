//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>

#include "handshake_common.hpp"
#include "test_common/create_diagnostics.hpp"
#include "test_unit/create_err.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
using detail::capabilities;
using detail::connection_status;

namespace {

BOOST_AUTO_TEST_SUITE(test_handshake_mnp)

BOOST_AUTO_TEST_CASE(ok)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_scramble).build())
        .expect_write(login_request_builder().auth_response(mnp_hash).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

BOOST_AUTO_TEST_CASE(err)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_scramble).build())
        .expect_write(login_request_builder().auth_response(mnp_hash).build())
        .expect_read(err_builder()
                         .seqnum(2)
                         .code(common_server_errc::er_access_denied_error)
                         .message("Denied")
                         .build_frame())
        .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Denied"));
}

// The flows with auth switch work
BOOST_AUTO_TEST_CASE(authswitch_ok)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_scramble).build()
        )
        .expect_write(
            login_request_builder().auth_plugin("caching_sha2_password").auth_response(csha2p_hash).build()
        )
        .expect_read(create_auth_switch_frame(2, "mysql_native_password", mnp_scramble))
        .expect_write(create_frame(3, mnp_hash))
        .expect_read(create_ok_frame(4, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

// mysql_native_password doesn't have interactions with TLS
BOOST_AUTO_TEST_CASE(mnp_tls)
{
    // Setup
    handshake_fixture fix;
    fix.st.tls_supported = true;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(tls_caps).auth_data(mnp_scramble).build())
        .expect_write(create_ssl_request())
        .expect_ssl_handshake()
        .expect_write(login_request_builder().seqnum(2).caps(tls_caps).auth_response(mnp_hash).build())
        .expect_read(create_ok_frame(3, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_tls_active(true)
        .will_set_capabilities(tls_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

// mysql_native_password does not support more_data packets
BOOST_AUTO_TEST_CASE(moredata)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_scramble).build())
        .expect_write(login_request_builder().auth_response(mnp_hash).build())
        .expect_read(create_more_data_frame(2, mnp_scramble))
        .check(fix, client_errc::bad_handshake_packet_type);
}

BOOST_AUTO_TEST_CASE(authswitch_moredata)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_scramble).build()
        )
        .expect_write(
            login_request_builder().auth_plugin("caching_sha2_password").auth_response(csha2p_hash).build()
        )
        .expect_read(create_auth_switch_frame(2, "mysql_native_password", mnp_scramble))
        .expect_write(create_frame(3, mnp_hash))
        .expect_read(create_more_data_frame(4, mnp_scramble))
        .check(fix, client_errc::bad_handshake_packet_type);
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

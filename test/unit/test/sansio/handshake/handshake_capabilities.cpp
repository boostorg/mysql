//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "handshake_common.hpp"
#include "test_common/printing.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
using detail::capabilities;
using detail::connection_status;

namespace {

BOOST_AUTO_TEST_SUITE(test_handshake_capabilities)

//
// connect with db
//

constexpr auto db_caps = min_caps | capabilities::connect_with_db;

BOOST_AUTO_TEST_CASE(db_nonempty_supported)
{
    // Setup
    handshake_fixture fix(handshake_params("example_user", "example_password", "mydb"));

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(db_caps).auth_data(mnp_scramble).build())
        .expect_write(login_request_builder().caps(db_caps).auth_response(mnp_hash).db("mydb").build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(db_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

BOOST_AUTO_TEST_CASE(db_nonempty_unsupported)
{
    // Setup
    handshake_fixture fix(handshake_params("example_user", "example_password", "mydb"));

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(min_caps).auth_data(mnp_scramble).build())
        .check(fix, client_errc::server_unsupported);
}

// If the user didn't request a DB, we don't send it
BOOST_AUTO_TEST_CASE(db_empty_supported)
{
    // Setup
    handshake_fixture fix(handshake_params("example_user", "example_password", ""));

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(db_caps).auth_data(mnp_scramble).build())
        .expect_write(login_request_builder().caps(min_caps).auth_response(mnp_hash).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

// If the server doesn't support connect with DB but the user didn't request it, we don't fail
BOOST_AUTO_TEST_CASE(db_empty_unsupported)
{
    // Setup
    handshake_fixture fix(handshake_params("example_user", "example_password", ""));

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

//
// multi_queries
//

constexpr auto multiq_caps = min_caps | capabilities::multi_statements;

// We request it and the server supports it
BOOST_AUTO_TEST_CASE(multiq_true_supported)
{
    // Setup
    handshake_params hparams("example_user", "example_password");
    hparams.set_multi_queries(true);
    handshake_fixture fix(hparams);

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(multiq_caps).auth_data(mnp_scramble).build())
        .expect_write(login_request_builder().caps(multiq_caps).auth_response(mnp_hash).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(multiq_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

// We request is but the serve doesn't support it
BOOST_AUTO_TEST_CASE(multiq_true_unsupported)
{
    // Setup
    handshake_params hparams("example_user", "example_password");
    hparams.set_multi_queries(true);
    handshake_fixture fix(hparams);

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(min_caps).auth_data(mnp_scramble).build())
        .check(fix, client_errc::server_unsupported);
}

// We don't request it but the server supports it. We request the server to disable it
BOOST_AUTO_TEST_CASE(multiq_false_supported)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(multiq_caps).auth_data(mnp_scramble).build())
        .expect_write(login_request_builder().caps(min_caps).auth_response(mnp_hash).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

// We don't request it and the server doesn't support it, either. That's OK
BOOST_AUTO_TEST_CASE(multiq_false_unsupported)
{
    // Setup
    handshake_fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(min_caps).auth_data(mnp_scramble).build())
        .expect_write(login_request_builder().caps(min_caps).auth_response(mnp_hash).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

//
// TLS
//

// Cases where we successfully negotiate the use of TLS
BOOST_AUTO_TEST_CASE(tls_on)
{
    for (auto mode : {ssl_mode::enable, ssl_mode::require})
    {
        BOOST_TEST_CONTEXT(mode)
        {
            // Setup
            handshake_params hparams("example_user", "example_password");
            hparams.set_ssl(mode);
            handshake_fixture fix(hparams, false);  // TLS only negotiated when the transport is not secure
            fix.st.tls_supported = true;            // TLS only negotiated if supported

            // Run the test
            algo_test()
                .expect_read(server_hello_builder().caps(tls_caps).auth_data(mnp_scramble).build())
                .expect_write(create_ssl_request())
                .expect_ssl_handshake()
                .expect_write(login_request_builder().seqnum(2).caps(tls_caps).auth_response(mnp_hash).build()
                )
                .expect_read(create_ok_frame(3, ok_builder().build()))
                .will_set_status(connection_status::ready)
                .will_set_tls_active(true)
                .will_set_capabilities(tls_caps)
                .will_set_current_charset(utf8mb4_charset)
                .will_set_connection_id(42)
                .check(fix);
        }
    }
}

// Cases where we negotiate that we won't use any TLS
BOOST_AUTO_TEST_CASE(tls_off)
{
    // TODO: handshake should be in charge of not negotiating
    // TLS when a secure channel is in place. Enable these tests then
    // constexpr struct
    // {
    //     const char* name;
    //     ssl_mode mode;
    //     bool secure_transport;
    //     bool transport_supports_tls;
    //     capabilities server_caps;
    // } test_cases[] = {
    //     {"disable_insecure_clino_serverno",   ssl_mode::disable, false, false, min_caps},
    //     {"disable_insecure_clino_serveryes",  ssl_mode::disable, false, false, tls_caps},
    //     {"disable_insecure_cliyes_serverno",  ssl_mode::disable, false, true,  min_caps},
    //     {"disable_insecure_cliyes_serveryes", ssl_mode::disable, false, true,  tls_caps},
    //     {"disable_secure_clino_serverno",     ssl_mode::disable, true,  false, min_caps},
    //     {"disable_secure_clino_serveryes",    ssl_mode::disable, true,  false, tls_caps},
    //     {"disable_secure_cliyes_serverno",    ssl_mode::disable, true,  true,  min_caps},
    //     {"disable_secure_cliyes_serveryes",   ssl_mode::disable, true,  true,  tls_caps},

    //     {"enable_insecure_clino_serverno",    ssl_mode::enable,  false, false, min_caps},
    //     {"enable_insecure_clino_serveryes",   ssl_mode::enable,  false, false, tls_caps},
    //     {"enable_insecure_cliyes_serverno",   ssl_mode::enable,  false, true,  min_caps},
    //     {"enable_secure_clino_serverno",      ssl_mode::enable,  true,  false, min_caps},
    //     {"enable_secure_clino_serveryes",     ssl_mode::enable,  true,  false, tls_caps},
    //     {"enable_secure_cliyes_serverno",     ssl_mode::enable,  true,  true,  min_caps},
    //     // {"enable_secure_cliyes_serveryes",    ssl_mode::enable,  true,  true,  tls_caps},

    //     {"require_insecure_clino_serverno",   ssl_mode::require, false, false, min_caps},
    //     {"require_insecure_clino_serveryes",  ssl_mode::require, false, false, tls_caps},
    //     {"require_secure_clino_serverno",     ssl_mode::require, true,  false, min_caps},
    //     {"require_secure_clino_serveryes",    ssl_mode::require, true,  false, tls_caps},
    //     {"require_secure_cliyes_serverno",    ssl_mode::require, true,  true,  min_caps},
    //     {"require_secure_cliyes_serveryes",   ssl_mode::require, true,  true,  tls_caps},
    // };

    constexpr struct
    {
        const char* name;
        ssl_mode mode;
        bool transport_supports_tls;
        capabilities server_caps;
    } test_cases[] = {
        {"disable_clino_serverno",   ssl_mode::disable, false, min_caps},
        {"disable_clino_serveryes",  ssl_mode::disable, false, tls_caps},
        {"disable_cliyes_serverno",  ssl_mode::disable, true,  min_caps},
        {"disable_cliyes_serveryes", ssl_mode::disable, true,  tls_caps},

        {"enable_clino_serverno",    ssl_mode::enable,  false, min_caps},
        {"enable_clino_serveryes",   ssl_mode::enable,  false, tls_caps},
        {"enable_cliyes_serverno",   ssl_mode::enable,  true,  min_caps},

        {"require_clino_serverno",   ssl_mode::require, false, min_caps},
        {"require_clino_serveryes",  ssl_mode::require, false, tls_caps},
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            handshake_params hparams("example_user", "example_password");
            hparams.set_ssl(tc.mode);
            handshake_fixture fix(hparams, false);
            fix.st.tls_supported = tc.transport_supports_tls;

            // Run the test
            algo_test()
                .expect_read(server_hello_builder().caps(tc.server_caps).auth_data(mnp_scramble).build())
                .expect_write(login_request_builder().caps(min_caps).auth_response(mnp_hash).build())
                .expect_read(create_ok_frame(2, ok_builder().build()))
                .will_set_status(connection_status::ready)
                .will_set_capabilities(min_caps)
                .will_set_current_charset(utf8mb4_charset)
                .will_set_connection_id(42)
                .check(fix);
        }
    }
}

// We strongly want TLS but the server doesn't support it
BOOST_AUTO_TEST_CASE(tls_error_unsupported)
{
    // Setup
    handshake_params hparams("example_user", "example_password");
    hparams.set_ssl(ssl_mode::require);
    handshake_fixture fix(hparams, false);  // This doesn't happen if the transport is already secure
    fix.st.tls_supported = true;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(min_caps).auth_data(mnp_scramble).build())
        .check(fix, client_errc::server_doesnt_support_ssl);
}

//
// Base capabilities
//

// If the server doesn't have these, we can't talk to it
BOOST_AUTO_TEST_CASE(caps_mandatory)
{
    constexpr struct
    {
        const char* name;
        capabilities caps;
    } test_cases[] = {
        {"no_plugin_auth",
         capabilities::protocol_41 | capabilities::plugin_auth_lenenc_data | capabilities::deprecate_eof |
             capabilities::secure_connection                                                            },
        {"no_protocol_41",
         capabilities::plugin_auth | capabilities::plugin_auth_lenenc_data | capabilities::deprecate_eof |
             capabilities::secure_connection                                                            },
        {"no_plugin_auth_lenenc_data",
         capabilities::plugin_auth | capabilities::protocol_41 | capabilities::deprecate_eof |
             capabilities::secure_connection                                                            },
        {"no_deprecate_eof",
         capabilities::plugin_auth | capabilities::protocol_41 | capabilities::plugin_auth_lenenc_data |
             capabilities::secure_connection                                                            },
        {"no_secure_connection",
         capabilities::plugin_auth | capabilities::protocol_41 | capabilities::plugin_auth_lenenc_data |
             capabilities::deprecate_eof                                                                },
        {"several_missing",            capabilities::plugin_auth | capabilities::plugin_auth_lenenc_data},
        {"none",                       capabilities{}                                                   },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            handshake_fixture fix;

            // Run the test
            algo_test()
                .expect_read(server_hello_builder().caps(tc.caps).auth_data(mnp_scramble).build())
                .check(fix, client_errc::server_unsupported);
        }
    }
}

// If the server doesn't have them, it's OK (but better if it has them)
BOOST_AUTO_TEST_CASE(caps_optional)
{
    constexpr struct
    {
        const char* name;
        capabilities caps;
    } test_cases[] = {
        {"multi_results",    capabilities::multi_results   },
        {"ps_multi_results", capabilities::ps_multi_results},
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            handshake_fixture fix;

            // Run the test
            algo_test()
                .expect_read(server_hello_builder().caps(min_caps | tc.caps).auth_data(mnp_scramble).build())
                .expect_write(login_request_builder().caps(min_caps | tc.caps).auth_response(mnp_hash).build()
                )
                .expect_read(create_ok_frame(2, ok_builder().build()))
                .will_set_status(connection_status::ready)
                .will_set_capabilities(min_caps | tc.caps)
                .will_set_current_charset(utf8mb4_charset)
                .will_set_connection_id(42)
                .check(fix);
        }
    }
}

// We don't understand these capabilities, so we set them to off even if the server supports them
BOOST_AUTO_TEST_CASE(caps_ignored)
{
    constexpr struct
    {
        const char* name;
        capabilities caps;
    } test_cases[] = {
        {"long_password",                capabilities::long_password               },
        {"found_rows",                   capabilities::found_rows                  },
        {"long_flag",                    capabilities::long_flag                   },
        {"no_schema",                    capabilities::no_schema                   },
        {"compress",                     capabilities::compress                    },
        {"odbc",                         capabilities::odbc                        },
        {"local_files",                  capabilities::local_files                 },
        {"ignore_space",                 capabilities::ignore_space                },
        {"interactive",                  capabilities::interactive                 },
        {"ignore_sigpipe",               capabilities::ignore_sigpipe              },
        {"transactions",                 capabilities::transactions                },
        {"reserved",                     capabilities::reserved                    },
        {"connect_attrs",                capabilities::connect_attrs               },
        {"can_handle_expired_passwords", capabilities::can_handle_expired_passwords},
        {"session_track",                capabilities::session_track               },
        {"ssl_verify_server_cert",       capabilities::ssl_verify_server_cert      },
        {"optional_resultset_metadata",  capabilities::optional_resultset_metadata },
        {"remember_options",             capabilities::remember_options            },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            handshake_fixture fix;

            // Run the test
            algo_test()
                .expect_read(server_hello_builder().caps(min_caps | tc.caps).auth_data(mnp_scramble).build())
                .expect_write(login_request_builder().caps(min_caps).auth_response(mnp_hash).build())
                .expect_read(create_ok_frame(2, ok_builder().build()))
                .will_set_status(connection_status::ready)
                .will_set_capabilities(min_caps)
                .will_set_current_charset(utf8mb4_charset)
                .will_set_connection_id(42)
                .check(fix);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

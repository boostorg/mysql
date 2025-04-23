//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/mysql_collations.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/character_set.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>

#include <boost/mysql/impl/internal/protocol/capabilities.hpp>
#include <boost/mysql/impl/internal/protocol/db_flavor.hpp>
#include <boost/mysql/impl/internal/protocol/frame_header.hpp>
#include <boost/mysql/impl/internal/protocol/impl/protocol_types.hpp>
#include <boost/mysql/impl/internal/protocol/impl/serialization_context.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/handshake.hpp>

#include <boost/core/span.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "test_common/create_diagnostics.hpp"
#include "test_common/printing.hpp"
#include "test_unit/algo_test.hpp"
#include "test_unit/create_err.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"
#include "test_unit/printing.hpp"
#include "test_unit/serialize_to_vector.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
using detail::capabilities;
using detail::connection_status;

namespace {

BOOST_AUTO_TEST_SUITE(test_handshake)

// Capabilities
constexpr capabilities min_caps{
    detail::CLIENT_PLUGIN_AUTH | detail::CLIENT_PROTOCOL_41 | detail::CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA |
    detail::CLIENT_DEPRECATE_EOF | detail::CLIENT_SECURE_CONNECTION
};

constexpr capabilities tls_caps{min_caps.get() | detail::CLIENT_SSL};

// Helpers to create the relevant packets
class server_hello_builder
{
    string_view server_version_{"8.1.33"};
    std::vector<std::uint8_t> auth_plugin_data_;
    detail::capabilities server_caps_{min_caps};
    string_view auth_plugin_name_{"mysql_native_password"};
    std::uint32_t connection_id_{42};

public:
    server_hello_builder& version(string_view v)
    {
        server_version_ = v;
        return *this;
    }
    server_hello_builder& auth_data(std::vector<std::uint8_t> v)
    {
        BOOST_ASSERT(v.size() >= 8u);  // split in two parts, and the 1st one is fixed size at 8 bytes
        BOOST_ASSERT(v.size() <= 0xfeu);
        auth_plugin_data_ = std::move(v);
        return *this;
    }
    server_hello_builder& caps(detail::capabilities v)
    {
        server_caps_ = v;
        return *this;
    }
    server_hello_builder& auth_plugin(string_view v)
    {
        auth_plugin_name_ = v;
        return *this;
    }
    server_hello_builder& connection_id(std::uint32_t v)
    {
        connection_id_ = v;
        return *this;
    }

    std::vector<std::uint8_t> build() const
    {
        return create_frame(
            0,
            serialize_to_vector([this](detail::serialization_context& ctx) {
                using namespace detail;

                // Auth plugin data is separated in two parts
                string_fixed<8> plugin_data_1{};
                auto plug_data_begin = reinterpret_cast<const char*>(auth_plugin_data_.data());
                std::copy(plug_data_begin, plug_data_begin + 8, plugin_data_1.value.data());
                auto plugin_data_2 = boost::span<const std::uint8_t>(auth_plugin_data_).subspan(8);

                // Capabilities is also divided in 2 parts
                string_fixed<2> caps_low{};
                string_fixed<2> caps_high{};
                std::uint32_t caps_little = boost::endian::native_to_little(server_caps_.get());
                auto* caps_begin = reinterpret_cast<const char*>(&caps_little);
                std::copy(caps_begin, caps_begin + 2, caps_low.value.data());
                std::copy(caps_begin + 2, caps_begin + 4, caps_high.value.data());

                ctx.serialize(
                    int1{10},  // protocol_version
                    string_null{server_version_},
                    int4{connection_id_},  // connection_id
                    plugin_data_1,         // plugin data, 1st part
                    int1{0},               // filler
                    caps_low,
                    int1{25},  // character set
                    int2{0},   // status flags
                    caps_high,
                    int1{static_cast<std::uint8_t>(auth_plugin_data_.size() + 1u)
                    },                  // auth plugin data length
                    string_fixed<10>{}  // reserved
                );
                ctx.add(plugin_data_2);
                ctx.add(0);  // extra null byte that mysql adds here
                ctx.serialize(string_null{auth_plugin_name_});
            })
        );
    }
};

class login_request_builder
{
    std::uint8_t seqnum_{1};
    capabilities caps_{min_caps};
    std::uint32_t collation_id_{45};  // utf8_general_ci
    string_view username_{"example_user"};
    std::vector<std::uint8_t> auth_response_;
    string_view database_{};
    string_view auth_plugin_name_{"mysql_native_password"};

public:
    login_request_builder() = default;

    login_request_builder& seqnum(std::uint8_t v)
    {
        seqnum_ = v;
        return *this;
    }

    login_request_builder& caps(detail::capabilities v)
    {
        caps_ = v;
        return *this;
    }

    login_request_builder& collation(std::uint32_t v)
    {
        collation_id_ = v;
        return *this;
    }

    login_request_builder& username(string_view v)
    {
        username_ = v;
        return *this;
    }

    login_request_builder& auth_response(std::vector<std::uint8_t> v)
    {
        auth_response_ = std::move(v);
        return *this;
    }

    login_request_builder& db(string_view v)
    {
        database_ = v;
        return *this;
    }

    login_request_builder& auth_plugin(string_view v)
    {
        auth_plugin_name_ = v;
        return *this;
    }

    std::vector<std::uint8_t> build() const
    {
        auto body = serialize_to_vector([this](detail::serialization_context& ctx) {
            ctx.serialize(detail::login_request{
                caps_,
                detail::max_packet_size,
                collation_id_,
                username_,
                auth_response_,
                database_,
                auth_plugin_name_
            });
        });
        return create_frame(seqnum_, body);
    }
};

class ssl_request_builder
{
    capabilities caps_{tls_caps};
    std::uint32_t collation_id_{45};

public:
    ssl_request_builder() = default;

    std::vector<std::uint8_t> build()
    {
        const auto body = serialize_to_vector([this](detail::serialization_context& ctx) {
            detail::ssl_request req{caps_, detail::max_packet_size, collation_id_};
            req.serialize(ctx);
        });
        return create_frame(1, body);
    }
};

std::vector<std::uint8_t> create_auth_switch_frame(
    std::uint8_t seqnum,
    string_view plugin_name,
    boost::span<const std::uint8_t> data
)
{
    return create_frame(seqnum, serialize_to_vector([=](detail::serialization_context& ctx) {
                            ctx.add(0xfe);  // auth switch header
                            detail::string_null{plugin_name}.serialize(ctx);
                            ctx.add(data);
                            ctx.add(std::uint8_t(0));  // This has a NULL byte at the end
                        }));
}

std::vector<std::uint8_t> create_more_data_frame(std::uint8_t seqnum, boost::span<const std::uint8_t> data)
{
    return create_frame(seqnum, serialize_to_vector([=](detail::serialization_context& ctx) {
                            ctx.add(0x01);  // more data header
                            ctx.add(data);
                        }));
}

struct fixture : algo_fixture_base
{
    detail::handshake_algo algo;

    fixture(
        const handshake_params& hparams = handshake_params("example_user", "example_password"),
        bool secure_transport = false
    )
        : algo({hparams, secure_transport})
    {
        st.status = connection_status::not_connected;
    }
};

// These challenge/responses have been captured with Wireshark
std::vector<std::uint8_t> mnp_challenge()
{
    return {0x1b, 0x0f, 0x6e, 0x59, 0x1b, 0x70, 0x33, 0x01, 0x0c, 0x01,
            0x7e, 0x2e, 0x30, 0x7a, 0x79, 0x5c, 0x02, 0x50, 0x51, 0x35};
}

std::vector<std::uint8_t> mnp_response()
{
    return {0xbe, 0xa5, 0xb5, 0xe7, 0x9c, 0x05, 0x23, 0x34, 0xda, 0x06,
            0x1d, 0xaf, 0xd9, 0x8b, 0x4b, 0x09, 0x86, 0xe5, 0xd1, 0x4a};
}

std::vector<std::uint8_t> csha2p_challenge()
{
    return {0x6f, 0x1b, 0x3b, 0x64, 0x39, 0x01, 0x46, 0x44, 0x53, 0x3b,
            0x74, 0x3c, 0x3e, 0x3c, 0x3c, 0x0b, 0x30, 0x77, 0x1a, 0x49};
}

std::vector<std::uint8_t> csha2p_response()
{
    return {0xa7, 0xc3, 0x7f, 0x88, 0x25, 0xec, 0x92, 0x2c, 0x88, 0xba, 0x47, 0x04, 0x14, 0xd2, 0xa3, 0xa3,
            0x5e, 0xa9, 0x41, 0x8e, 0xdc, 0x89, 0xeb, 0xe2, 0xa1, 0xec, 0xd8, 0x4f, 0x73, 0xa1, 0x49, 0x60};
}

//
// mysql_native_password
//

BOOST_AUTO_TEST_CASE(mnp_ok)
{
    // Setup
    fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_challenge()).build())
        .expect_write(login_request_builder().auth_response(mnp_response()).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

BOOST_AUTO_TEST_CASE(mnp_err)
{
    // Setup
    fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_challenge()).build())
        .expect_write(login_request_builder().auth_response(mnp_response()).build())
        .expect_read(err_builder()
                         .seqnum(2)
                         .code(common_server_errc::er_access_denied_error)
                         .message("Denied")
                         .build_frame())
        .will_set_capabilities(min_caps)  // incidental
        .will_set_connection_id(42)       // incidental
        .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Denied"));
}

// The authentication plugin generates an error during fast track
BOOST_AUTO_TEST_CASE(mnp_bad_challenge_length)
{
    // Setup
    fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(std::vector<std::uint8_t>(21, 0x0a)).build())
        .will_set_capabilities(min_caps)  // incidental
        .will_set_connection_id(42)       // incidental
        .check(fix, client_errc::protocol_value_error);
}

// Receiving a more data message at this point is illegal
// TODO: re-enable this test when we improve the handshake algorithm
// BOOST_AUTO_TEST_CASE(mnp_moredata)
// {
//     // Setup
//     fixture fix;

//     // Run the test
//     algo_test()
//         .expect_read(server_hello_builder().auth_data(mnp_challenge()).build())
//         .expect_write(login_request_builder().auth_response(mnp_response()).build())
//         .expect_read(create_more_data_frame(2, mnp_challenge()))
//         .will_set_capabilities(min_caps)  // incidental
//         .will_set_connection_id(42)       // incidental
//         .check(fix, client_errc::protocol_value_error);
// }

BOOST_AUTO_TEST_CASE(mnp_authswitch_ok)
{
    // Setup
    fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge()).build()
        )
        .expect_write(login_request_builder()
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response())
                          .build())
        .expect_read(create_auth_switch_frame(2, "mysql_native_password", mnp_challenge()))
        .expect_write(create_frame(3, mnp_response()))
        .expect_read(create_ok_frame(4, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

BOOST_AUTO_TEST_CASE(mnp_authswitch_error)
{
    // Setup
    fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge()).build()
        )
        .expect_write(login_request_builder()
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response())
                          .build())
        .expect_read(create_auth_switch_frame(2, "mysql_native_password", mnp_challenge()))
        .expect_write(create_frame(3, mnp_response()))
        .expect_read(err_builder()
                         .seqnum(4)
                         .code(common_server_errc::er_access_denied_error)
                         .message("Denied")
                         .build_frame())
        .will_set_capabilities(min_caps)  // incidental
        .will_set_connection_id(42)       // incidental
        .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Denied"));
}

// The authentication plugin generates an error during auth switch
BOOST_AUTO_TEST_CASE(mnp_authswitch_bad_challenge_length)
{
    // Setup
    fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge()).build()
        )
        .expect_write(login_request_builder()
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response())
                          .build())
        .expect_read(create_auth_switch_frame(2, "mysql_native_password", std::vector<std::uint8_t>(21, 0x0a))
        )
        .will_set_capabilities(min_caps)  // incidental
        .will_set_connection_id(42)       // incidental
        .check(fix, client_errc::protocol_value_error);
}

// After receiving an auth switch, receiving another one is illegal
// TODO: re-enable this test when we make the handshake more robust
// BOOST_AUTO_TEST_CASE(mnp_authswitch_authswitch)
// {
//     // Setup
//     fixture fix;

//     // Run the test
//     algo_test()
//         .expect_read(
//             server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge()).build()
//         )
//         .expect_write(login_request_builder()
//                           .auth_plugin("caching_sha2_password")
//                           .auth_response(csha2p_response())
//                           .build())
//         .expect_read(create_auth_switch_frame(2, "mysql_native_password", mnp_challenge()))
//         .expect_write(create_frame(3, mnp_response()))
//         .expect_read(create_auth_switch_frame(4, "mysql_native_password", mnp_challenge()))
//         .will_set_capabilities(min_caps)  // incidental
//         .will_set_connection_id(42)       // incidental
//         .check(fix, client_errc::protocol_value_error);
// }

// In mysql_native_password, more data packets are not supported
// TODO: re-enable this test when we make the handshake more robust
// BOOST_AUTO_TEST_CASE(mnp_authswitch_moredata)
// {
//     // Setup
//     fixture fix;

//     // Run the test
//     algo_test()
//         .expect_read(
//             server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge()).build()
//         )
//         .expect_write(login_request_builder()
//                           .auth_plugin("caching_sha2_password")
//                           .auth_response(csha2p_response())
//                           .build())
//         .expect_read(create_auth_switch_frame(2, "mysql_native_password", mnp_challenge()))
//         .expect_write(create_frame(3, mnp_response()))
//         .expect_read(create_more_data_frame(4, mnp_challenge()))
//         .will_set_capabilities(min_caps)  // incidental
//         .will_set_connection_id(42)       // incidental
//         .check(fix, client_errc::protocol_value_error);
// }

// Spotcheck: mysql_native_password doesn't have interactions with TLS
BOOST_AUTO_TEST_CASE(mnp_tls)
{
    // Setup
    fixture fix;
    fix.st.tls_supported = true;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(tls_caps).auth_data(mnp_challenge()).build())
        .expect_write(ssl_request_builder().build())
        .expect_ssl_handshake()
        .expect_write(login_request_builder().seqnum(2).caps(tls_caps).auth_response(mnp_response()).build())
        .expect_read(create_ok_frame(3, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_tls_active(true)
        .will_set_capabilities(tls_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

//
// caching_sha2_password
//

static constexpr std::array<std::uint8_t, 1> csha2p_ok_follows{{0x03}};
static constexpr std::array<std::uint8_t, 1> csha2p_full_auth{{0x04}};

// Null-terminated password, as required by the plugin
boost::span<const std::uint8_t> null_terminated_password()
{
    const char* passwd = "example_password";
    return {reinterpret_cast<const std::uint8_t*>(passwd), std::strlen(passwd) + 1};
}

// Edge case: we tolerate a direct OK packet, without an OK follows
BOOST_AUTO_TEST_CASE(csha2p_ok)
{
    // Setup
    fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge()).build()
        )
        .expect_write(login_request_builder()
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response())
                          .build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

// Edge case: we tolerate a direct error packet, without an OK follows
BOOST_AUTO_TEST_CASE(csha2p_err)
{
    // Setup
    fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge()).build()
        )
        .expect_write(login_request_builder()
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response())
                          .build())
        .expect_read(err_builder()
                         .seqnum(2)
                         .code(common_server_errc::er_access_denied_error)
                         .message("Denied")
                         .build_frame())
        .will_set_capabilities(min_caps)  // incidental
        .will_set_connection_id(42)       // incidental
        .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Denied"));
}

// At the moment, this plugin requires TLS, so this is an error
BOOST_AUTO_TEST_CASE(csha2p_fullauth)
{
    // Setup
    fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder()
                         .caps(tls_caps)
                         .auth_plugin("caching_sha2_password")
                         .auth_data(csha2p_challenge())
                         .build())
        .expect_write(login_request_builder()
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response())
                          .build())
        .expect_read(create_more_data_frame(2, csha2p_full_auth))
        .will_set_capabilities(min_caps)
        .will_set_connection_id(42)
        .check(fix, client_errc::auth_plugin_requires_ssl);
}

// Usual success when using the fast track
BOOST_AUTO_TEST_CASE(csha2p_okfollows_ok)
{
    // Setup
    fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge()).build()
        )
        .expect_write(login_request_builder()
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response())
                          .build())
        .expect_read(create_more_data_frame(2, csha2p_ok_follows))
        .expect_read(create_ok_frame(3, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

// Password errors don't trigger this path (they always go through full auth),
// but other errors (like incorrect database) trigger this path
// TODO: uncomment this after https://github.com/boostorg/mysql/issues/468 gets fixed
// BOOST_AUTO_TEST_CASE(csha2p_okfollows_err)
// {
//     // Setup
//     fixture fix;

//     // Run the test
//     algo_test()
//         .expect_read(
//             server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge()).build()
//         )
//         .expect_write(login_request_builder()
//                           .auth_plugin("caching_sha2_password")
//                           .auth_response(csha2p_response())
//                           .build())
//         .expect_read(create_more_data_frame(2, csha2p_ok_follows))
//         .expect_read(err_builder()
//                          .seqnum(3)
//                          .code(common_server_errc::er_access_denied_error)
//                          .message("Denied")
//                          .build_frame())
//         .will_set_capabilities(min_caps)  // incidental
//         .will_set_connection_id(42)       // incidental
//         .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Denied"));
// }

// TODO: okfollows-more data
// TODO: okfollows-auth switch
// TODO: okfollows-full auth

// The authentication plugin raises an error during the "fast track" auth
BOOST_AUTO_TEST_CASE(csha2p_bad_challenge_length)
{
    // Setup
    fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder()
                         .auth_plugin("caching_sha2_password")
                         .auth_data(std::vector<std::uint8_t>(21, 0x01))
                         .build())
        .will_set_capabilities(min_caps)
        .will_set_connection_id(42)
        .check(fix, client_errc::protocol_value_error);
}

// Usual flow when requesting full auth
BOOST_AUTO_TEST_CASE(csha2p_tls_fullauth_ok)
{
    // Setup
    fixture fix;
    fix.st.tls_supported = true;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder()
                         .caps(tls_caps)
                         .auth_plugin("caching_sha2_password")
                         .auth_data(csha2p_challenge())
                         .build())
        .expect_write(ssl_request_builder().build())
        .expect_ssl_handshake()
        .expect_write(login_request_builder()
                          .seqnum(2)
                          .caps(tls_caps)
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response())
                          .build())
        .expect_read(create_more_data_frame(3, csha2p_full_auth))
        .expect_write(create_frame(4, null_terminated_password()))
        .expect_read(create_ok_frame(5, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(tls_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .will_set_tls_active(true)
        .check(fix);
}

// Error with full auth
BOOST_AUTO_TEST_CASE(csha2p_tls_fullauth_err)
{
    // Setup
    fixture fix;
    fix.st.tls_supported = true;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder()
                         .caps(tls_caps)
                         .auth_plugin("caching_sha2_password")
                         .auth_data(csha2p_challenge())
                         .build())
        .expect_write(ssl_request_builder().build())
        .expect_ssl_handshake()
        .expect_write(login_request_builder()
                          .seqnum(2)
                          .caps(tls_caps)
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response())
                          .build())
        .expect_read(create_more_data_frame(3, csha2p_full_auth))
        .expect_write(create_frame(4, null_terminated_password()))
        .expect_read(err_builder()
                         .seqnum(5)
                         .code(common_server_errc::er_access_denied_error)
                         .message("Denied")
                         .build_frame())
        .will_set_capabilities(tls_caps)
        .will_set_connection_id(42)
        .will_set_tls_active(true)
        .check(fix, common_server_errc::er_access_denied_error, create_server_diag("Denied"));
}

// TODO: other cases with authswitch
BOOST_AUTO_TEST_CASE(csha2p_authswitch_okfollows_ok)
{
    // Setup
    fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("mysql_native_password").auth_data(mnp_challenge()).build()
        )
        .expect_write(
            login_request_builder().auth_plugin("mysql_native_password").auth_response(mnp_response()).build()
        )
        .expect_read(create_auth_switch_frame(2, "caching_sha2_password", csha2p_challenge()))
        .expect_write(create_frame(3, csha2p_response()))
        .expect_read(create_more_data_frame(4, csha2p_ok_follows))
        .expect_read(create_ok_frame(5, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

// If we're using a secure transport (e.g. UNIX socket), caching_sha2_password
// just sends the raw password
// TODO: after we refactor handshake, maybe run more cases with secure_channel=true
BOOST_AUTO_TEST_CASE(csha2p_securetransport_fullauth_ok)
{
    // Setup
    fixture fix(handshake_params("example_user", "example_password"), true);

    // Run the test
    algo_test()
        .expect_read(server_hello_builder()
                         .caps(min_caps)
                         .auth_plugin("caching_sha2_password")
                         .auth_data(csha2p_challenge())
                         .build())
        .expect_write(login_request_builder()
                          .caps(min_caps)
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response())
                          .build())
        .expect_read(create_more_data_frame(2, csha2p_full_auth))
        .expect_write(create_frame(3, null_terminated_password()))
        .expect_read(create_ok_frame(4, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

//
// capabilities: connect with db
//

constexpr capabilities db_caps = min_caps | capabilities(detail::CLIENT_CONNECT_WITH_DB);

BOOST_AUTO_TEST_CASE(db_nonempty_supported)
{
    // Setup
    fixture fix(handshake_params("example_user", "example_password", "mydb"));

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(db_caps).auth_data(mnp_challenge()).build())
        .expect_write(login_request_builder().caps(db_caps).auth_response(mnp_response()).db("mydb").build())
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
    fixture fix(handshake_params("example_user", "example_password", "mydb"));

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(min_caps).auth_data(mnp_challenge()).build())
        .check(fix, client_errc::server_unsupported);  // TODO: some diagnostics here would be great
}

// If the user didn't request a DB, we don't send it
BOOST_AUTO_TEST_CASE(db_empty_supported)
{
    // Setup
    fixture fix(handshake_params("example_user", "example_password", ""));

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(db_caps).auth_data(mnp_challenge()).build())
        .expect_write(login_request_builder().caps(min_caps).auth_response(mnp_response()).build())
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
    fixture fix(handshake_params("example_user", "example_password", ""));

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_challenge()).build())
        .expect_write(login_request_builder().auth_response(mnp_response()).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

//
// capabilities: multi_queries
//

constexpr capabilities multiq_caps = min_caps | capabilities(detail::CLIENT_MULTI_STATEMENTS);

// We request it and the server supports it
BOOST_AUTO_TEST_CASE(multiq_true_supported)
{
    // Setup
    handshake_params hparams("example_user", "example_password");
    hparams.set_multi_queries(true);
    fixture fix(hparams);

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(multiq_caps).auth_data(mnp_challenge()).build())
        .expect_write(login_request_builder().caps(multiq_caps).auth_response(mnp_response()).build())
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
    fixture fix(hparams);

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(min_caps).auth_data(mnp_challenge()).build())
        .check(fix, client_errc::server_unsupported);  // TODO: some diagnostics here would be great
}

// We don't request it but the server supports it. We request the server to disable it
BOOST_AUTO_TEST_CASE(multiq_false_supported)
{
    // Setup
    fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(multiq_caps).auth_data(mnp_challenge()).build())
        .expect_write(login_request_builder().caps(min_caps).auth_response(mnp_response()).build())
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
    fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(min_caps).auth_data(mnp_challenge()).build())
        .expect_write(login_request_builder().caps(min_caps).auth_response(mnp_response()).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

//
// capabilities: TLS
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
            fixture fix(hparams, false);  // TLS only negotiated when the transport is not secure
            fix.st.tls_supported = true;  // TLS only negotiated if supported

            // Run the test
            algo_test()
                .expect_read(server_hello_builder().caps(tls_caps).auth_data(mnp_challenge()).build())
                .expect_write(ssl_request_builder().build())
                .expect_ssl_handshake()
                .expect_write(
                    login_request_builder().seqnum(2).caps(tls_caps).auth_response(mnp_response()).build()
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
            fixture fix(hparams, false);
            fix.st.tls_supported = tc.transport_supports_tls;

            // Run the test
            algo_test()
                .expect_read(server_hello_builder().caps(tc.server_caps).auth_data(mnp_challenge()).build())
                .expect_write(login_request_builder().caps(min_caps).auth_response(mnp_response()).build())
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
    fixture fix(hparams, false);  // This doesn't happen if the transport is already secure
    fix.st.tls_supported = true;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().caps(min_caps).auth_data(mnp_challenge()).build())
        .check(fix, client_errc::server_doesnt_support_ssl);
}

//
// Base capabilities
//
// TODO: having the capabilities in all uppercase likely conflicts with official headers

// If the server doesn't have these, we can't talk to it
BOOST_AUTO_TEST_CASE(caps_mandatory)
{
    constexpr struct
    {
        const char* name;
        capabilities caps;
    } test_cases[] = {
        {"no_protocol_41",             capabilities(min_caps.get() & ~detail::CLIENT_PROTOCOL_41)      },
        {"no_plugin_auth",             capabilities(min_caps.get() & ~detail::CLIENT_PLUGIN_AUTH)      },
        {"no_plugin_auth_lenenc_data",
         capabilities(min_caps.get() & ~detail::CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA)                 },
        {"no_deprecate_eof",           capabilities(min_caps.get() & ~detail::CLIENT_DEPRECATE_EOF)    },
        {"no_secure_connection",       capabilities(min_caps.get() & ~detail::CLIENT_SECURE_CONNECTION)},
        {"several_missing",
         capabilities(detail::CLIENT_PLUGIN_AUTH | detail::CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA)      },
        {"none",                       capabilities()                                                  },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            fixture fix;

            // Run the test
            algo_test()
                .expect_read(server_hello_builder().caps(tc.caps).auth_data(mnp_challenge()).build())
                .check(fix, client_errc::server_unsupported);  // TODO: some diagnostics here would be great
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
        {"multi_results",    capabilities(detail::CLIENT_MULTI_RESULTS)   },
        {"ps_multi_results", capabilities(detail::CLIENT_PS_MULTI_RESULTS)},
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            fixture fix;

            // Run the test
            algo_test()
                .expect_read(
                    server_hello_builder().caps(min_caps | tc.caps).auth_data(mnp_challenge()).build()
                )
                .expect_write(
                    login_request_builder().caps(min_caps | tc.caps).auth_response(mnp_response()).build()
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
        {"long_password",                capabilities(detail::CLIENT_LONG_PASSWORD)               },
        {"found_rows",                   capabilities(detail::CLIENT_FOUND_ROWS)                  },
        {"long_flag",                    capabilities(detail::CLIENT_LONG_FLAG)                   },
        {"no_schema",                    capabilities(detail::CLIENT_NO_SCHEMA)                   },
        {"compress",                     capabilities(detail::CLIENT_COMPRESS)                    },
        {"odbc",                         capabilities(detail::CLIENT_ODBC)                        },
        {"local_files",                  capabilities(detail::CLIENT_LOCAL_FILES)                 },
        {"ignore_space",                 capabilities(detail::CLIENT_IGNORE_SPACE)                },
        {"interactive",                  capabilities(detail::CLIENT_INTERACTIVE)                 },
        {"ignore_sigpipe",               capabilities(detail::CLIENT_IGNORE_SIGPIPE)              },
        {"transactions",                 capabilities(detail::CLIENT_TRANSACTIONS)                },
        {"reserved",                     capabilities(detail::CLIENT_RESERVED)                    },
        {"connect_attrs",                capabilities(detail::CLIENT_CONNECT_ATTRS)               },
        {"can_handle_expired_passwords", capabilities(detail::CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS)},
        {"session_track",                capabilities(detail::CLIENT_SESSION_TRACK)               },
        {"ssl_verify_server_cert",       capabilities(detail::CLIENT_SSL_VERIFY_SERVER_CERT)      },
        {"optional_resultset_metadata",  capabilities(detail::CLIENT_OPTIONAL_RESULTSET_METADATA) },
        {"remember_options",             capabilities(detail::CLIENT_REMEMBER_OPTIONS)            },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            fixture fix;

            // Run the test
            algo_test()
                .expect_read(
                    server_hello_builder().caps(min_caps | tc.caps).auth_data(mnp_challenge()).build()
                )
                .expect_write(login_request_builder().caps(min_caps).auth_response(mnp_response()).build())
                .expect_read(create_ok_frame(2, ok_builder().build()))
                .will_set_status(connection_status::ready)
                .will_set_capabilities(min_caps)
                .will_set_current_charset(utf8mb4_charset)
                .will_set_connection_id(42)
                .check(fix);
        }
    }
}

//
// Generic auth plugin errors
//

BOOST_AUTO_TEST_CASE(hello_unknown_plugin)
{
    // Setup
    fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_plugin("unknown").auth_data(csha2p_challenge()).build())
        .will_set_capabilities(min_caps)
        .will_set_connection_id(42)
        .check(fix, client_errc::unknown_auth_plugin);
}

BOOST_AUTO_TEST_CASE(authswitch_unknown_plugin)
{
    // Setup
    fixture fix;

    // Run the test
    algo_test()
        .expect_read(
            server_hello_builder().auth_plugin("caching_sha2_password").auth_data(csha2p_challenge()).build()
        )
        .expect_write(login_request_builder()
                          .auth_plugin("caching_sha2_password")
                          .auth_response(csha2p_response())
                          .build())
        .expect_read(create_auth_switch_frame(2, "unknown", mnp_challenge()))
        .will_set_capabilities(min_caps)
        .will_set_connection_id(42)
        .check(fix, client_errc::unknown_auth_plugin);
}

// TODO: auth switch to itself (better after we refactor the handshake)
// TODO: auth switch more than once (better after we refactor the handshake)

//
// Data in the initial hello
//

// No value of connection_id causes trouble
BOOST_AUTO_TEST_CASE(hello_connection_id)
{
    for (std::uint32_t value : {0u, 10u, 0xffffffffu})
    {
        BOOST_TEST_CONTEXT(value)
        {
            // Setup
            fixture fix;

            // Run the test
            algo_test()
                .expect_read(server_hello_builder().connection_id(value).auth_data(mnp_challenge()).build())
                .expect_write(login_request_builder().auth_response(mnp_response()).build())
                .expect_read(create_ok_frame(2, ok_builder().build()))
                .will_set_status(connection_status::ready)
                .will_set_capabilities(min_caps)
                .will_set_current_charset(utf8mb4_charset)
                .will_set_connection_id(value)
                .check(fix);
        }
    }
}

// Flavor is set, regardless of what we had before
BOOST_AUTO_TEST_CASE(flavor)
{
    struct
    {
        string_view version;
        detail::db_flavor flavor;
    } test_cases[] = {
        {"11.4.2-MariaDB-ubu2404",             detail::db_flavor::mariadb},
        {"8.4.1 MySQL Community Server - GPL", detail::db_flavor::mysql  },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.flavor)
        {
            // Setup
            fixture fix;
            fix.st.flavor = static_cast<detail::db_flavor>(0xffff);  // make sure that we set the value

            // Run the test
            algo_test()
                .expect_read(server_hello_builder().version(tc.version).auth_data(mnp_challenge()).build())
                .expect_write(login_request_builder().auth_response(mnp_response()).build())
                .expect_read(create_ok_frame(2, ok_builder().build()))
                .will_set_status(connection_status::ready)
                .will_set_capabilities(min_caps)
                .will_set_current_charset(utf8mb4_charset)
                .will_set_connection_id(42)
                .will_set_flavor(tc.flavor)
                .check(fix);
        }
    }
}

//
// Collations
//
// TODO: the known ones require an integration test
BOOST_AUTO_TEST_CASE(collations)
{
    constexpr struct
    {
        const char* name;
        std::uint16_t collation_id;
        character_set charset;
    } test_cases[] = {
        {"utf8mb4_bin",        mysql_collations::utf8mb4_bin,        utf8mb4_charset},
        {"utf8mb4_general_ci", mysql_collations::utf8mb4_general_ci, utf8mb4_charset},
        {"ascii_general_ci",   mysql_collations::ascii_general_ci,   ascii_charset  },
        {"ascii_bin",          mysql_collations::ascii_bin,          ascii_charset  },
        {"latin1_general_ci",  mysql_collations::latin1_general_ci,  {}             },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            handshake_params hparams("example_user", "example_password");
            hparams.set_connection_collation(tc.collation_id);
            fixture fix(hparams);
            fix.st.current_charset = {"other", detail::next_char_utf8mb4};  // make sure that we set the value

            // Run the test
            algo_test()
                .expect_read(server_hello_builder().auth_data(mnp_challenge()).build())
                .expect_write(
                    login_request_builder().collation(tc.collation_id).auth_response(mnp_response()).build()
                )
                .expect_read(create_ok_frame(2, ok_builder().build()))
                .will_set_status(connection_status::ready)
                .will_set_capabilities(min_caps)
                .will_set_current_charset(tc.charset)
                .will_set_connection_id(42)
                .check(fix);
        }
    }
}

// The value of backslash_escapes in the final OK packet doesn't get ignored
BOOST_AUTO_TEST_CASE(backslash_escapes)
{
    // Setup
    fixture fix;
    fix.st.backslash_escapes = true;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_challenge()).build())
        .expect_write(login_request_builder().auth_response(mnp_response()).build())
        .expect_read(create_ok_frame(2, ok_builder().no_backslash_escapes(true).build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_backslash_escapes(false)
        .will_set_connection_id(42)
        .check(fix);
}

// Handshake should not modify the value of metadata mode
BOOST_AUTO_TEST_CASE(meta_mode)
{
    // Setup
    fixture fix;
    fix.st.meta_mode = metadata_mode::full;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_challenge()).build())
        .expect_write(login_request_builder().auth_response(mnp_response()).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_status(connection_status::ready)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

//
// Connection status
//

// On success, set to ready, regardless of the initial value
BOOST_AUTO_TEST_CASE(connection_status_success)
{
    constexpr connection_status all_status[] = {
        connection_status::not_connected,
        connection_status::ready,
        connection_status::engaged_in_multi_function
    };

    for (auto initial_status : all_status)
    {
        BOOST_TEST_CONTEXT(initial_status)
        {
            // Setup
            fixture fix;
            fix.st.status = initial_status;

            // Run the test
            algo_test()
                .expect_read(server_hello_builder().auth_data(mnp_challenge()).build())
                .expect_write(login_request_builder().auth_response(mnp_response()).build())
                .expect_read(create_ok_frame(2, ok_builder().build()))
                .will_set_status(connection_status::ready)
                .will_set_capabilities(min_caps)
                .will_set_current_charset(utf8mb4_charset)
                .will_set_connection_id(42)
                .check(fix);
        }
    }
}

// On success, set to not connected, regardless of the initial value
BOOST_AUTO_TEST_CASE(connection_status_error)
{
    constexpr connection_status all_status[] = {
        connection_status::not_connected,
        connection_status::ready,
        connection_status::engaged_in_multi_function
    };

    for (auto initial_status : all_status)
    {
        BOOST_TEST_CONTEXT(initial_status)
        {
            // Setup
            fixture fix;
            fix.st.status = initial_status;

            // Run the test
            algo_test()
                .expect_read(client_errc::sequence_number_mismatch)
                .will_set_status(connection_status::not_connected)
                .check(fix, client_errc::sequence_number_mismatch);
        }
    }
}

//
// Deserialization errors
//

BOOST_AUTO_TEST_CASE(deserialization_error_hello)
{
    // Setup
    fixture fix;

    // Run the test
    algo_test()
        .expect_read(create_frame(0, boost::span<const std::uint8_t>()))
        .check(fix, client_errc::incomplete_message);
}

BOOST_AUTO_TEST_CASE(deserialization_error_handshake_server_response)
{
    // Setup
    fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_challenge()).build())
        .expect_write(login_request_builder().auth_response(mnp_response()).build())
        .expect_read(create_frame(2, boost::span<const std::uint8_t>()))
        .will_set_capabilities(min_caps)
        .will_set_connection_id(42)
        .check(fix, client_errc::incomplete_message);
}

/**
other stuff
    SSL handshake
    Error deserializing hello/contains an error packet (e.g. too many connections)
    Error deserializing auth switch
Network errors
    Auth with more data
    Auth with auth switch
    Using SSL
*/

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

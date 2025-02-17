//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/string_view.hpp>

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
#include <cassert>
#include <cstdint>
#include <vector>

#include "test_common/create_diagnostics.hpp"
#include "test_unit/algo_test.hpp"
#include "test_unit/create_err.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"
#include "test_unit/serialize_to_vector.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
using detail::capabilities;

namespace {

BOOST_AUTO_TEST_SUITE(test_handshake)

// Capabilities
constexpr capabilities min_caps{
    detail::CLIENT_PLUGIN_AUTH | detail::CLIENT_PROTOCOL_41 | detail::CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA |
    detail::CLIENT_DEPRECATE_EOF | detail::CLIENT_SECURE_CONNECTION
};

// Helpers to create the relevant packets
class server_hello_builder
{
    string_view server_version_{"8.1.33"};
    std::vector<std::uint8_t> auth_plugin_data_;
    detail::capabilities server_caps_{min_caps};
    string_view auth_plugin_name_{"mysql_native_password"};

public:
    server_hello_builder& version(string_view v) noexcept
    {
        server_version_ = v;
        return *this;
    }
    server_hello_builder& auth_data(std::vector<std::uint8_t> v)
    {
        BOOST_ASSERT(v.size() <= 0xfeu);
        auth_plugin_data_ = std::move(v);
        return *this;
    }
    server_hello_builder& caps(detail::capabilities v) noexcept
    {
        server_caps_ = v;
        return *this;
    }
    server_hello_builder& auth_plugin(string_view v) noexcept
    {
        auth_plugin_name_ = v;
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
                    int4{42},       // connection_id
                    plugin_data_1,  // plugin data, 1st part
                    int1{0},        // filler
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
    capabilities caps_{min_caps};
    std::uint32_t collation_id_{45};  // utf8_general_ci
    string_view username_{"example_user"};
    std::vector<std::uint8_t> auth_response_;
    string_view database_{};
    string_view auth_plugin_name_{"mysql_native_password"};

public:
    login_request_builder() = default;

    login_request_builder& caps(detail::capabilities v) noexcept
    {
        caps_ = v;
        return *this;
    }

    login_request_builder& collation(std::uint32_t v) noexcept
    {
        collation_id_ = v;
        return *this;
    }

    login_request_builder& username(string_view v) noexcept
    {
        username_ = v;
        return *this;
    }

    login_request_builder& auth_response(std::vector<std::uint8_t> v)
    {
        auth_response_ = std::move(v);
        return *this;
    }

    login_request_builder& db(string_view v) noexcept
    {
        database_ = v;
        return *this;
    }

    login_request_builder& auth_plugin(string_view v) noexcept
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

struct fixture : algo_fixture_base
{
    detail::handshake_algo algo{
        {handshake_params("example_user", "example_password"), false}
    };
};

/**
csha2p
    fast track success
        hello, login request, more data ok follows, ok
        hello, login request, auth switch, auth switch response, more data ok follows, ok
    fast track non-password error: (password error causes full auth)
        hello, login request, more data ok follows, error => we have a bug
        hello, login request, auth switch with scram,  auth switch response, more data ok follows, error
    request to perform full auth, success
        hello, login request, more data perform full auth, password, ok
        hello, login request, auth switch with scram, auth switch response, more data perform full auth,
            password, ok
        (theoretical) hello, login request, auth switch perform full auth, password, ok
    request to perform full auth, password/non-password error
        hello, login request, more data perform full auth, error
        hello, login request, auth switch with scram, auth switch response, more data perform full auth, error
    request to perform full auth, no ssl
    bad challenge length in any of them
    more data but it's not full auth or wait for ok
Capabilities
    With database
    Without database
    With all possible collation values
    With an unknown collation
    All combinations of ssl_mode, server supports ssl, transport supports ssl/transport is secure
    Server doesn't support deprecate eof/other mandatory capabilities
    Server doesn't support multi queries, requested/not requested
    Server doesn't support connect with DB, requested/not requested
other stuff
    Auth switch with unknown default plugin: same as above
    Auth switch to unknown default plugin: hello, response, auth switch, error
    Auth switch to itself
    auth switch more than once
    The correct secure_channel value is passed to the plugin?
    SSL handshake
    Error deserializing hello/contains an error packet (e.g. too many connections)
    Error deserializing auth switch
    Error: capabilities are insufficient
    The correct DB flavor is set
    backslash escapes
    flags in hello
    connection_id
    everything is correctly reset
Network errors
    Auth with more data
    Auth with auth switch
    Using SSL
*/

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

// mysql_native_password
//     bad challenge length in any of them
//     more data

// std::vector<std::uint8_t> challenge2()
// {
//     return {0x36, 0x2c, 0x3f, 0x49, 0x1e, 0x51, 0x13, 0x79, 0x4c, 0x0b,
//             0x0e, 0x06, 0x08, 0x40, 0x04, 0x0b, 0x2c, 0x53, 0x1e, 0x36};
// }

// std::vector<std::uint8_t> response2()
// {
//     return {0x21, 0x34, 0xca, 0xa6, 0x49, 0x91, 0x77, 0x63, 0x72, 0x7a,
//             0xa6, 0xc9, 0x9b, 0x58, 0x3c, 0x9e, 0x89, 0x94, 0x34, 0x41};
// }

BOOST_AUTO_TEST_CASE(mnp_fast_track_success)
{
    // Setup
    fixture fix;

    // Run the test
    algo_test()
        .expect_read(server_hello_builder().auth_data(mnp_challenge()).build())
        .expect_write(login_request_builder().auth_response(mnp_response()).build())
        .expect_read(create_ok_frame(2, ok_builder().build()))
        .will_set_is_connected(true)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

BOOST_AUTO_TEST_CASE(mnp_fast_track_auth_error)
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

BOOST_AUTO_TEST_CASE(mnp_auth_switch_success)
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
        .will_set_is_connected(true)
        .will_set_capabilities(min_caps)
        .will_set_current_charset(utf8mb4_charset)
        .will_set_connection_id(42)
        .check(fix);
}

BOOST_AUTO_TEST_CASE(mnp_auth_switch_auth_error)
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

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

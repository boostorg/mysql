//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_TEST_SANSIO_HANDSHAKE_HANDSHAKE_COMMON_HPP
#define BOOST_MYSQL_TEST_UNIT_TEST_SANSIO_HANDSHAKE_HANDSHAKE_COMMON_HPP

#include <boost/mysql/string_view.hpp>

#include <boost/mysql/impl/internal/protocol/capabilities.hpp>
#include <boost/mysql/impl/internal/protocol/impl/protocol_types.hpp>
#include <boost/mysql/impl/internal/sansio/handshake.hpp>

#include <algorithm>

#include "test_unit/algo_test.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/serialize_to_vector.hpp"

// Functions and constants common to all handshake tests

namespace boost {
namespace mysql {
namespace test {

// Capabilities
constexpr auto min_caps = detail::capabilities::plugin_auth | detail::capabilities::protocol_41 |
                          detail::capabilities::plugin_auth_lenenc_data |
                          detail::capabilities::deprecate_eof | detail::capabilities::secure_connection;

constexpr auto tls_caps = min_caps | detail::capabilities::ssl;

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
    server_hello_builder& auth_data(span<const std::uint8_t> v)
    {
        BOOST_ASSERT(v.size() >= 8u);  // split in two parts, and the 1st one is fixed size at 8 bytes
        BOOST_ASSERT(v.size() <= 0xfeu);
        auth_plugin_data_.assign(v.begin(), v.end());
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
                std::uint32_t caps_little = boost::endian::native_to_little(
                    static_cast<std::uint32_t>(server_caps_)
                );
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
    detail::capabilities caps_{min_caps};
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

    login_request_builder& auth_response(span<const std::uint8_t> v)
    {
        auth_response_.assign(v.begin(), v.end());
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

inline std::vector<std::uint8_t> create_ssl_request()
{
    const auto body = serialize_to_vector([](detail::serialization_context& ctx) {
        constexpr std::uint32_t collation_id = 45;  // utf8_general_ci
        detail::ssl_request req{tls_caps, detail::max_packet_size, collation_id};
        req.serialize(ctx);
    });
    return create_frame(1, body);
}

inline std::vector<std::uint8_t> create_auth_switch_frame(
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

inline std::vector<std::uint8_t> create_more_data_frame(
    std::uint8_t seqnum,
    boost::span<const std::uint8_t> data
)
{
    return create_frame(seqnum, serialize_to_vector([=](detail::serialization_context& ctx) {
                            ctx.add(0x01);  // more data header
                            ctx.add(data);
                        }));
}

// These scrambles/hashes have been captured with Wireshark.
constexpr const char* password = "example_password";

constexpr std::uint8_t mnp_scramble[] = {
    0x1b, 0x0f, 0x6e, 0x59, 0x1b, 0x70, 0x33, 0x01, 0x0c, 0x01,
    0x7e, 0x2e, 0x30, 0x7a, 0x79, 0x5c, 0x02, 0x50, 0x51, 0x35,
};

constexpr std::uint8_t mnp_hash[] = {
    0xbe, 0xa5, 0xb5, 0xe7, 0x9c, 0x05, 0x23, 0x34, 0xda, 0x06,
    0x1d, 0xaf, 0xd9, 0x8b, 0x4b, 0x09, 0x86, 0xe5, 0xd1, 0x4a,
};

constexpr std::uint8_t csha2p_scramble[] = {
    0x6f, 0x1b, 0x3b, 0x64, 0x39, 0x01, 0x46, 0x44, 0x53, 0x3b,
    0x74, 0x3c, 0x3e, 0x3c, 0x3c, 0x0b, 0x30, 0x77, 0x1a, 0x49,
};

constexpr std::uint8_t csha2p_hash[] = {
    0xa7, 0xc3, 0x7f, 0x88, 0x25, 0xec, 0x92, 0x2c, 0x88, 0xba, 0x47, 0x04, 0x14, 0xd2, 0xa3, 0xa3,
    0x5e, 0xa9, 0x41, 0x8e, 0xdc, 0x89, 0xeb, 0xe2, 0xa1, 0xec, 0xd8, 0x4f, 0x73, 0xa1, 0x49, 0x60,
};

constexpr std::uint8_t csha2p_request_key[] = {0x02};
constexpr std::uint8_t csha2p_fast_auth_ok[] = {0x03};
constexpr std::uint8_t csha2p_perform_full_auth[] = {0x04};

// Null-terminated password, as required by the plugin
inline boost::span<const std::uint8_t> null_terminated_password()
{
    return {reinterpret_cast<const std::uint8_t*>(password), std::strlen(password) + 1};
}

struct handshake_fixture : algo_fixture_base
{
    detail::handshake_algo algo;

    handshake_fixture(
        const handshake_params& hparams = handshake_params("example_user", password),
        bool secure_transport = false
    )
        : algo({hparams, secure_transport})
    {
        st.status = detail::connection_status::not_connected;
    }

    handshake_fixture(bool secure_transport)
        : handshake_fixture(handshake_params("example_user", password), secure_transport)
    {
    }
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif

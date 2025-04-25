//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CACHING_SHA2_PASSWORD_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CACHING_SHA2_PASSWORD_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/protocol/impl/protocol_types.hpp>
#include <boost/mysql/impl/internal/protocol/impl/serialization_context.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

#include <boost/core/span.hpp>
#include <boost/system/result.hpp>

#include <array>
#include <cstdint>
#include <openssl/sha.h>

// Reference:
// https://dev.mysql.com/doc/dev/mysql-server/latest/page_caching_sha2_authentication_exchanges.html

namespace boost {
namespace mysql {
namespace detail {

// Algorithm itself
BOOST_INLINE_CONSTEXPR std::size_t csha2p_challenge_length = 20;
BOOST_INLINE_CONSTEXPR std::size_t csha2p_response_length = 32;

inline std::array<std::uint8_t, csha2p_response_length> csha2p_compute_auth_string(
    string_view password,
    boost::span<const std::uint8_t, csha2p_challenge_length> challenge
)
{
    static_assert(csha2p_response_length == SHA256_DIGEST_LENGTH, "Buffer size mismatch");

    // SHA(SHA(password_sha) concat challenge) XOR password_sha
    // hash1 = SHA(pass)
    std::array<std::uint8_t, csha2p_response_length> password_sha;
    SHA256(reinterpret_cast<const unsigned char*>(password.data()), password.size(), password_sha.data());

    // SHA(password_sha) concat challenge = buffer
    std::array<std::uint8_t, csha2p_response_length + csha2p_challenge_length> buffer;
    SHA256(password_sha.data(), password_sha.size(), buffer.data());
    std::memcpy(buffer.data() + csha2p_response_length, challenge.data(), csha2p_challenge_length);

    // SHA(SHA(password_sha) concat challenge) = SHA(buffer) = salted_password
    std::array<std::uint8_t, csha2p_response_length> salted_password;
    SHA256(buffer.data(), buffer.size(), salted_password.data());

    // salted_password XOR password_sha
    std::array<std::uint8_t, csha2p_response_length> res;
    for (unsigned i = 0; i < csha2p_response_length; ++i)
    {
        res[i] = salted_password[i] ^ password_sha[i];
    }

    return res;
}

class caching_sha2_password_algo
{
    int resume_point_{0};
    std::array<std::uint8_t, csha2p_response_length> hashed_password_{};

    static bool is_perform_full_auth(boost::span<const std::uint8_t> server_data)
    {
        return server_data.size() == 1u && server_data[0] == 4;
    }

    static bool is_fast_auth_ok(boost::span<const std::uint8_t> server_data)
    {
        return server_data.size() == 1u && server_data[0] == 3;
    }

public:
    caching_sha2_password_algo() = default;

    system::result<span<const std::uint8_t>> hash_password(
        string_view password,
        span<const std::uint8_t> challenge
    )
    {
        // If the challenge doesn't match the expected size,
        // something wrong is going on and we should fail
        if (challenge.size() != csha2p_challenge_length)
            return client_errc::protocol_value_error;

        // Empty passwords are not hashed
        if (password.empty())
            return span<const std::uint8_t>();

        // Run the algorithm
        hashed_password_ = csha2p_compute_auth_string(
            password,
            span<const std::uint8_t, csha2p_challenge_length>(challenge)
        );
        return hashed_password_;
    }

    // Can be:
    //   write the packet in the write buffer
    //   read a packet and call me again
    //   an error
    // TODO: this should use a type != to next_action
    next_action resume(
        connection_state_data& st,
        boost::span<const std::uint8_t> server_data,
        string_view password,
        bool secure_channel,
        std::uint8_t& seqnum
    )
    {
        switch (resume_point_)
        {
        case 0:
            // If we got a more data packet, the server either required us to perform full auth,
            // or told us to read again because an OK packet or error packet is coming.
            if (is_perform_full_auth(server_data))
            {
                // At this point, we don't support full auth over insecure channels
                if (!secure_channel)
                {
                    return make_error_code(client_errc::auth_plugin_requires_ssl);
                }

                // We should send a packet with just the password, as a NULL-terminated string
                BOOST_MYSQL_YIELD(resume_point_, 1, st.write(string_null{password}, seqnum))
            }
            else if (is_fast_auth_ok(server_data))
            {
                // We should wait for the server to send an OK or an error
                BOOST_MYSQL_YIELD(resume_point_, 2, st.read(seqnum))
            }
            else
            {
                // The server sent a data packet and we don't know what it means.
                // Treat it as a protocol violation error and exit
                return error_code(client_errc::protocol_value_error);
            }
        }

        // If we got here, the server sent us more data, which is a protocol violation
        // TODO: make an error code for this
        return error_code(client_errc::protocol_value_error);
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif

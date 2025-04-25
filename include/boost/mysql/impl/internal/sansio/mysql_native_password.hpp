//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_MYSQL_NATIVE_PASSWORD_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_MYSQL_NATIVE_PASSWORD_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/next_action.hpp>

#include <boost/core/span.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <openssl/sha.h>

namespace boost {
namespace mysql {
namespace detail {

BOOST_INLINE_CONSTEXPR std::size_t mnp_challenge_length = 20;
BOOST_INLINE_CONSTEXPR std::size_t mnp_response_length = 20;

// SHA1( password ) XOR SHA1( "20-bytes random data from server" <concat> SHA1( SHA1( password ) ) )
inline std::array<std::uint8_t, mnp_response_length> mnp_compute_auth_string(
    string_view password,
    boost::span<const std::uint8_t, mnp_challenge_length> challenge
)
{
    static_assert(mnp_response_length == SHA_DIGEST_LENGTH, "Buffer size mismatch");

    // SHA1 (password)
    std::array<std::uint8_t, SHA_DIGEST_LENGTH> password_sha1;
    SHA1(reinterpret_cast<const unsigned char*>(password.data()), password.size(), password_sha1.data());

    // Add server challenge (salt)
    std::array<std::uint8_t, mnp_challenge_length + SHA_DIGEST_LENGTH> salted_buffer;
    std::memcpy(salted_buffer.data(), challenge.data(), challenge.size());
    SHA1(password_sha1.data(), password_sha1.size(), salted_buffer.data() + mnp_challenge_length);
    std::array<std::uint8_t, SHA_DIGEST_LENGTH> salted_sha1;
    SHA1(salted_buffer.data(), salted_buffer.size(), salted_sha1.data());

    // XOR
    std::array<std::uint8_t, mnp_response_length> output;
    for (std::size_t i = 0; i < SHA_DIGEST_LENGTH; ++i)
    {
        output[i] = password_sha1[i] ^ salted_sha1[i];
    }
    return output;
}

class mysql_native_password_algo
{
    std::array<std::uint8_t, mnp_response_length> hashed_password_{};

public:
    mysql_native_password_algo() = default;

    system::result<span<const std::uint8_t>> hash_password(
        string_view password,
        span<const std::uint8_t> challenge
    )
    {
        // If the challenge doesn't match the expected size,
        // something wrong is going on and we should fail
        if (challenge.size() != mnp_challenge_length)
            return client_errc::protocol_value_error;

        // Empty passwords are not hashed
        if (password.empty())
            return span<const std::uint8_t>();

        // Run the algorithm
        hashed_password_ = mnp_compute_auth_string(
            password,
            span<const std::uint8_t, mnp_challenge_length>(challenge)
        );
        return hashed_password_;
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif

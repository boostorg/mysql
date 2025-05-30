//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CSHA2P_ENCRYPT_PASSWORD_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CSHA2P_ENCRYPT_PASSWORD_HPP

// Having this in a separate file allows us to mock the OpenSSL API in the tests

#include <boost/mysql/error_code.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/impl/internal/sansio/auth_plugin_common.hpp>

#include <boost/container/small_vector.hpp>
#include <boost/core/span.hpp>

#include <cstdint>
#include <memory>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/types.h>

namespace boost {
namespace mysql {
namespace detail {

inline unsigned long csha2p_encrypt_password(
    string_view password,
    span<const std::uint8_t, scramble_size> scramble,
    span<const std::uint8_t> server_key,
    container::small_vector<std::uint8_t, 512>& output
)
{
    // RAII helpers
    struct bio_deleter
    {
        void operator()(BIO* bio) const noexcept { BIO_free(bio); }
    };
    using unique_bio = std::unique_ptr<BIO, bio_deleter>;

    struct evp_pkey_deleter
    {
        void operator()(EVP_PKEY* pkey) const noexcept { EVP_PKEY_free(pkey); }
    };
    using unique_evp_pkey = std::unique_ptr<EVP_PKEY, evp_pkey_deleter>;

    struct evp_pkey_ctx_deleter
    {
        void operator()(EVP_PKEY_CTX* ctx) const noexcept { EVP_PKEY_CTX_free(ctx); }
    };
    using unique_evp_pkey_ctx = std::unique_ptr<EVP_PKEY_CTX, evp_pkey_ctx_deleter>;

    // Try to parse the private key. TODO: size check here
    unique_bio bio{BIO_new_mem_buf(server_key.data(), server_key.size())};
    if (!bio)
        return ERR_get_error();
    unique_evp_pkey key(PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr));
    if (!key)
        return ERR_get_error();

    // Salt the password, as a NULL-terminated string
    container::small_vector<std::uint8_t, 512> salted_password(password.size() + 1u, 0);
    for (std::size_t i = 0; i < password.size(); ++i)
        salted_password[i] = password[i] ^ scramble[i % scramble.size()];

    // Add the NULL terminator. It should be salted, too. Since 0 ^ U = U,
    // the byte should be the scramble at the position we're in
    salted_password[password.size()] = scramble[password.size() % scramble.size()];

    // Set up the encryption context
    unique_evp_pkey_ctx ctx(EVP_PKEY_CTX_new(key.get(), nullptr));
    if (!ctx)
        return ERR_get_error();
    if (EVP_PKEY_encrypt_init(ctx.get()) <= 0)
        return ERR_get_error();
    if (EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_OAEP_PADDING) <= 0)
        return ERR_get_error();

    // Encrypt
    int max_size = EVP_PKEY_get_size(key.get());
    BOOST_ASSERT(max_size >= 0);
    output.resize(max_size);
    std::size_t actual_size = static_cast<std::size_t>(max_size);
    if (EVP_PKEY_encrypt(
            ctx.get(),
            output.data(),
            &actual_size,
            salted_password.data(),
            salted_password.size()
        ) <= 0)
    {
        return ERR_get_error();
    }
    output.resize(actual_size);

    // Done
    return 0u;
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif

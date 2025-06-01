//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/error_code.hpp"
#define BOOST_TEST_MODULE test_csha2p_encrypt_password_errors

#include <boost/mysql/impl/internal/sansio/csha2p_encrypt_password.hpp>

#include <boost/container/small_vector.hpp>
#include <boost/system/error_category.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>

#include <cstdint>
#include <string>

using namespace boost::mysql;
using detail::csha2p_encrypt_password;

// Contains tests that need mocking OpenSSL functions.
// We do this at link time, by defining the functions declared in OpenSSL headers here
// and not linking to libssl/libcrypto

namespace {

// If we use asio::error::ssl_category, many more other OpenSSL functions
// become used, and mocking becomes problematic.
class mock_ssl_category final : public boost::system::error_category
{
public:
    const char* name() const noexcept override { return "mock_ssl"; }
    std::string message(int) const override { return {}; }
} ssl_category;

constexpr std::uint8_t scramble[20]{};

using vector_type = boost::container::small_vector<std::uint8_t, 512>;

struct
{
    // Number of times that each function has been called.
    // Tracking this helps us to check that we're actually covering the case we want
    std::size_t BIO_new_mem_buf_calls{};
    std::size_t PEM_read_bio_PUBKEY_calls{};
    std::size_t EVP_PKEY_CTX_new_calls{};
    std::size_t EVP_PKEY_encrypt_init_calls{};
    std::size_t EVP_PKEY_CTX_set_rsa_padding_calls{};
    std::size_t EVP_PKEY_get_size_calls{};
    std::size_t EVP_PKEY_encrypt_calls{};

    BIO* bio{reinterpret_cast<BIO*>(static_cast<std::uintptr_t>(100))};
    EVP_PKEY* key{reinterpret_cast<EVP_PKEY*>(static_cast<std::uintptr_t>(200))};
    EVP_PKEY_CTX* ctx{reinterpret_cast<EVP_PKEY_CTX*>(static_cast<std::uintptr_t>(300))};
    int encrypt_init_result{0};
    unsigned long last_error{0};

} openssl_mock;

BOOST_AUTO_TEST_CASE(error_creating_bio)
{
    // Setup
    openssl_mock = {};
    openssl_mock.bio = nullptr;
    openssl_mock.last_error = 42u;
    vector_type out;

    // Call the function
    auto ec = csha2p_encrypt_password("passwd", scramble, {}, out, ssl_category);

    // Check
    BOOST_TEST(ec == error_code(42, ssl_category));
    BOOST_TEST(ec.has_location());
    BOOST_TEST(openssl_mock.BIO_new_mem_buf_calls == 1u);
    BOOST_TEST(openssl_mock.PEM_read_bio_PUBKEY_calls == 0u);
}

// // Determining the maximum size of the ciphertext fails
// BOOST_AUTO_TEST_CASE(error_get_size)
// {
//     openssl_mock.last_error = 43u;
//     vector_type out;
//     auto ec = csha2p_encrypt_password("passwd", scramble, {}, out, ssl_category);
//     BOOST_TEST(ec == error_code(42, ssl_category));
// }

/**
error loading key
    TODO: should we fuzz this function?
    EVP_PKEY_CTX_set_rsa_padding fails with a value != -2 (mock)
    TODO: if openssl returns 0 in ERR_get_error, does the error code count as failed, too? no it does not
determining the size of the hash
    failure (EVP_PKEY_get_size < 0: mock)
    not available (EVP_PKEY_get_size = 0: mock)
error creating ctx (mock)
encrypting
    the returned size is < buffer
    the returned size is == buffer
    the returned size is > buffer (mock)
    encryption fails (probably merge with the one below)
    password is too big for encryption (with 2 sizes)
buffer is reset?
*/

}  // namespace

BIO* BIO_new_mem_buf(const void*, int)
{
    ++openssl_mock.BIO_new_mem_buf_calls;
    return openssl_mock.bio;
}
int BIO_free(BIO*) { return 0; }

EVP_PKEY* PEM_read_bio_PUBKEY(BIO* bio, EVP_PKEY**, pem_password_cb*, void*)
{
    ++openssl_mock.PEM_read_bio_PUBKEY_calls;
    BOOST_TEST(bio == openssl_mock.bio);
    return openssl_mock.key;
}
void EVP_PKEY_free(EVP_PKEY*) {}

EVP_PKEY_CTX* EVP_PKEY_CTX_new(EVP_PKEY* pkey, ENGINE*)
{
    ++openssl_mock.EVP_PKEY_CTX_new_calls;
    BOOST_TEST(pkey == openssl_mock.key);
    return openssl_mock.ctx;
}
void EVP_PKEY_CTX_free(EVP_PKEY_CTX*) {}
int EVP_PKEY_encrypt_init(EVP_PKEY_CTX* ctx)
{
    ++openssl_mock.EVP_PKEY_encrypt_init_calls;
    BOOST_TEST(ctx == openssl_mock.ctx);
    return openssl_mock.encrypt_init_result;
}
int EVP_PKEY_CTX_set_rsa_padding(EVP_PKEY_CTX* ctx, int)
{
    ++openssl_mock.EVP_PKEY_CTX_set_rsa_padding_calls;
    BOOST_TEST(ctx == openssl_mock.ctx);
    return 0;
}
int EVP_PKEY_get_size(const EVP_PKEY* pkey)
{
    ++openssl_mock.EVP_PKEY_get_size_calls;
    BOOST_TEST(pkey == openssl_mock.key);
    return 256;
}
int EVP_PKEY_encrypt(EVP_PKEY_CTX* ctx, unsigned char*, size_t*, const unsigned char*, size_t)
{
    ++openssl_mock.EVP_PKEY_encrypt_calls;
    BOOST_TEST(ctx == openssl_mock.ctx);
    return 0;
}

unsigned long ERR_get_error() { return openssl_mock.last_error; }

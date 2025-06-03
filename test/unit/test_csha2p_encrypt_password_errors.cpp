//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

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
// and not linking to libssl/libcrypto.
// These tests cover cases that can't be covered directly by the unit tests using the real OpenSSL.
// Try to put as few tests here as possible.
// Some of the mocked functions are macros in OpenSSL versions before 3, so this setup can't work

#if OPENSSL_VERSION_NUMBER >= 0x30000000L

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
    int set_rsa_padding_result{1};
    int get_size_result{256};
    std::size_t actual_ciphertext_size{256u};
    unsigned long last_error{0u};

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

BOOST_AUTO_TEST_CASE(error_creating_pkey_ctx)
{
    // Setup
    openssl_mock = {};
    openssl_mock.ctx = nullptr;
    openssl_mock.last_error = 42u;
    vector_type out;

    // Call the function
    auto ec = csha2p_encrypt_password("passwd", scramble, {}, out, ssl_category);

    // Check
    BOOST_TEST(ec == error_code(42, ssl_category));
    BOOST_TEST(ec.has_location());
    BOOST_TEST(openssl_mock.EVP_PKEY_CTX_new_calls == 1u);
    BOOST_TEST(openssl_mock.EVP_PKEY_encrypt_init_calls == 0u);
}

BOOST_AUTO_TEST_CASE(error_setting_rsa_padding)
{
    // Setup. The return value should be != -2, which indicates
    // operation not supported and is handled separately
    openssl_mock = {};
    openssl_mock.set_rsa_padding_result = -1;
    openssl_mock.last_error = 42u;
    vector_type out;

    // Call the function
    auto ec = csha2p_encrypt_password("passwd", scramble, {}, out, ssl_category);

    // Check
    BOOST_TEST(ec == error_code(42, ssl_category));
    BOOST_TEST(ec.has_location());
    BOOST_TEST(openssl_mock.EVP_PKEY_CTX_set_rsa_padding_calls == 1u);
    BOOST_TEST(openssl_mock.EVP_PKEY_encrypt_calls == 0u);
}

// Getting a zero size as max buffer size might happen in theory (although it shouldn't for RSA)
BOOST_AUTO_TEST_CASE(get_size_zero)
{
    // Setup
    openssl_mock = {};
    openssl_mock.get_size_result = 0;
    openssl_mock.last_error = 42u;
    vector_type out;

    // Call the function
    auto ec = csha2p_encrypt_password("passwd", scramble, {}, out, ssl_category);

    // Check
    BOOST_TEST(ec == error_code(42, ssl_category));
    BOOST_TEST(ec.has_location());
    BOOST_TEST(openssl_mock.EVP_PKEY_get_size_calls == 1u);
    BOOST_TEST(openssl_mock.EVP_PKEY_encrypt_calls == 0u);
}

// In theory, the encryption function may communicate that it didn't use all the bytes
// in the buffer. This shouldn't happen in RSA, but we handle the case anyway
BOOST_AUTO_TEST_CASE(encrypt_actual_size_lt_max_size)
{
    // Setup
    openssl_mock = {};
    openssl_mock.get_size_result = 256;
    openssl_mock.actual_ciphertext_size = 200u;
    vector_type out;

    // Call the function
    auto ec = csha2p_encrypt_password("passwd", scramble, {}, out, ssl_category);

    // Check
    BOOST_TEST(ec == error_code());
    BOOST_TEST(out.size() == 200u);
}

// OpenSSL functions might fail without adding an error to the stack.
// If that's the case, the operation must still fail
BOOST_AUTO_TEST_CASE(error_code_zero)
{
    // Setup. The return value should be != -2, which indicates
    // operation not supported and is handled separately
    openssl_mock = {};
    openssl_mock.set_rsa_padding_result = -1;
    vector_type out;

    // Call the function
    auto ec = csha2p_encrypt_password("passwd", scramble, {}, out, ssl_category);

    // Check
    BOOST_TEST(ec == error_code(client_errc::unknown_openssl_error));
    BOOST_TEST(ec.has_location());
    BOOST_TEST(openssl_mock.EVP_PKEY_CTX_set_rsa_padding_calls == 1u);
    BOOST_TEST(openssl_mock.EVP_PKEY_encrypt_calls == 0u);
}

// OpenSSL 3+ might report system errors represented as codes > 0x80000000
BOOST_AUTO_TEST_CASE(error_code_system)
{
    // Setup. The return value should be != -2, which indicates
    // operation not supported and is handled separately
    openssl_mock = {};
    openssl_mock.set_rsa_padding_result = -1;
    openssl_mock.last_error = 0x800000ab;
    vector_type out;

    // Call the function
    auto ec = csha2p_encrypt_password("passwd", scramble, {}, out, ssl_category);

    // Check
    BOOST_TEST(ec.failed());
    BOOST_TEST(ec.has_location());
    BOOST_TEST((ec.category() == boost::system::system_category()));
    BOOST_TEST(openssl_mock.EVP_PKEY_CTX_set_rsa_padding_calls == 1u);
    BOOST_TEST(openssl_mock.EVP_PKEY_encrypt_calls == 0u);
}

}  // namespace

// Implement the OpenSSL functions
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
    return 1;
}
int EVP_PKEY_CTX_set_rsa_padding(EVP_PKEY_CTX* ctx, int)
{
    ++openssl_mock.EVP_PKEY_CTX_set_rsa_padding_calls;
    BOOST_TEST(ctx == openssl_mock.ctx);
    return openssl_mock.set_rsa_padding_result;
}

int EVP_PKEY_get_size(const EVP_PKEY* pkey)
{
    ++openssl_mock.EVP_PKEY_get_size_calls;
    BOOST_TEST(pkey == openssl_mock.key);
    return openssl_mock.get_size_result;
}
int EVP_PKEY_encrypt(EVP_PKEY_CTX* ctx, unsigned char*, size_t* actual_size, const unsigned char*, size_t)
{
    ++openssl_mock.EVP_PKEY_encrypt_calls;
    BOOST_TEST(ctx == openssl_mock.ctx);
    if (actual_size)
        *actual_size = openssl_mock.actual_ciphertext_size;
    return 1;
}

unsigned long ERR_get_error() { return openssl_mock.last_error; }

#else
BOOST_AUTO_TEST_CASE(dummy) {}
#endif

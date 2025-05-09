//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/impl/internal/sansio/csha2p_encrypt_password.hpp>

#include <boost/container/small_vector.hpp>
#include <boost/core/lightweight_test.hpp>

#include <cstdint>

using namespace boost::mysql;
using detail::csha2p_encrypt_password;

namespace {

constexpr std::uint8_t scramble[20]{};

using vector_type = boost::container::small_vector<std::uint8_t, 512>;

struct
{
    BIO* bio{reinterpret_cast<BIO*>(static_cast<std::uintptr_t>(100))};
    EVP_PKEY* key{reinterpret_cast<EVP_PKEY*>(static_cast<std::uintptr_t>(200))};
    EVP_PKEY_CTX* ctx{reinterpret_cast<EVP_PKEY_CTX*>(static_cast<std::uintptr_t>(300))};
    int encrypt_init_result{0};
    unsigned long last_error{0};

} openssl_mock;

void test_bio_new_error()
{
    openssl_mock.last_error = 42u;
    vector_type out;
    unsigned long err = csha2p_encrypt_password("passwd", scramble, {}, out);
    BOOST_TEST_EQ(err, 42u);
}

}  // namespace

BIO* BIO_new_mem_buf(const void*, int) { return openssl_mock.bio; }
int BIO_free(BIO*) { return 0; }

EVP_PKEY* PEM_read_bio_PUBKEY(BIO* bio, EVP_PKEY**, pem_password_cb*, void*)
{
    BOOST_TEST_EQ(bio, openssl_mock.bio);
    return openssl_mock.key;
}
void EVP_PKEY_free(EVP_PKEY*) {}

EVP_PKEY_CTX* EVP_PKEY_CTX_new(EVP_PKEY* pkey, ENGINE*)
{
    BOOST_TEST_EQ(pkey, openssl_mock.key);
    return openssl_mock.ctx;
}
void EVP_PKEY_CTX_free(EVP_PKEY_CTX*) {}
int EVP_PKEY_encrypt_init(EVP_PKEY_CTX* ctx)
{
    BOOST_TEST_EQ(ctx, openssl_mock.ctx);
    return openssl_mock.encrypt_init_result;
}
int EVP_PKEY_CTX_set_rsa_padding(EVP_PKEY_CTX* ctx, int)
{
    BOOST_TEST_EQ(ctx, openssl_mock.ctx);
    return 0;
}
int EVP_PKEY_get_size(const EVP_PKEY* pkey)
{
    BOOST_TEST_EQ(pkey, openssl_mock.key);
    return 256;
}
int EVP_PKEY_encrypt(EVP_PKEY_CTX* ctx, unsigned char*, size_t*, const unsigned char*, size_t)
{
    BOOST_TEST_EQ(ctx, openssl_mock.ctx);
    return 0;
}

unsigned long ERR_get_error() { return openssl_mock.last_error; }

int main()
{
    test_bio_new_error();

    return boost::report_errors();
}

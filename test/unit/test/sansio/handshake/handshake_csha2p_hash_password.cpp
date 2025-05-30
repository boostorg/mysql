//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/impl/internal/sansio/caching_sha2_password.hpp>
#include <boost/mysql/impl/internal/sansio/csha2p_encrypt_password.hpp>

#include <boost/core/span.hpp>
#include <boost/test/unit_test.hpp>

#include <cstddef>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <vector>

#include "test_common/assert_buffer_equals.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using detail::csha2p_encrypt_password;
using detail::csha2p_hash_password;

using buffer_type = boost::container::small_vector<std::uint8_t, 512>;

namespace {

BOOST_AUTO_TEST_SUITE(test_handshake_csha2p_hash_password)

// Values snooped using the MySQL Python connector
// TODO: this doesn't make a lot of sense as a separate test now
constexpr std::uint8_t scramble[20] = {
    0x3e, 0x3b, 0x4,  0x55, 0x4,  0x70, 0x16, 0x3a, 0x4c, 0x15,
    0x35, 0x3,  0x15, 0x76, 0x73, 0x22, 0x46, 0x8,  0x18, 0x1,
};

constexpr std::uint8_t hash[32] = {
    0xa1, 0xc1, 0xe1, 0xe9, 0x1b, 0xb6, 0x54, 0x4b, 0xa7, 0x37, 0x4b, 0x9c, 0x56, 0x6d, 0x69, 0x3e,
    0x6,  0xca, 0x7,  0x2,  0x98, 0xac, 0xd1, 0x6,  0x18, 0xc6, 0x90, 0x38, 0x9d, 0x88, 0xe1, 0x20,
};

BOOST_AUTO_TEST_CASE(nonempty_password)
{
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(csha2p_hash_password("hola", scramble), hash);
}

BOOST_AUTO_TEST_CASE(empty_password)
{
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(csha2p_hash_password("", scramble), std::vector<std::uint8_t>());
}

BOOST_AUTO_TEST_SUITE_END()

// Encrypting with RSA and OAEP padding involves random numbers for padding.
// There isn't a reliable way to seed OpenSSL's random number generators so that
// the output is deterministic. So we do the following:
//    1. We know the server's public and private keys and the password (the constants below).
//    2. We capture a scramble and a corresponding ciphertext using Wireshark.
//    3. We decrypt the ciphertext with OpenSSL to obtain the expected plaintext (the salted password).
//    4. Tests run csha2p_encrypt_password, decrypt its output, and verify that it matches the expected
//    plaintext.
BOOST_AUTO_TEST_SUITE(test_handshake_csha2p_encrypt_password)

constexpr unsigned char public_key_2048[] = R"%(-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA36OYSpdiy1lFDrdO1Vux
GwjPTo35R2+mXqW2SZV7kH5C6BSCeoTk6STVRBJbgOCabtp5bpUZ+x2bYWOZp4fs
JakC75CN2YTJoYg5z5U6XUBEWn6WNBpvEoSJaUtrzfU69J07uWqB6v0MdJf3JTgd
ILfGKvk2T+maxqUiYObs0BJd5eKJZDlUaf2r4a9KC8zGUZzHdgtZEXlkHVNLEbbD
Ju4KjtCtJCG1NEBAh3oSnNp/Q1FKFywqU7YnEBWI0B9C5UcKNFbg7M35daimXfGp
V7WJKhO9w7iBJYL1SW+PwyUCh3DNsuSm3nLmuwKhTvGQHZJS/5OVdSHgZhjDnk2V
WwIDAQAB
-----END PUBLIC KEY-----
)%";

constexpr unsigned char private_key_2048[] = R"%(-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDfo5hKl2LLWUUO
t07VW7EbCM9OjflHb6ZepbZJlXuQfkLoFIJ6hOTpJNVEEluA4Jpu2nlulRn7HZth
Y5mnh+wlqQLvkI3ZhMmhiDnPlTpdQERafpY0Gm8ShIlpS2vN9Tr0nTu5aoHq/Qx0
l/clOB0gt8Yq+TZP6ZrGpSJg5uzQEl3l4olkOVRp/avhr0oLzMZRnMd2C1kReWQd
U0sRtsMm7gqO0K0kIbU0QECHehKc2n9DUUoXLCpTticQFYjQH0LlRwo0VuDszfl1
qKZd8alXtYkqE73DuIElgvVJb4/DJQKHcM2y5Kbecua7AqFO8ZAdklL/k5V1IeBm
GMOeTZVbAgMBAAECggEANI0UtDJunKoVeCfK9ofdTiT70dG6yfaKeaMm+pONvZ5t
ymtHXdLsl3x4QM6vgdFFeNcNwdZ3jHKgmHn3GU7vRso4TmMBciOp3bNNImJGnLMF
XN5yHTw47XkHcR6v7m25tNFdv2wvqzBbROqQwMY20gFdJ6v3/z89h4A2W97nttyp
ixcNdSTHOfu6iUceEGi2PjHrvw4STPQeihXFNTnYG7hvlWzAerQ5STx5K2n4JoXz
xI5MuHS6PGj5EBPUoq0+EQhmhORWBdNCMcijpHqobVDjifPRY2JbI3zZHtVjYpGH
otmc72hIDjNW7RX1ePKW/gsq6p2by3U8+4dOdya4AQKBgQDz4/6uo7atJSTgVo3d
Zr/7UJ4Qi2mlc992jLAqTQle6JchhNERoPvb19Qy8BaOz6LcxmuMXnRij05mxdyb
LmoH8TPe6RFLCOJrRapkUjUtRD8dIg6UNFLk98LD5t3o5PoHwNpBFfokRlchwoHL
uBvbEHQkrPX2xPbgla5e7zJLgQKBgQDqvjCUMvmap9+AlqxGFhv51V9dIlKvT0xW
p5KEMVfs3JXCHAM7o0ClZ8NitHXw18E8+iMG4pWw3+FX4K6tKGR+rCeGCUNGuiQT
FzXjrEej+Pnuk8zacjXkbS+PNnEqhpSq6STZVFn2UW+ZWAoq2iAMR7qw0eAjylln
h//Wad3+2wKBgAjwWEtKUM2zyNA4G+b7dxnc8I4mre6UeqI7sdE7FZbW64Mc/RSq
U9DQ7kQXrJv7XDq/Qv3YEGf0XKlDozxEzToRSxdmb23Sm4nW+dHHeY95Kt8EeohQ
CqG9uvO3KHb6vXc/SECOb6aYtWTVXjB7RPoYdklJ1ZH/0hSVJ9ju52cBAoGBAK63
A90p25F6ZOWGP46iogve/e2JwFTvBnhwnKJ7P1/yBhzFULqwlUsG4euzOR0a2J6T
5kIXnyZYW5ZWimwi5jlJ1Nj0R/h6TqNO4TMlZOTsSMmDhDMKUoZDpeRHtw7ZwAk9
IcoH+DVXA2L0ngyq8LNzJ8a3TsYUs1pVZNunTC2FAoGARe4x29tdri8akxxwF3BV
bjJ9qRUIfIDK8rGWRdw94vVCB7XVmSWCEchmLqA1DqGYvAhYMYjkXTg9akfBTUQS
s+8JasUuQam8Y88JAfC0QqGbLgUsh0TpRUOXj+YQuoNiMVu14NNgYgFkx71WtvAq
kUmkxr/moPcZ+O1ahVjv/Us=
-----END PRIVATE KEY-----
)%";

// Decrypts the output of
std::vector<std::uint8_t> decrypt(
    boost::span<const std::uint8_t> private_key,
    boost::span<const std::uint8_t> ciphertext
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

    // Create a BIO with the key
    unique_bio bio(BIO_new_mem_buf(private_key.data(), private_key.size()));
    BOOST_TEST_REQUIRE(bio != nullptr, "Creating a BIO failed: " << ERR_get_error());

    // Load the key
    unique_evp_pkey pkey(PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr));
    BOOST_TEST_REQUIRE(bio != nullptr, "Loading the key failed: " << ERR_get_error());

    // Create a decryption context
    unique_evp_pkey_ctx ctx(EVP_PKEY_CTX_new(pkey.get(), nullptr));
    BOOST_TEST_REQUIRE(ctx != nullptr, "Creating a context failed: " << ERR_get_error());

    // Initialize it
    BOOST_TEST_REQUIRE(
        EVP_PKEY_decrypt_init(ctx.get()) > 0,
        "Initializing decryption failed: " << ERR_get_error()
    );

    // Set the padding scheme
    BOOST_TEST_REQUIRE(
        EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_OAEP_PADDING) > 0,
        "Setting the padding scheme failed: " << ERR_get_error()
    );

    // Determine the size of the decrypted buffer
    std::size_t out_size = 0;
    BOOST_TEST_REQUIRE(
        EVP_PKEY_decrypt(ctx.get(), nullptr, &out_size, ciphertext.data(), ciphertext.size()) > 0,
        "Determining decryption size failed: " << ERR_get_error()
    );
    std::vector<std::uint8_t> res(out_size, 0);

    // Actually decrypt
    BOOST_TEST_REQUIRE(
        EVP_PKEY_decrypt(ctx.get(), res.data(), &out_size, ciphertext.data(), ciphertext.size()) > 0,
        "Decrypting failed: " << ERR_get_error()
    );
    res.resize(out_size);

    // Done
    return res;
}

// Password is < length of the scramble
BOOST_AUTO_TEST_CASE(password_shorter_scramble)
{
    // Setup
    constexpr std::uint8_t scramble[] = {
        0x0f, 0x64, 0x4f, 0x2f, 0x2b, 0x3b, 0x27, 0x6b, 0x45, 0x5c,
        0x53, 0x01, 0x13, 0x7e, 0x4f, 0x10, 0x26, 0x23, 0x5d, 0x27,
    };
    constexpr std::uint8_t expected_decrypted[] =
        {0x6c, 0x17, 0x27, 0x4e, 0x19, 0x4b, 0x78, 0x1b, 0x24, 0x2f, 0x20, 0x76, 0x7c, 0x0c, 0x2b, 0x10};
    buffer_type buff;

    // Call the function
    unsigned long err = csha2p_encrypt_password("csha2p_password", scramble, public_key_2048, buff);

    // Verify
    BOOST_TEST_REQUIRE(err == 0u);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(decrypt(private_key_2048, buff), expected_decrypted);
}

// Password (with NULL byte) is equal to the length of the scramble
BOOST_AUTO_TEST_CASE(password_same_size_scramble)
{
    // Setup
    constexpr std::uint8_t scramble[] = {
        0x0f, 0x64, 0x4f, 0x2f, 0x2b, 0x3b, 0x27, 0x6b, 0x45, 0x5c,
        0x53, 0x01, 0x13, 0x7e, 0x4f, 0x10, 0x26, 0x23, 0x5d, 0x27,
    };
    constexpr std::uint8_t expected_decrypted[] = {
        0x67, 0x0e, 0x2d, 0x45, 0x4f, 0x02, 0x15, 0x58, 0x0e, 0x17,
        0x1f, 0x68, 0x7c, 0x0d, 0x20, 0x79, 0x1f, 0x13, 0x17, 0x27,
    };
    buffer_type buff;

    // Call the function
    unsigned long err = csha2p_encrypt_password("hjbjd923KKLiosoi90J", scramble, public_key_2048, buff);

    // Verify
    BOOST_TEST_REQUIRE(err == 0u);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(decrypt(private_key_2048, buff), expected_decrypted);
}

// Passwords longer than the scramble use it cyclically
BOOST_AUTO_TEST_CASE(password_longer_scramble)
{
    // Setup
    constexpr std::uint8_t scramble[] = {
        0x0f, 0x64, 0x4f, 0x2f, 0x2b, 0x3b, 0x27, 0x6b, 0x45, 0x5c,
        0x53, 0x01, 0x13, 0x7e, 0x4f, 0x10, 0x26, 0x23, 0x5d, 0x27,
    };
    constexpr std::uint8_t expected_decrypted[] = {
        0x64, 0x0e, 0x2e, 0x5c, 0x40, 0x52, 0x1f, 0x59, 0x7c, 0x6f, 0x6b, 0x31, 0x79, 0x08, 0x21, 0x7e, 0x6b,
        0x0f, 0x36, 0x46, 0x34, 0x5e, 0x75, 0x70, 0x40, 0x48, 0x4f, 0x0d, 0x7c, 0x6f, 0x1a, 0x4b, 0x50, 0x32,
        0x06, 0x5a, 0x69, 0x0a, 0x37, 0x44, 0x65, 0x17, 0x21, 0x4e, 0x40, 0x57, 0x68, 0x54, 0x24, 0x5c,
    };
    buffer_type buff;

    // Call the function
    unsigned long err = csha2p_encrypt_password(
        "kjaski829380jvnnM,ka;::_kshf93IJCLIJO)jcjsnaklO?a",
        scramble,
        public_key_2048,
        buff
    );

    // Verify
    BOOST_TEST_REQUIRE(err == 0u);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(decrypt(private_key_2048, buff), expected_decrypted);
}

// The longest password that a 2048 RSA key can encrypt with the OAEP scheme
BOOST_AUTO_TEST_CASE(password_max_size_2048)
{
    // Setup
    constexpr std::uint8_t scramble[] = {
        0x0f, 0x64, 0x4f, 0x2f, 0x2b, 0x3b, 0x27, 0x6b, 0x45, 0x5c,
        0x53, 0x01, 0x13, 0x7e, 0x4f, 0x10, 0x26, 0x23, 0x5d, 0x27,
    };
    constexpr std::uint8_t expected_decrypted[] = {
        0x67, 0x0e, 0x2d, 0x45, 0x4f, 0x02, 0x15, 0x58, 0x0e, 0x17, 0x1f, 0x6a, 0x79, 0x1c, 0x2b, 0x7b, 0x45,
        0x49, 0x2a, 0x4f, 0x6a, 0x0f, 0x26, 0x56, 0x13, 0x08, 0x1e, 0x58, 0x2a, 0x29, 0x61, 0x76, 0x76, 0x0b,
        0x3c, 0x79, 0x42, 0x4b, 0x34, 0x46, 0x67, 0x2e, 0x0d, 0x64, 0x61, 0x70, 0x6f, 0x28, 0x0c, 0x14, 0x10,
        0x4a, 0x59, 0x37, 0x3a, 0x29, 0x6d, 0x6b, 0x12, 0x17, 0x36, 0x2d, 0x05, 0x66, 0x5b, 0x54, 0x4d, 0x0a,
        0x23, 0x6c, 0x24, 0x32, 0x2a, 0x14, 0x2e, 0x7c, 0x55, 0x49, 0x10, 0x6a, 0x44, 0x0e, 0x24, 0x45, 0x43,
        0x52, 0x52, 0x0e, 0x7c, 0x6f, 0x1a, 0x3c, 0x3a, 0x57, 0x1a, 0x48, 0x6f, 0x6c, 0x17, 0x6c, 0x57, 0x2a,
        0x04, 0x61, 0x43, 0x50, 0x46, 0x02, 0x7d, 0x65, 0x61, 0x32, 0x7c, 0x17, 0x2e, 0x67, 0x4f, 0x42, 0x36,
        0x54, 0x7c, 0x05, 0x24, 0x41, 0x43, 0x5a, 0x4c, 0x03, 0x0c, 0x15, 0x1b, 0x48, 0x50, 0x36, 0x06, 0x5f,
        0x0f, 0x60, 0x08, 0x0e, 0x46, 0x2c, 0x0c, 0x64, 0x63, 0x78, 0x6c, 0x21, 0x2d, 0x35, 0x20, 0x68, 0x64,
        0x1b, 0x26, 0x7f, 0x6e, 0x6b, 0x17, 0x6f, 0x5a, 0x27, 0x07, 0x66, 0x62, 0x72, 0x6d, 0x22, 0x0a, 0x0c,
        0x38, 0x6b, 0x74, 0x09, 0x26, 0x7a, 0x4f, 0x4c, 0x2e, 0x48, 0x66, 0x5d, 0x25, 0x5c, 0x5e, 0x03, 0x13,
        0x23, 0x0d, 0x09, 0x1b, 0x42, 0x5b, 0x37, 0x76, 0x28, 0x15, 0x1a, 0x35, 0x43, 0x65, 0x17, 0x2d, 0x5c,
        0x4f, 0x51, 0x52, 0x3e, 0x0d, 0x16, 0x39, 0x63, 0x59, 0x7e,
    };
    buffer_type buff;

    string_view password =
        "hjbjd923KKLkjbdkcjwhekiy8393ou2weusidhiahJBKJKHCIHCKJIu9KHO09IJIpojaf0w39jalsjMMKjkjhiue93I=))"
        "UXIOJKXNKNhkai8923oiawiakssaknhakhIIHICHIO)CU)"
        "IHCKHCKJhisiweioHHJHUCHIIIJIOPkjgwijiosoi9jsu84HHUHCHI9839hdjsbsdjuUHJjbJ";

    BOOST_TEST(password.size() == 213u);

    // Call the function
    unsigned long err = csha2p_encrypt_password(password, scramble, public_key_2048, buff);

    // Verify
    BOOST_TEST_REQUIRE(err == 0u);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(decrypt(private_key_2048, buff), expected_decrypted);
}

/**
encrypt password success
    success password is > max length but key is longer than default max length
    password is > 512 (small buffer)
    digest is > 512 (small buffer)
    no character causes problems
error creating buffer (mock)
error loading key
    buffer is empty
    key is malformed
    TODO: should we fuzz this function?
    key is not RSA
    key is smaller than what we expect?
determining the size of the hash
    failure (EVP_PKEY_get_size < 0: mock)
    not available (EVP_PKEY_get_size = 0: mock)
error creating ctx (mock)
error setting RSA padding (mock)
encrypting
    the returned size is < buffer
    the returned size is == buffer
    the returned size is > buffer (mock)
    encryption fails (probably merge with the one below)
    password is too big for encryption (with 2 sizes)
buffer is reset?
*/

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
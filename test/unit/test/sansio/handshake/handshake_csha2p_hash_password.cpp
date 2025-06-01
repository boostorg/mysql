//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/impl/internal/sansio/caching_sha2_password.hpp>
#include <boost/mysql/impl/internal/sansio/csha2p_encrypt_password.hpp>

#include <boost/asio/ssl/error.hpp>
#include <boost/core/span.hpp>
#include <boost/test/unit_test.hpp>

#include <cstddef>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <string>
#include <vector>

#include "test_common/assert_buffer_equals.hpp"
#include "test_common/printing.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using boost::asio::error::ssl_category;
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

constexpr unsigned char public_key_8192[] = R"%(-----BEGIN PUBLIC KEY-----
MIIEIjANBgkqhkiG9w0BAQEFAAOCBA8AMIIECgKCBAEA43dhEnCSwC/hAAm9XrZc
t0QaC4bkoSifeiL00+U8xBbAuAYnZSQ4PBbEERnxIRLpgCf0b2SDvCXPJmXB6lKz
1W4vVcw2fVuG4Kmp5C9skhXeGXrXoEOgSxBp8VkWWWB1tbpKmKwdGnh6O0JOwWRq
wWxefR3J9EWBNTz0vvbjGxYWt0XfcCjULPuXuPVlrbZkTsdCl8ncWG8G/OjRNHr3
f9eiWHb2gA+xsUJfP/iWlUmy+9MiEGBT+JMW2wZAOu7JgVbyZWXV/pkwodP1IVFy
c9RTexnC7Th3dDlT+HcAWXDCTuWwq2KhD/WUnfXO6uZEjyp3aJiJOa7Mceag0B9t
T9d1AZkXQs3PZYn+Sl1cF3hq6e8OoKHZsA95PAtNrXfDe7vkyICB4sp1Dkqg75Ws
arOTC45CWcr/woReqCDYRiSr0aQodlJmSvSIlOz5IAyB40nFzA3VgEgTgHJ4mU7O
QcsOSStmZNvk6Hl9xfUfJi9xTB/vwf7FKWe+IcDGdkU9WV3yJ6UTXPW4TQu25uTc
6WoK4Us8Lk2h7f+Tk2dsyt+VTDOma5qbwTT8U/NKSd8Se5ewS4++ERqaRikLmp0o
AciYxH00Wk3ZLPFUn9/svcA4BAwOCTkYjIQvW/gLDrYyu4qyrgkGXiNs0L2QYB7l
AUTOl5M0Bmez5Nl8SIdaVk4bj5D+4BbHrDZGLMkkP1KWRgNFOJOfLT3JgMPxDwme
n4qFs/HV/d4qh8kzyjITsBRQj1EloqqU+40WJ9X3mIEo7xQ1DPloQMWADDYrO/bX
fFtYnTtnJLV6ndTSdQz0yZ7ubROJFatPy1VQ6AiBeN24sq9iKm6NXuWjszS0paZ/
JfO+QOTB9D3lVnFzV2yHRIboHNpm/X1eVeXDlzzVTobqVr4eN78bL4+VQiGQav1I
8ebv3Gx9+NO1qVfsWHC89I+hw9HSN69gEyCmeWwIuY8hMGfTBgERVKPkWhSPM0cU
JlkM+ER1Pngz93TR7tWPsp9tYaKMG4DVth0fINRSVsFFkeHu1CLeKboUW8QMoH+A
vPDVnSE2M3ecAeFz0yNXTwVtHbQT2YpYZ88gnYvYcRNdqeOvRKkrGUON0fDGvBnF
xxcuNzZVP2wBEUTkYljnLrJ5zR+uabPgrQiIhTVdDplRRZd6DVRh9s357L5b94tX
wtNAFfHT3AsU/C5p8O6taxckrxYMqNsOJ/6zYp7gUTTdb3e/V1c3xgi/x8+gUrUn
ivGZZdOaWUbSDZf1cz2kfQPsaiDDPVKL7goo0ZA5gnbjkhHUNpGmE5KauHek9rxO
2KX3dpjl3YJyHSmOKv7Lf46Uja0cNWbTh/0nHlG5xtbzBNGvxF90iGmHDmr+u6BT
zwIDAQAB
-----END PUBLIC KEY-----
)%";

constexpr unsigned char private_key_8192[] = R"%(-----BEGIN PRIVATE KEY-----
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
2vrUYCC8gWblftYH+yTWu2bQ4Y1pDw==
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

BOOST_AUTO_TEST_CASE(success)
{
    // Common for all tests
    constexpr std::uint8_t scramble[] = {
        0x0f, 0x64, 0x4f, 0x2f, 0x2b, 0x3b, 0x27, 0x6b, 0x45, 0x5c,
        0x53, 0x01, 0x13, 0x7e, 0x4f, 0x10, 0x26, 0x23, 0x5d, 0x27,
    };

    // A password with all the possible ASCII values
    std::string all_chars_password;
    for (int i = 0; i < 256; ++i)
        all_chars_password.push_back(static_cast<char>(i));

    struct
    {
        string_view name;
        std::string password;
        boost::span<const std::uint8_t> public_key;
        boost::span<const std::uint8_t> private_key;
        std::vector<std::uint8_t> expected_decrypted;
    } test_cases[] = {
        // clang-format off
        // An empty password does not cause trouble. This is an edge case, since
        // an empty password should employ a different workflow
        {
            "password_empty",
            {},
            public_key_2048,
            private_key_2048,
            {0x0f}
        },

        // Password is < length of the scramble
        {
            "password_shorter_scramble",
            "csha2p_password",
            public_key_2048,
            private_key_2048, 
            {
                0x6c, 0x17, 0x27, 0x4e, 0x19, 0x4b, 0x78, 0x1b, 0x24, 0x2f, 0x20, 0x76, 0x7c, 0x0c, 0x2b, 0x10
            }
        },

        // Password (with NULL byte) is equal to the length of the scramble
        {
            "password_same_size_scramble",
            "hjbjd923KKLiosoi90J",
            public_key_2048,
            private_key_2048,
            {
                0x67, 0x0e, 0x2d, 0x45, 0x4f, 0x02, 0x15, 0x58, 0x0e, 0x17,
                0x1f, 0x68, 0x7c, 0x0d, 0x20, 0x79, 0x1f, 0x13, 0x17, 0x27
            }
        },

        // Passwords longer than the scramble use it cyclically
        {
            "password_longer_scramble",
            "kjaski829380jvnnM,ka;::_kshf93IJCLIJO)jcjsnaklO?a",
            public_key_2048,
            private_key_2048,
            {
                0x64, 0x0e, 0x2e, 0x5c, 0x40, 0x52, 0x1f, 0x59, 0x7c, 0x6f, 0x6b, 0x31, 0x79, 0x08, 0x21, 0x7e, 0x6b,
                0x0f, 0x36, 0x46, 0x34, 0x5e, 0x75, 0x70, 0x40, 0x48, 0x4f, 0x0d, 0x7c, 0x6f, 0x1a, 0x4b, 0x50, 0x32,
                0x06, 0x5a, 0x69, 0x0a, 0x37, 0x44, 0x65, 0x17, 0x21, 0x4e, 0x40, 0x57, 0x68, 0x54, 0x24, 0x5c,
            }
        },

        // The longest password that a 2048 RSA key can encrypt with the OAEP scheme
        {
            "password_max_size_2048",
            "hjbjd923KKLkjbdkcjwhekiy8393ou2weusidhiahJBKJKHCIHCKJIu9KHO09IJIpojaf0w39jalsjMMKjkjhiue93I=))"
                "UXIOJKXNKNhkai8923oiawiakssaknhakhIIHICHIO)CU)"
                "IHCKHCKJhisiweioHHJHUCHIIIJIOPkjgwijiosoi9jsu84HHUHCHI9839hdjsbsdjuUHJjbJ",
            public_key_2048,
            private_key_2048,
            {
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
            }
        },

        // Longer passwords are accepted if the key supports them.
        // Concretely, passwords longer than 512 (size of our SBO) are supported
        {
            "password_longer_sbo",
            std::string(600, '5'),
            public_key_8192,
            private_key_8192,
            {
                0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13,
                0x16, 0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66, 0x34, 0x26, 0x4b,
                0x7a, 0x25, 0x13, 0x16, 0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66,
                0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13, 0x16, 0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e,
                0x70, 0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13, 0x16, 0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e,
                0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13, 0x16, 0x68, 0x12, 0x3a, 0x51,
                0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13, 0x16, 0x68,
                0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25,
                0x13, 0x16, 0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66, 0x34, 0x26,
                0x4b, 0x7a, 0x25, 0x13, 0x16, 0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69,
                0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13, 0x16, 0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12,
                0x5e, 0x70, 0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13, 0x16, 0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a,
                0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13, 0x16, 0x68, 0x12, 0x3a,
                0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13, 0x16,
                0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a,
                0x25, 0x13, 0x16, 0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66, 0x34,
                0x26, 0x4b, 0x7a, 0x25, 0x13, 0x16, 0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70,
                0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13, 0x16, 0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e,
                0x12, 0x5e, 0x70, 0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13, 0x16, 0x68, 0x12, 0x3a, 0x51, 0x7a,
                0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13, 0x16, 0x68, 0x12,
                0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13,
                0x16, 0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66, 0x34, 0x26, 0x4b,
                0x7a, 0x25, 0x13, 0x16, 0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66,
                0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13, 0x16, 0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e,
                0x70, 0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13, 0x16, 0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e,
                0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13, 0x16, 0x68, 0x12, 0x3a, 0x51,
                0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13, 0x16, 0x68,
                0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25,
                0x13, 0x16, 0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66, 0x34, 0x26,
                0x4b, 0x7a, 0x25, 0x13, 0x16, 0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69,
                0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13, 0x16, 0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12,
                0x5e, 0x70, 0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13, 0x16, 0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a,
                0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13, 0x16, 0x68, 0x12, 0x3a,
                0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a, 0x25, 0x13, 0x16,
                0x68, 0x12, 0x3a, 0x51, 0x7a, 0x1a, 0x1e, 0x0e, 0x12, 0x5e, 0x70, 0x69, 0x66, 0x34, 0x26, 0x4b, 0x7a,
                0x25, 0x13, 0x16, 0x68, 0x12, 0x0f
            }
        },

        // No character causes trouble
        {
            "password_all_characters",
            all_chars_password,
            public_key_8192,
            private_key_8192,
            {
                0x0f, 0x65, 0x4d, 0x2c, 0x2f, 0x3e, 0x21, 0x6c, 0x4d, 0x55, 0x59, 0x0a, 0x1f, 0x73, 0x41, 0x1f, 0x36,
                0x32, 0x4f, 0x34, 0x1b, 0x71, 0x59, 0x38, 0x33, 0x22, 0x3d, 0x70, 0x59, 0x41, 0x4d, 0x1e, 0x33, 0x5f,
                0x6d, 0x33, 0x02, 0x06, 0x7b, 0x00, 0x27, 0x4d, 0x65, 0x04, 0x07, 0x16, 0x09, 0x44, 0x75, 0x6d, 0x61,
                0x32, 0x27, 0x4b, 0x79, 0x27, 0x1e, 0x1a, 0x67, 0x1c, 0x33, 0x59, 0x71, 0x10, 0x6b, 0x7a, 0x65, 0x28,
                0x01, 0x19, 0x15, 0x46, 0x5b, 0x37, 0x05, 0x5b, 0x6a, 0x6e, 0x13, 0x68, 0x5f, 0x35, 0x1d, 0x7c, 0x7f,
                0x6e, 0x71, 0x3c, 0x1d, 0x05, 0x09, 0x5a, 0x4f, 0x23, 0x11, 0x4f, 0x46, 0x42, 0x3f, 0x44, 0x6b, 0x01,
                0x29, 0x48, 0x43, 0x52, 0x4d, 0x00, 0x29, 0x31, 0x3d, 0x6e, 0x63, 0x0f, 0x3d, 0x63, 0x52, 0x56, 0x2b,
                0x50, 0x77, 0x1d, 0x35, 0x54, 0x57, 0x46, 0x59, 0x14, 0xc5, 0xdd, 0xd1, 0x82, 0x97, 0xfb, 0xc9, 0x97,
                0xae, 0xaa, 0xd7, 0xac, 0x83, 0xe9, 0xc1, 0xa0, 0xbb, 0xaa, 0xb5, 0xf8, 0xd1, 0xc9, 0xc5, 0x96, 0x8b,
                0xe7, 0xd5, 0x8b, 0xba, 0xbe, 0xc3, 0xb8, 0xaf, 0xc5, 0xed, 0x8c, 0x8f, 0x9e, 0x81, 0xcc, 0xed, 0xf5,
                0xf9, 0xaa, 0xbf, 0xd3, 0xe1, 0xbf, 0x96, 0x92, 0xef, 0x94, 0xbb, 0xd1, 0xf9, 0x98, 0x93, 0x82, 0x9d,
                0xd0, 0xf9, 0xe1, 0xed, 0xbe, 0xd3, 0xbf, 0x8d, 0xd3, 0xe2, 0xe6, 0x9b, 0xe0, 0xc7, 0xad, 0x85, 0xe4,
                0xe7, 0xf6, 0xe9, 0xa4, 0x95, 0x8d, 0x81, 0xd2, 0xc7, 0xab, 0x99, 0xc7, 0xfe, 0xfa, 0x87, 0xfc, 0xd3,
                0xb9, 0x91, 0xf0, 0xcb, 0xda, 0xc5, 0x88, 0xa1, 0xb9, 0xb5, 0xe6, 0xfb, 0x97, 0xa5, 0xfb, 0xca, 0xce,
                0xb3, 0xc8, 0xff, 0x95, 0xbd, 0xdc, 0xdf, 0xce, 0xd1, 0x9c, 0xbd, 0xa5, 0xa9, 0xfa, 0xef, 0x83, 0xb1,
                0xef, 0x26
            }
        }

        // clang-format on
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            buffer_type buff;

            // Call the function
            auto ec = csha2p_encrypt_password(tc.password, scramble, tc.public_key, buff, ssl_category);

            // Verify
            BOOST_TEST_REQUIRE(ec == error_code());
            BOOST_MYSQL_ASSERT_BUFFER_EQUALS(decrypt(tc.private_key, buff), tc.expected_decrypted);
        }
    }
}

//
// Errors. It's not defined what exact error code will each function return.
//

// We passed an empty buffer to the key parser
BOOST_AUTO_TEST_CASE(error_key_buffer_empty)
{
    constexpr std::uint8_t scramble[] = {
        0x0f, 0x64, 0x4f, 0x2f, 0x2b, 0x3b, 0x27, 0x6b, 0x45, 0x5c,
        0x53, 0x01, 0x13, 0x7e, 0x4f, 0x10, 0x26, 0x23, 0x5d, 0x27,
    };
    buffer_type buff;
    auto ec = csha2p_encrypt_password("csha2p_password", scramble, {}, buff, ssl_category);
    BOOST_TEST((ec.category() == ssl_category));
    BOOST_TEST(ec.value() > 0u);     // is an error
    BOOST_TEST(ec.message() != "");  // produces some output
}

BOOST_AUTO_TEST_CASE(error_key_malformed)
{
    constexpr std::uint8_t scramble[] = {
        0x0f, 0x64, 0x4f, 0x2f, 0x2b, 0x3b, 0x27, 0x6b, 0x45, 0x5c,
        0x53, 0x01, 0x13, 0x7e, 0x4f, 0x10, 0x26, 0x23, 0x5d, 0x27,
    };
    constexpr std::uint8_t key_buffer[] = "-----BEGIN PUBLIC KEY-----zwIDAQAB__kaj0?))=";
    buffer_type buff;
    auto ec = csha2p_encrypt_password("csha2p_password", scramble, key_buffer, buff, ssl_category);
    BOOST_TEST((ec.category() == ssl_category));
    BOOST_TEST(ec.value() > 0u);     // is an error
    BOOST_TEST(ec.message() != "");  // produces some output
}

// Passing in a public key type that does not support encryption operations
// (like ECDSA) fails
BOOST_AUTO_TEST_CASE(error_key_doesnt_support_encryption)
{
    constexpr std::uint8_t scramble[] = {
        0x0f, 0x64, 0x4f, 0x2f, 0x2b, 0x3b, 0x27, 0x6b, 0x45, 0x5c,
        0x53, 0x01, 0x13, 0x7e, 0x4f, 0x10, 0x26, 0x23, 0x5d, 0x27,
    };
    constexpr unsigned char key_buffer[] = R"%(-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAERDkCI/degPJXEIYYncyvGsTdj9YI
4xQ6KPTzoF+DY2jM09w1TCncxk9XV1eOo44UMDUuK9K01halQy70mohFSQ==
-----END PUBLIC KEY-----
)%";

    buffer_type buff;
    auto ec = csha2p_encrypt_password("csha2p_password", scramble, key_buffer, buff, ssl_category);
    BOOST_TEST((ec.category() == ssl_category));
    BOOST_TEST(ec.value() > 0u);     // is an error
    BOOST_TEST(ec.message() != "");  // produces some output
}

// Passing in a public key type that allows encryption but is not RSA fails as expected.
// This is a SM2 key, generated by:
//   openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:sm2 -out public.pem
//   openssl pkey -in private.pem -pubout -out public.pem
BOOST_AUTO_TEST_CASE(error_key_not_rsa)
{
    constexpr std::uint8_t scramble[] = {
        0x0f, 0x64, 0x4f, 0x2f, 0x2b, 0x3b, 0x27, 0x6b, 0x45, 0x5c,
        0x53, 0x01, 0x13, 0x7e, 0x4f, 0x10, 0x26, 0x23, 0x5d, 0x27,
    };
    constexpr unsigned char key_buffer[] = R"%(-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoEcz1UBgi0DQgAEWGAXSpPHb2bWQjROuegjWPcuVwNW
mVpTh++3j7pnpWUjnFuarvWmWh/H6t96/pTx566FKGxZpLn3H9TLHZJsog==
-----END PUBLIC KEY-----
)%";

    buffer_type buff;
    auto ec = csha2p_encrypt_password("csha2p_password", scramble, key_buffer, buff, ssl_category);
    BOOST_TEST(ec == client_errc::protocol_value_error);  // OpenSSL does not provide an error code here
}

/**
error creating buffer (mock)
error loading key
    TODO: should we fuzz this function?
    key is smaller than what we expect?
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

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
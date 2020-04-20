//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/detail/auth/auth_calculator.hpp"
#include "test_common.hpp"
#include <gtest/gtest.h>

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using namespace testing;
using boost::mysql::error_code;
using boost::mysql::errc;

namespace
{

// mysql_native_password
struct MysqlNativePasswordTest : public Test
{
    auth_calculator calc;
    std::uint8_t challenge_buffer [20] {
        0x79, 0x64, 0x3d, 0x12, 0x1d, 0x71, 0x74, 0x47,
        0x5f, 0x48, 0x3e, 0x3e, 0x0b, 0x62, 0x0a, 0x03,
        0x3d, 0x27, 0x3a, 0x4c
    }; // Values snooped using Wireshark
    std::uint8_t expected_buffer [20] {
        0xf1, 0xb2, 0xfb, 0x1c, 0x8d, 0xe7, 0x5d, 0xb8,
        0xeb, 0xa8, 0x12, 0x6a, 0xd1, 0x0f, 0xe9, 0xb1,
        0x10, 0x50, 0xd4, 0x28
    };
    std::string_view challenge = makesv(challenge_buffer);
    std::string_view expected = makesv(expected_buffer);
};

TEST_F(MysqlNativePasswordTest, NonEmptyPasswordSslFalse_ReturnsExpectedHash)
{
    auto err = calc.calculate("mysql_native_password", "root", challenge, false);
    EXPECT_EQ(err, error_code());
    EXPECT_EQ(calc.response(), expected);
    EXPECT_EQ(calc.plugin_name(), "mysql_native_password");
}

TEST_F(MysqlNativePasswordTest, NonEmptyPasswordSslTrue_ReturnsExpectedHash)
{
    auto err = calc.calculate("mysql_native_password", "root", challenge, true);
    EXPECT_EQ(err, error_code());
    EXPECT_EQ(calc.response(), expected);
    EXPECT_EQ(calc.plugin_name(), "mysql_native_password");
}

TEST_F(MysqlNativePasswordTest, EmptyPasswordSslFalse_ReturnsEmpty)
{
    auto err = calc.calculate("mysql_native_password", "", challenge, false);
    EXPECT_EQ(err, error_code());
    EXPECT_EQ(calc.response(), "");
    EXPECT_EQ(calc.plugin_name(), "mysql_native_password");
}

TEST_F(MysqlNativePasswordTest, EmptyPasswordSslTrue_ReturnsEmpty)
{
    auto err = calc.calculate("mysql_native_password", "", challenge, false);
    EXPECT_EQ(err, error_code());
    EXPECT_EQ(calc.response(), "");
    EXPECT_EQ(calc.plugin_name(), "mysql_native_password");
}

TEST_F(MysqlNativePasswordTest, BadChallengeLength_Fail)
{
    EXPECT_EQ((calc.calculate("mysql_native_password", "password", "", true)),
            make_error_code(errc::protocol_value_error));
    EXPECT_EQ((calc.calculate("mysql_native_password", "password", "bad_challenge", true)),
            make_error_code(errc::protocol_value_error));
}

// caching_sha2_password
struct CachingSha2PasswordTest : public Test
{
    auth_calculator calc;
    std::uint8_t challenge_buffer [20] {
        0x3e, 0x3b, 0x4, 0x55, 0x4, 0x70, 0x16, 0x3a,
        0x4c, 0x15, 0x35, 0x3, 0x15, 0x76, 0x73, 0x22,
        0x46, 0x8, 0x18, 0x1
    }; // Values snooped using the MySQL Python connector
    std::uint8_t expected_buffer [32] {
        0xa1, 0xc1, 0xe1, 0xe9, 0x1b, 0xb6, 0x54, 0x4b,
        0xa7, 0x37, 0x4b, 0x9c, 0x56, 0x6d, 0x69, 0x3e,
        0x6, 0xca, 0x7, 0x2, 0x98, 0xac, 0xd1, 0x6,
        0x18, 0xc6, 0x90, 0x38, 0x9d, 0x88, 0xe1, 0x20
    };
    std::string_view challenge = makesv(challenge_buffer);
    std::string_view expected = makesv(expected_buffer);
    std::string_view cleartext_challenge { "\4" };
};

TEST_F(CachingSha2PasswordTest, NonEmptyPasswordChallengeAuthSslFalse_ReturnsExpectedHash)
{
    auto err = calc.calculate("caching_sha2_password", "hola", challenge, false);
    EXPECT_EQ(err, error_code());
    EXPECT_EQ(calc.response(), expected);
    EXPECT_EQ(calc.plugin_name(), "caching_sha2_password");
}

TEST_F(CachingSha2PasswordTest, NonEmptyPasswordChallengeAuthSslTrue_ReturnsExpectedHash)
{
    auto err = calc.calculate("caching_sha2_password", "hola", challenge, true);
    EXPECT_EQ(err, error_code());
    EXPECT_EQ(calc.response(), expected);
    EXPECT_EQ(calc.plugin_name(), "caching_sha2_password");
}

TEST_F(CachingSha2PasswordTest, NonEmptyPasswordCleartextAuthSslFalse_Fail)
{
    auto err = calc.calculate("caching_sha2_password", "hola", cleartext_challenge, false);
    EXPECT_EQ(err, make_error_code(errc::auth_plugin_requires_ssl));
}

TEST_F(CachingSha2PasswordTest, NonEmptyPasswordCleartextAuthSslTrue_ReturnsPassword)
{
    auto err = calc.calculate("caching_sha2_password", "hola", cleartext_challenge, true);
    EXPECT_EQ(err, error_code());
    EXPECT_EQ(calc.response(), std::string("hola") + '\0');
    EXPECT_EQ(calc.plugin_name(), "caching_sha2_password");
}

TEST_F(CachingSha2PasswordTest, EmptyPasswordChallengeAuthSslFalse_ReturnsEmpty)
{
    auto err = calc.calculate("caching_sha2_password", "", challenge, false);
    EXPECT_EQ(err, error_code());
    EXPECT_EQ(calc.response(), "");
    EXPECT_EQ(calc.plugin_name(), "caching_sha2_password");
}

TEST_F(CachingSha2PasswordTest, EmptyPasswordChallengeAuthSslTrue_ReturnsEmpty)
{
    auto err = calc.calculate("caching_sha2_password", "", challenge, true);
    EXPECT_EQ(err, error_code());
    EXPECT_EQ(calc.response(), "");
    EXPECT_EQ(calc.plugin_name(), "caching_sha2_password");
}

TEST_F(CachingSha2PasswordTest, EmptyPasswordCleartextAuthSslFalse_ReturnsEmpty)
{
    auto err = calc.calculate("caching_sha2_password", "", cleartext_challenge, false);
    EXPECT_EQ(err, error_code());
    EXPECT_EQ(calc.response(), "");
    EXPECT_EQ(calc.plugin_name(), "caching_sha2_password");
}

TEST_F(CachingSha2PasswordTest, EmptyPasswordCleartextAuthSslTrue_ReturnsEmpty)
{
    auto err = calc.calculate("caching_sha2_password", "", cleartext_challenge, true);
    EXPECT_EQ(err, error_code());
    EXPECT_EQ(calc.response(), "");
    EXPECT_EQ(calc.plugin_name(), "caching_sha2_password");
}

TEST_F(CachingSha2PasswordTest, BadChallengeLength_Fail)
{
    EXPECT_EQ((calc.calculate("caching_sha2_password", "password", "", true)),
            make_error_code(errc::protocol_value_error));
    EXPECT_EQ((calc.calculate("caching_sha2_password", "password", "bad_challenge", true)),
            make_error_code(errc::protocol_value_error));
}

// Bad authentication plugin
TEST(AuthCalculator, UnknownAuthPlugin_Fail)
{
    auth_calculator calc;
    EXPECT_EQ((calc.calculate("bad_plugin", "password", "challenge", true)),
            make_error_code(errc::unknown_auth_plugin));
    EXPECT_EQ((calc.calculate("", "password", "challenge", true)),
            make_error_code(errc::unknown_auth_plugin));
}


}

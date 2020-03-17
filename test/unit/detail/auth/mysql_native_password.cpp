/*
 * auth.cpp
 *
 *  Created on: Oct 21, 2019
 *      Author: ruben
 */

#include "boost/mysql/detail/auth/mysql_native_password.hpp"
#include <gtest/gtest.h>
#include <array>

using namespace mysql::detail;
using namespace testing;

namespace
{

TEST(MysqlNativePassword, ComputeAuthString_NonEmptyPassword_ReturnsExpectedHash)
{
	// Values snooped using Wireshark
	std::array<std::uint8_t, mysql_native_password::challenge_length> challenge {
		0x79, 0x64, 0x3d, 0x12, 0x1d, 0x71, 0x74, 0x47,
		0x5f, 0x48, 0x3e, 0x3e, 0x0b, 0x62, 0x0a, 0x03,
		0x3d, 0x27, 0x3a, 0x4c
	};
	std::array<std::uint8_t, mysql_native_password::response_length> expected {
		0xf1, 0xb2, 0xfb, 0x1c, 0x8d, 0xe7, 0x5d, 0xb8,
		0xeb, 0xa8, 0x12, 0x6a, 0xd1, 0x0f, 0xe9, 0xb1,
		0x10, 0x50, 0xd4, 0x28
	};
	std::array<std::uint8_t, mysql_native_password::response_length> actual {};
	const char* password = "root";
	mysql_native_password::compute_auth_string(password, challenge.data(), actual.data());
	EXPECT_EQ(expected, actual);
}

} // anon namespace

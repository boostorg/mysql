/*
 * error.cpp
 *
 *  Created on: Jan 19, 2020
 *      Author: ruben
 */

#include <gtest/gtest.h>
#include "boost/mysql/error.hpp"

using namespace testing;
using boost::mysql::errc;
using boost::mysql::detail::error_to_string;

TEST(errc, ErrorToString_Ok_ReturnsOk)
{
	EXPECT_STREQ(error_to_string(errc::ok), "no error");
}

TEST(errc, ErrorToString_MysqlAsioError_ReturnsDescription)
{
	EXPECT_STREQ(error_to_string(errc::sequence_number_mismatch), "Mismatched sequence numbers");
}

TEST(errc, ErrorToString_ServerError_ReturnsEnumName)
{
	EXPECT_STREQ(error_to_string(errc::bad_db_error), "bad_db_error");
}

TEST(errc, ErrorToString_UnknownError_ReturnsUnknown)
{
	EXPECT_STREQ(error_to_string(static_cast<errc>(0xfffefdfc)), "<unknown error>");
}

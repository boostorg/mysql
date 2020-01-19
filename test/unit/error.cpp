/*
 * error.cpp
 *
 *  Created on: Jan 19, 2020
 *      Author: ruben
 */

#include <gtest/gtest.h>
#include "mysql/error.hpp"

using namespace testing;
using mysql::Error;
using mysql::detail::error_to_string;

TEST(Error, ErrorToString_Ok_ReturnsOk)
{
	EXPECT_STREQ(error_to_string(Error::ok), "no error");
}

TEST(Error, ErrorToString_MysqlAsioError_ReturnsDescription)
{
	EXPECT_STREQ(error_to_string(Error::sequence_number_mismatch), "Mismatched sequence numbers");
}

TEST(Error, ErrorToString_ServerError_ReturnsEnumName)
{
	EXPECT_STREQ(error_to_string(Error::bad_db_error), "bad_db_error");
}

TEST(Error, ErrorToString_UnknownError_ReturnsUnknown)
{
	EXPECT_STREQ(error_to_string(static_cast<Error>(0xfffefdfc)), "<unknown error>");
}

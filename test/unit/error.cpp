//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <gtest/gtest.h>
#include "boost/mysql/error.hpp"

using namespace testing;
using boost::mysql::errc;
using boost::mysql::detail::error_to_string;
using boost::mysql::error_info;

// error_to_string
TEST(ErrcEnum, ErrorToString_Ok_ReturnsOk)
{
    EXPECT_STREQ(error_to_string(errc::ok), "No error");
}

TEST(ErrcEnum, ErrorToString_ClientError_ReturnsDescription)
{
    EXPECT_STREQ(error_to_string(errc::sequence_number_mismatch), "Mismatched sequence numbers");
}

TEST(ErrcEnum, ErrorToString_ServerError_ReturnsEnumName)
{
    EXPECT_STREQ(error_to_string(errc::bad_db_error), "bad_db_error");
}

TEST(ErrcEnum, ErrorToString_UnknownErrorOutOfRange_ReturnsUnknown)
{
    EXPECT_STREQ(error_to_string(static_cast<errc>(0xfffefdfc)), "<unknown error>");
}

TEST(ErrcEnum, ErrorToString_UnknownErrorServerRange_ReturnsUnknown)
{
    EXPECT_STREQ(error_to_string(static_cast<errc>(1009)), "<unknown error>");
}

TEST(ErrcEnum, ErrorToString_UnknownErrorBetweenServerAndClientRange_ReturnsUnknown)
{
    EXPECT_STREQ(error_to_string(static_cast<errc>(5000)), "<unknown error>");
}

// errro_info
TEST(ErrorInfo, OperatorEquals_Trivial_ComparesMessages)
{
    EXPECT_TRUE(error_info() == error_info());
    EXPECT_TRUE(error_info("abc") == error_info("abc"));
    EXPECT_FALSE(error_info() == error_info("abc"));
    EXPECT_FALSE(error_info("def") == error_info("abc"));
}

TEST(ErrorInfo, OperatorNotEquals_Trivial_ComparesMessages)
{
    EXPECT_FALSE(error_info() != error_info());
    EXPECT_FALSE(error_info("abc") != error_info("abc"));
    EXPECT_TRUE(error_info() != error_info("abc"));
    EXPECT_TRUE(error_info("def") != error_info("abc"));
}

TEST(ErrorInfo, OperatorStream_Trivial_StreamsMessage)
{
    std::ostringstream ss;
    ss << error_info("abc");
    EXPECT_EQ(ss.str(), "abc");
}

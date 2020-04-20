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

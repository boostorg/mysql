//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <gtest/gtest.h>
#include "boost/mysql/detail/auxiliar/static_string.hpp"

using boost::mysql::detail::static_string;
using boost::string_view;

namespace
{

struct StaticStringTest : public testing::Test
{
    static constexpr std::size_t max_size_value = 32;
    using string_type = static_string<max_size_value>;

    static std::string original_midsize() { return "abc"; }
    static std::string original_maxsize() { return std::string(max_size_value, 'a'); }

    std::string midsize = original_midsize();
    std::string maxsize = original_maxsize();

    void wipe_midsize() { midsize = "fff"; }
    void wipe_maxsize() { maxsize = std::string(max_size_value, 'f'); }
};

// Default ctor.
TEST_F(StaticStringTest, DefaultConstructor_Trivial_Empty)
{
    string_type v;
    EXPECT_EQ(v.value(), "");
}

// Init ctor.
TEST_F(StaticStringTest, InitializingConstructor_EmptyArg_Empty)
{
    string_type v ("");
    EXPECT_EQ(v.value(), "");
}

TEST_F(StaticStringTest, InitializingConstructor_MidSizeArg_CopiesIt)
{
    string_type v (midsize);
    wipe_midsize();
    EXPECT_EQ(v.value(), original_midsize());
}

TEST_F(StaticStringTest, InitializingConstructor_MaxSizeArg_CopiesIt)
{
    string_type v (maxsize);
    wipe_maxsize();
    EXPECT_EQ(v.value(), original_maxsize());
}

// Copy ctor.
TEST_F(StaticStringTest, CopyConstructor_EmptyArg_Empty)
{
    string_type v (string_type{}); // {} prevent deambiguation as function declaration
    EXPECT_EQ(v.value(), "");
}

TEST_F(StaticStringTest, CopyConstructor_MidSizeArg_CopiesIt)
{
    string_type v (string_type{midsize});
    wipe_midsize();
    EXPECT_EQ(v.value(), original_midsize());
}

TEST_F(StaticStringTest, CopyConstructor_MaxSizeArg_CopiesIt)
{
    string_type v (string_type{maxsize});
    wipe_maxsize();
    EXPECT_EQ(v.value(), original_maxsize());
}

// Copy assignment
TEST_F(StaticStringTest, CopyAssignment_EmptySource_Empty)
{
    string_type v (maxsize);
    v = string_type();
    EXPECT_EQ(v.value(), "");
}

TEST_F(StaticStringTest, CopyAssignment_MidSizeSource_CopiesIt)
{
    string_type v (maxsize);
    v = string_type(midsize);
    wipe_midsize();
    EXPECT_EQ(v.value(), original_midsize());
}

TEST_F(StaticStringTest, CopyAssignment_MaxSizeSource_CopiesIt)
{
    string_type v (midsize);
    v = string_type(maxsize);
    wipe_midsize();
    wipe_maxsize();
    EXPECT_EQ(v.value(), original_maxsize());
}

// operator==
TEST_F(StaticStringTest, OperatorEquals_BothEmpty_ReturnsTrue)
{
    EXPECT_TRUE(string_type() == string_type());
}

TEST_F(StaticStringTest, OperatorEquals_BothEmptyAfterClear_ReturnsTrue)
{
    string_type s1 ("abc");
    string_type s2 ("def");
    s1.clear();
    s2.clear();
    EXPECT_TRUE(s1 == s2);
}

TEST_F(StaticStringTest, OperatorEquals_OneEmptyOneNot_ReturnsFalse)
{
    EXPECT_FALSE(string_type() == string_type(midsize));
    EXPECT_FALSE(string_type(midsize) == string_type());
    EXPECT_FALSE(string_type() == string_type(maxsize));
    EXPECT_FALSE(string_type(maxsize) == string_type());
}

TEST_F(StaticStringTest, OperatorEquals_SameBeginningDifferentSize_ReturnsFalse)
{
    string_type s1 ("abcd");
    string_type s2 ("abcde");
    EXPECT_FALSE(s1 == s2);
    EXPECT_FALSE(s2 == s1);
}

TEST_F(StaticStringTest, OperatorEquals_SameSizeDifferentContents_ReturnsFalse)
{
    string_type s1 ("abcd");
    string_type s2 ("dcba");
    EXPECT_FALSE(s1 == s2);
    EXPECT_FALSE(s2 == s1);
}

TEST_F(StaticStringTest, OperatorEquals_SameContents_ReturnsTrue)
{
    EXPECT_TRUE(string_type(midsize) == string_type(midsize));
    EXPECT_TRUE(string_type(maxsize) == string_type(maxsize));
}

// operator !=
TEST_F(StaticStringTest, OperatorNotEquals_Equals_ReturnsFalse)
{
    EXPECT_FALSE(string_type() != string_type());
    EXPECT_FALSE(string_type(midsize) != string_type(midsize));
    EXPECT_FALSE(string_type(maxsize) != string_type(maxsize));
}

TEST_F(StaticStringTest, OperatorNotEquals_NotEquals_ReturnsTrue)
{
    EXPECT_TRUE(string_type() != string_type(midsize));
    EXPECT_TRUE(string_type("abc") != string_type("cba"));
    EXPECT_TRUE(string_type(midsize) != string_type(maxsize));
}

// clear
TEST_F(StaticStringTest, Clear_Empty_Empty)
{
    string_type v;
    v.clear();
    EXPECT_EQ(v.value(), "");
}

TEST_F(StaticStringTest, Clear_NotEmpty_Empty)
{
    string_type v (maxsize);
    v.clear();
    EXPECT_EQ(v.value(), "");
}

// append
TEST_F(StaticStringTest, Append_FromEmptyToEmpty_Empty)
{
    string_type v;
    v.append(midsize.data(), 0);
    wipe_midsize();
    EXPECT_EQ(v.value(), "");
}

TEST_F(StaticStringTest, Append_FromEmptyToMidsize_Copies)
{
    string_type v;
    v.append(midsize.data(), midsize.size());
    wipe_midsize();
    EXPECT_EQ(v.value(), original_midsize());
}

TEST_F(StaticStringTest, Append_FromEmptyToMaxsize_Copies)
{
    string_type v;
    v.append(maxsize.data(), maxsize.size());
    wipe_maxsize();
    EXPECT_EQ(v.value(), original_maxsize());
}

TEST_F(StaticStringTest, Append_FromMidsizeToMidsize_Copies)
{
    string_type v ("222");
    v.append(midsize.data(), midsize.size());
    wipe_midsize();
    EXPECT_EQ(v.value(), "222" + original_midsize());
}

TEST_F(StaticStringTest, Append_FromMidsizeToMaxsize_Copies)
{
    string_type v (midsize);
    std::string newbuff (max_size_value - midsize.size(), '1');
    v.append(newbuff.data(), newbuff.size());
    wipe_midsize();
    EXPECT_EQ(v.value(), original_midsize() + newbuff);
}

} // anon namespace


/*
 * row.cpp
 *
 *  Created on: Dec 7, 2019
 *      Author: ruben
 */

#include <gtest/gtest.h>
#include "boost/mysql/row.hpp"
#include "test_common.hpp"

using namespace mysql::test;
using namespace testing;
using mysql::row;

namespace
{

// Equality operators
TEST(RowTest, OperatorsEqNe_BothEmpty_ReturnEquals)
{
	EXPECT_TRUE(row() == row());
	EXPECT_FALSE(row() != row());
}

TEST(RowTest, OperatorsEqNe_OneEmptyOtherNotEmpty_ReturnNotEquals)
{
	row empty_row;
	row non_empty_row (makevalues("a_value"));
	EXPECT_FALSE(empty_row == non_empty_row);
	EXPECT_TRUE(empty_row != non_empty_row);
}

TEST(RowTest, OperatorsEqNe_Subset_ReturnNotEquals)
{
	row lhs (makevalues("a_value", 42));
	row rhs (makevalues("a_value"));
	EXPECT_FALSE(lhs == rhs);
	EXPECT_TRUE(lhs != rhs);
}

TEST(RowTest, OperatorsEqNe_SameSizeDifferentValues_ReturnNotEquals)
{
	row lhs (makevalues("a_value", 42));
	row rhs (makevalues("another_value", 42));
	EXPECT_FALSE(lhs == rhs);
	EXPECT_TRUE(lhs != rhs);
}

TEST(RowTest, OperatorsEqNe_SameSizeAndValues_ReturnEquals)
{
	row lhs (makevalues("a_value", 42));
	row rhs (makevalues("a_value", 42));
	EXPECT_TRUE(lhs == rhs);
	EXPECT_FALSE(lhs != rhs);
}

// Stream operators
std::string to_string(const row& input)
{
	std::ostringstream ss;
	ss << input;
	return ss.str();
}

TEST(RowTest, OperatorStream_EmptyRow)
{
	EXPECT_EQ(to_string(row()), "{}");
}

TEST(RowTest, OperatorStream_OneElement)
{
	EXPECT_EQ(to_string(makerow(42)), "{42}");
}

TEST(RowTest, OperatorStream_SeveralElements)
{
	EXPECT_EQ((to_string(makerow("value", nullptr))), "{value, <NULL>}");
	EXPECT_EQ((to_string(makerow("value", std::uint32_t(2019), 3.14f))), "{value, 2019, 3.14}");
}

}


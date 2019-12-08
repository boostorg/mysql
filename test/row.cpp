/*
 * row.cpp
 *
 *  Created on: Dec 7, 2019
 *      Author: ruben
 */

#include <gtest/gtest.h>
#include "mysql/row.hpp"
#include "test_common.hpp"

using namespace mysql;
using namespace mysql::test;
using namespace testing;

namespace
{

struct RowTest : public Test
{
};

TEST_F(RowTest, OperatorsEqNe_BothEmpty_ReturnEquals)
{
	EXPECT_TRUE(row() == row());
	EXPECT_FALSE(row() != row());
}

TEST_F(RowTest, OperatorsEqNe_OneEmptyOtherNotEmpty_ReturnNotEquals)
{
	row empty_row;
	row non_empty_row (makevalues("a_value"));
	EXPECT_FALSE(empty_row == non_empty_row);
	EXPECT_TRUE(empty_row != non_empty_row);
}

TEST_F(RowTest, OperatorsEqNe_Subset_ReturnNotEquals)
{
	row lhs (makevalues("a_value", 42));
	row rhs (makevalues("a_value"));
	EXPECT_FALSE(lhs == rhs);
	EXPECT_TRUE(lhs != rhs);
}

TEST_F(RowTest, OperatorsEqNe_SameSizeDifferentValues_ReturnNotEquals)
{
	row lhs (makevalues("a_value", 42));
	row rhs (makevalues("another_value", 42));
	EXPECT_FALSE(lhs == rhs);
	EXPECT_TRUE(lhs != rhs);
}

TEST_F(RowTest, OperatorsEqNe_SameSizeAndValues_ReturnEquals)
{
	row lhs (makevalues("a_value", 42));
	row rhs (makevalues("a_value", 42));
	EXPECT_TRUE(lhs == rhs);
	EXPECT_FALSE(lhs != rhs);
}

}


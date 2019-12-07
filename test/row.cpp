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
	// Metadata is not important, as comparisons should ignore it
	std::vector<field_metadata> meta0 {};
	std::vector<field_metadata> meta1 {
		field_metadata(detail::msgs::column_definition{})
	};
	std::vector<field_metadata> meta2 {
		field_metadata(detail::msgs::column_definition{}),
		field_metadata(detail::msgs::column_definition{})
	};
};

TEST_F(RowTest, OperatorsEqNe_BothDefaultConstructed_ReturnEquals)
{
	EXPECT_TRUE(row() == row());
	EXPECT_FALSE(row() != row());
}

TEST_F(RowTest, OperatorsEqNe_OneDefaultConstructedOtherEmpty_ReturnEquals)
{
	row empty_row ({}, meta0);
	EXPECT_TRUE(row() == empty_row);
	EXPECT_FALSE(row() != empty_row);
}

TEST_F(RowTest, OperatorsEqNe_BothEmpty_ReturnEquals)
{
	row empty_row0 ({}, meta0);
	row empty_row1 ({}, meta1);
	EXPECT_TRUE(empty_row0 == empty_row1);
	EXPECT_FALSE(empty_row0 != empty_row1);
}

TEST_F(RowTest, OperatorsEqNe_OneEmptyOtherNotEmpty_ReturnNotEquals)
{
	row empty_row ({}, meta0);
	row non_empty_row (makevalues("a_value"), meta1);
	EXPECT_FALSE(empty_row == non_empty_row);
	EXPECT_TRUE(empty_row != non_empty_row);
}

TEST_F(RowTest, OperatorsEqNe_Subset_ReturnNotEquals)
{
	row lhs (makevalues("a_value", 42), meta0);
	row rhs (makevalues("a_value"), meta1);
	EXPECT_FALSE(lhs == rhs);
	EXPECT_TRUE(lhs != rhs);
}

TEST_F(RowTest, OperatorsEqNe_SameSizeDifferentValues_ReturnNotEquals)
{
	row lhs (makevalues("a_value", 42), meta0);
	row rhs (makevalues("another_value", 42), meta1);
	EXPECT_FALSE(lhs == rhs);
	EXPECT_TRUE(lhs != rhs);
}

TEST_F(RowTest, OperatorsEqNe_SameSizeAndValues_ReturnEquals)
{
	row lhs (makevalues("a_value", 42), meta0);
	row rhs (makevalues("a_value", 42), meta1);
	EXPECT_TRUE(lhs == rhs);
	EXPECT_FALSE(lhs != rhs);
}

}


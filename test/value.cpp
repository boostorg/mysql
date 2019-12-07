/*
 * value.cpp
 *
 *  Created on: Dec 7, 2019
 *      Author: ruben
 */

#include <gtest/gtest.h>
#include "mysql/value.hpp"
#include "test_common.hpp"

using namespace mysql;
using namespace mysql::test;
using namespace testing;
using namespace ::date::literals;

namespace
{

struct ValuesTest : public Test
{
	std::vector<value> values = makevalues(
		std::int32_t(20),
		std::int64_t(-1),
		std::uint32_t(0xffffffff),
		std::uint64_t(0x100000000),
		3.14f,
		8.89,
		mysql::date(1_d/10/2019_y),
		mysql::date(1_d/10/2019_y) + std::chrono::hours(10),
		mysql::time(std::chrono::seconds(-10)),
		mysql::year(2010),
		nullptr
	);
	std::vector<value> values_copy = values;
	std::vector<value> other_values = makevalues(
		std::int32_t(10),
		std::int64_t(-22),
		std::uint32_t(0xff6723),
		std::uint64_t(222),
		-3.0f,
		8e24,
		mysql::date(1_d/9/2019_y),
		mysql::date(1_d/9/2019_y) + std::chrono::hours(10),
		mysql::time(std::chrono::seconds(10)),
		mysql::year(1900),
		nullptr
	);
};

TEST_F(ValuesTest, OperatorsEqNe_DifferentType_ReturnNotEquals)
{
	for (int i = 0; i < values.size(); ++i)
	{
		for (int j = 0; j < i; ++j)
		{
			EXPECT_FALSE(values.at(i) == values.at(j)) << "i=" << i << ", j=" << j;
			EXPECT_TRUE(values.at(i) != values.at(j))  << "i=" << i << ", j=" << j;
		}
	}
}

TEST_F(ValuesTest, OperatorsEqNe_SameTypeDifferentValue_ReturnNotEquals)
{
	// Note: nullptr_t (the last value) can't have other value than nullptr
	// so it is excluded from this test
	for (int i = 0; i < values.size() - 1; ++i)
	{
		EXPECT_FALSE(values.at(i) == other_values.at(i)) << "i=" << i;
		EXPECT_TRUE(values.at(i) != other_values.at(i))  << "i=" << i;
	}
}

TEST_F(ValuesTest, OperatorsEqNe_SameTypeSameValue_ReturnEquals)
{
	for (int i = 0; i < values.size(); ++i)
	{
		EXPECT_TRUE(values.at(i) == values_copy.at(i))  << "i=" << i;
		EXPECT_FALSE(values.at(i) != values_copy.at(i)) << "i=" << i;
	}
}

}

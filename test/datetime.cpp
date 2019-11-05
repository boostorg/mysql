/*
 * datetime.cpp
 *
 *  Created on: Nov 2, 2019
 *      Author: ruben
 */

#include "mysql/datetime.hpp"
#include <gtest/gtest.h>

using namespace testing;
using namespace mysql;

TEST(Datetime, DefaultConstructor_Trivial_AllZeros)
{
	datetime dt;
	EXPECT_EQ(dt.year(), 0);
	EXPECT_EQ(dt.month(), 0);
	EXPECT_EQ(dt.day(), 0);
	EXPECT_EQ(dt.hour(), 0);
	EXPECT_EQ(dt.minute(), 0);
	EXPECT_EQ(dt.second(), 0);
	EXPECT_EQ(dt.microsecond(), 0);
}

TEST(Datetime, InitializationConstructor_Trivial_SetsAllMembers)
{
	datetime dt (2010, 10, 9, 22, 50, 55, 2090);
	EXPECT_EQ(dt.year(), 2010);
	EXPECT_EQ(dt.month(), 10);
	EXPECT_EQ(dt.day(), 9);
	EXPECT_EQ(dt.hour(), 22);
	EXPECT_EQ(dt.minute(), 50);
	EXPECT_EQ(dt.second(), 55);
	EXPECT_EQ(dt.microsecond(), 2090);
}

TEST(Datetime, InitializationConstructor_DefaultArgs_SetsThemToZero)
{
	datetime dt (2010, 10, 9);
	EXPECT_EQ(dt.year(), 2010);
	EXPECT_EQ(dt.month(), 10);
	EXPECT_EQ(dt.day(), 9);
	EXPECT_EQ(dt.hour(), 0);
	EXPECT_EQ(dt.minute(), 0);
	EXPECT_EQ(dt.second(), 0);
	EXPECT_EQ(dt.microsecond(), 0);
}

TEST(Datetime, Setters_Trivial_SetRelevantMember)
{
	datetime dt;
	dt.set_year(2010);
	dt.set_month(10);
	dt.set_day(9);
	dt.set_hour(22);
	dt.set_minute(50);
	dt.set_second(55);
	dt.set_microsecond(2090);
	EXPECT_EQ(dt.year(), 2010);
	EXPECT_EQ(dt.month(), 10);
	EXPECT_EQ(dt.day(), 9);
	EXPECT_EQ(dt.hour(), 22);
	EXPECT_EQ(dt.minute(), 50);
	EXPECT_EQ(dt.second(), 55);
	EXPECT_EQ(dt.microsecond(), 2090);
}

TEST(Datetime, OperatorEquals_AllMembersEqual_ReturnsTrue)
{
	EXPECT_EQ(datetime(), datetime());
	EXPECT_EQ((datetime(2010, 9, 10)), (datetime(2010, 9, 10)));
	EXPECT_EQ((datetime(2010, 9, 10, 22, 59, 60, 9999)), (datetime(2010, 9, 10, 22, 59, 60, 9999)));
}

TEST(Datetime, OperatorEquals_AnyNonEqualMember_ReturnsFalse)
{
	EXPECT_FALSE((datetime(2010, 0, 0)) == datetime());
	EXPECT_FALSE((datetime(2010, 9, 10)) == (datetime(2010, 10, 10)));
	EXPECT_FALSE((datetime(2010, 9, 10, 22, 59, 60, 9999)) == (datetime(2011, 9, 10, 22, 59, 60, 9999)));
	EXPECT_FALSE((datetime(2010, 9, 10, 22, 59, 60, 9999)) == (datetime(2010, 8, 10, 22, 59, 60, 9999)));
	EXPECT_FALSE((datetime(2010, 9, 10, 22, 59, 60, 9999)) == (datetime(2010, 9, 11, 22, 59, 60, 9999)));
	EXPECT_FALSE((datetime(2010, 9, 10, 22, 59, 60, 9999)) == (datetime(2010, 9, 10, 23, 59, 60, 9999)));
	EXPECT_FALSE((datetime(2010, 9, 10, 22, 59, 60, 9999)) == (datetime(2010, 9, 10, 22, 55, 60, 9999)));
	EXPECT_FALSE((datetime(2010, 9, 10, 22, 59, 60, 9999)) == (datetime(2010, 9, 10, 22, 59, 59, 9999)));
	EXPECT_FALSE((datetime(2010, 9, 10, 22, 59, 60, 9999)) == (datetime(2010, 9, 10, 22, 59, 60, 88)));
}

TEST(Datetime, OperatorNotEquals_Trivial_ReturnsNotEquals)
{
	EXPECT_TRUE(datetime() != datetime(2010, 0, 0));
	EXPECT_FALSE(datetime(2010, 0, 0) != datetime(2010, 0, 0));
}




/*
 * capabilities.cpp
 *
 *  Created on: Oct 21, 2019
 *      Author: ruben
 */

#include "boost/mysql/detail/protocol/capabilities.hpp"
#include <gtest/gtest.h>

using namespace boost::mysql::detail;
using namespace testing;

TEST(Capabilities, Has_BitSet_ReturnsTrue)
{
	capabilities caps (CLIENT_COMPRESS);
	EXPECT_TRUE(caps.has(CLIENT_COMPRESS));
}

TEST(Capabilities, Has_BitNotSet_ReturnsFalse)
{
	capabilities caps (CLIENT_COMPRESS);
	EXPECT_FALSE(caps.has(CLIENT_SSL));
}

TEST(Capabilities, Has_MultipleBitsSet_ReturnsTrueForSetBits)
{
	capabilities caps (
			CLIENT_CONNECT_WITH_DB |
			CLIENT_SSL |
			CLIENT_COMPRESS);
	for (int i = 0; i < 32; ++i)
	{
		std::uint32_t cap_bit = 1 << i;
		bool is_set =
			cap_bit == CLIENT_CONNECT_WITH_DB ||
			cap_bit == CLIENT_SSL ||
			cap_bit == CLIENT_COMPRESS;
		EXPECT_EQ(caps.has(cap_bit), is_set);
	}
}

struct CapabilitiesHasAllTest : public Test
{
	capabilities rhs {CLIENT_CONNECT_WITH_DB | CLIENT_SSL | CLIENT_COMPRESS};
};

TEST_F(CapabilitiesHasAllTest, HasAll_HasNone_ReturnsFalse)
{
	capabilities lhs (0);
	EXPECT_FALSE(lhs.has_all(rhs));
}

TEST_F(CapabilitiesHasAllTest, HasAll_HasSomeButNotAll_ReturnsFalse)
{
	capabilities lhs (CLIENT_CONNECT_WITH_DB | CLIENT_COMPRESS);
	EXPECT_FALSE(lhs.has_all(rhs));
}

TEST_F(CapabilitiesHasAllTest, HasAll_HasSomeButNotAllPlusUnrelated_ReturnsFalse)
{
	capabilities lhs (CLIENT_CONNECT_WITH_DB | CLIENT_COMPRESS | CLIENT_TRANSACTIONS);
	EXPECT_FALSE(lhs.has_all(rhs));
}

TEST_F(CapabilitiesHasAllTest, HasAll_HasOnlyTheRequestedOnes_ReturnsTrue)
{
	capabilities lhs (rhs);
	EXPECT_TRUE(lhs.has_all(rhs));
}

TEST_F(CapabilitiesHasAllTest, HasAll_HasTheRequestedOnesAndOthers_ReturnsTrue)
{
	capabilities lhs = rhs | capabilities(CLIENT_TRANSACTIONS);
	EXPECT_TRUE(lhs.has_all(rhs));
}

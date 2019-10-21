/*
 * capabilities.cpp
 *
 *  Created on: Oct 21, 2019
 *      Author: ruben
 */

#include "mysql/impl/capabilities.hpp"
#include <gtest/gtest.h>

using namespace mysql::detail;
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

TEST(Capabilities, HasMandatoryCapabilities_MissingMandatoryCapability_ReturnsFalse)
{
	EXPECT_FALSE(has_mandatory_capabilities(capabilities(0)));
	EXPECT_FALSE(has_mandatory_capabilities(capabilities(CLIENT_PROTOCOL_41)));
	EXPECT_FALSE(has_mandatory_capabilities(capabilities(
		CLIENT_PROTOCOL_41 |
		CLIENT_PLUGIN_AUTH |
		CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA
	)));
	EXPECT_FALSE(has_mandatory_capabilities(capabilities(
		CLIENT_PROTOCOL_41 |
		CLIENT_PLUGIN_AUTH |
		CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA |
		CLIENT_SSL |
		CLIENT_COMPRESS
	)));
}

TEST(Capabilities, HasMandatoryCapabilities_HasAllMandatoryCapabilities_ReturnsTrue)
{
	EXPECT_TRUE(has_mandatory_capabilities(capabilities(
		CLIENT_PROTOCOL_41 |
		CLIENT_PLUGIN_AUTH |
		CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA |
		CLIENT_DEPRECATE_EOF
	)));
	EXPECT_TRUE(has_mandatory_capabilities(capabilities(
		CLIENT_PROTOCOL_41 |
		CLIENT_PLUGIN_AUTH |
		CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA |
		CLIENT_DEPRECATE_EOF |
		CLIENT_SSL |
		CLIENT_COMPRESS
	)));
}

TEST(Capabilities, CalculateCapabilities_OnlyMandatory_ReturnsMandatory)
{
	capabilities server_caps (
		CLIENT_PROTOCOL_41 |
		CLIENT_PLUGIN_AUTH |
		CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA |
		CLIENT_DEPRECATE_EOF
	);
	auto client_caps = calculate_capabilities(server_caps);
	EXPECT_EQ(server_caps, client_caps);
}

TEST(Capabilities, CalculateCapabilities_MandatoryOptional_ReturnsMandatoryAndOptional)
{
	capabilities server_caps (
		CLIENT_PROTOCOL_41 |
		CLIENT_PLUGIN_AUTH |
		CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA |
		CLIENT_DEPRECATE_EOF |
		CLIENT_CONNECT_WITH_DB
	);
	auto client_caps = calculate_capabilities(server_caps);
	EXPECT_EQ(server_caps, client_caps);
}

TEST(Capabilities, CalculateCapabilities_MandatoryOptionalUnknown_ReturnsMandatoryAndOptional)
{
	capabilities server_caps (
		CLIENT_PROTOCOL_41 |
		CLIENT_PLUGIN_AUTH |
		CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA |
		CLIENT_DEPRECATE_EOF |
		CLIENT_CONNECT_WITH_DB |
		CLIENT_LOCAL_FILES
	);
	capabilities expected (
		CLIENT_PROTOCOL_41 |
		CLIENT_PLUGIN_AUTH |
		CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA |
		CLIENT_DEPRECATE_EOF |
		CLIENT_CONNECT_WITH_DB
	);
	auto client_caps = calculate_capabilities(server_caps);
	EXPECT_EQ(expected, client_caps);
}


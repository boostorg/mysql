/*
 * deserialize_row.cpp
 *
 *  Created on: Nov 8, 2019
 *      Author: ruben
 */

#include <gtest/gtest.h>
#include "mysql/impl/deserialize_row.hpp"

using namespace mysql;
using namespace mysql::detail;
using namespace testing;

namespace
{

struct TextValueParam
{
	std::string name;
	std::string_view from;
	value expected;
	field_type type;
	unsigned decimals;
	bool unsign;

	template <typename T>
	TextValueParam(
		std::string name,
		std::string_view from,
		T&& expected_value,
		field_type type,
		bool unsign=false,
		unsigned decimals=0
	):
		name(std::move(name)),
		from(from),
		expected(std::forward<T>(expected_value)),
		type(type),
		decimals(decimals),
		unsign(unsign)
	{
	};
};

std::ostream& operator<<(std::ostream& os, const TextValueParam& value) { return os << value.name; }

struct DeserializeTextValueTest : public TestWithParam<TextValueParam>
{
};

TEST_P(DeserializeTextValueTest, CorrectFormat_SetsOutputValueReturnsTrue)
{
	msgs::column_definition coldef;
	coldef.type = GetParam().type;
	coldef.decimals.value = static_cast<std::uint8_t>(GetParam().decimals);
	coldef.flags.value = GetParam().unsign ? column_flags::unsigned_ : 0;
	field_metadata meta (coldef);
	value actual_value;
	auto err = deserialize_text_value(GetParam().from, meta, actual_value);
	EXPECT_EQ(err, Error::ok);
	EXPECT_EQ(actual_value, GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(VARCHAR, DeserializeTextValueTest, Values(
	TextValueParam("non-empty", "string", "string", field_type::var_string),
	TextValueParam("empty", "", "", field_type::var_string)
));

INSTANTIATE_TEST_SUITE_P(TINYINT, DeserializeTextValueTest, Values(
	TextValueParam("signed", "20", std::int32_t(20), field_type::tiny),
	TextValueParam("signed max", "127", std::int32_t(127), field_type::tiny),
	TextValueParam("signed negative", "-20", std::int32_t(-20), field_type::tiny),
	TextValueParam("signed negative max", "-128", std::int32_t(-128), field_type::tiny),
	TextValueParam("unsigned", "20", std::uint32_t(20), field_type::tiny, true),
	TextValueParam("usigned min", "0", std::uint32_t(0), field_type::tiny, true),
	TextValueParam("usigned max", "255", std::uint32_t(255), field_type::tiny, true)
));

INSTANTIATE_TEST_SUITE_P(SMALLINT, DeserializeTextValueTest, Values(
	TextValueParam("signed", "20", std::int32_t(20), field_type::short_),
	TextValueParam("signed max", "32767", std::int32_t(32767), field_type::short_),
	TextValueParam("signed negative", "-20", std::int32_t(-20), field_type::short_),
	TextValueParam("signed negative max", "-32768", std::int32_t(-32768), field_type::short_),
	TextValueParam("unsigned", "20", std::uint32_t(20), field_type::short_, true),
	TextValueParam("usigned min", "0", std::uint32_t(0), field_type::short_, true),
	TextValueParam("usigned max", "65535", std::uint32_t(65535), field_type::short_, true)
));

INSTANTIATE_TEST_SUITE_P(MEDIUMINT, DeserializeTextValueTest, Values(
	TextValueParam("signed", "20", std::int32_t(20), field_type::int24),
	TextValueParam("signed max", "8388607", std::int32_t(8388607), field_type::int24),
	TextValueParam("signed negative", "-20", std::int32_t(-20), field_type::int24),
	TextValueParam("signed negative max", "-8388607", std::int32_t(-8388607), field_type::int24),
	TextValueParam("unsigned", "20", std::uint32_t(20), field_type::int24, true),
	TextValueParam("usigned min", "0", std::uint32_t(0), field_type::int24, true),
	TextValueParam("usigned max", "16777215", std::uint32_t(16777215), field_type::int24, true)
));

INSTANTIATE_TEST_SUITE_P(YEAR, DeserializeTextValueTest, Values(
	TextValueParam("regular value", "1999", year(1999), field_type::year),
	TextValueParam("min", "1901", year(1901), field_type::year),
	TextValueParam("max", "2155", year(2155), field_type::year),
	TextValueParam("zero", "0000", year(0), field_type::year)
));

}



/*
 * metadata_validator.cpp
 *
 *  Created on: Nov 17, 2019
 *      Author: ruben
 */

#include "metadata_validator.hpp"
#include <gtest/gtest.h>

using namespace boost::mysql::test;

#define MYSQL_TEST_FLAG_GETTER_NAME_ENTRY(getter) \
	{ #getter, &boost::mysql::field_metadata::getter }

static struct flag_entry
{
	const char* name;
	meta_validator::flag_getter getter;
} flag_names [] = {
	MYSQL_TEST_FLAG_GETTER_NAME_ENTRY(is_not_null),
	MYSQL_TEST_FLAG_GETTER_NAME_ENTRY(is_primary_key),
	MYSQL_TEST_FLAG_GETTER_NAME_ENTRY(is_unique_key),
	MYSQL_TEST_FLAG_GETTER_NAME_ENTRY(is_multiple_key),
	MYSQL_TEST_FLAG_GETTER_NAME_ENTRY(is_unsigned),
	MYSQL_TEST_FLAG_GETTER_NAME_ENTRY(is_zerofill),
	MYSQL_TEST_FLAG_GETTER_NAME_ENTRY(is_auto_increment),
	MYSQL_TEST_FLAG_GETTER_NAME_ENTRY(has_no_default_value),
	MYSQL_TEST_FLAG_GETTER_NAME_ENTRY(is_set_to_now_on_update)
};

static bool contains(
	meta_validator::flag_getter flag,
	const std::vector<meta_validator::flag_getter>& flagvec
)
{
	return std::find(flagvec.begin(), flagvec.end(), flag) != flagvec.end();
}

void meta_validator::validate(
	const mysql::field_metadata& value
) const
{
	// Fixed fields
	EXPECT_EQ(value.database(), "awesome");
	EXPECT_EQ(value.table(), table_);
	EXPECT_EQ(value.original_table(), org_table_);
	EXPECT_EQ(value.field_name(), field_);
	EXPECT_EQ(value.original_field_name(), org_field_);
	EXPECT_GT(value.column_length(), 0u);
	EXPECT_EQ(value.type(), type_);
	EXPECT_EQ(value.decimals(), decimals_);

	// Flags
	std::vector<flag_entry> all_flags (std::begin(flag_names), std::end(flag_names));

	for (flag_getter true_flag: flags_)
	{
		auto it = std::find_if(
			all_flags.begin(),
			all_flags.end(),
			[true_flag](const flag_entry& entry) { return entry.getter == true_flag; }
		);
		ASSERT_NE(it, all_flags.end()); // no repeated flag
		ASSERT_FALSE(contains(true_flag, ignore_flags_)); // ignore flags cannot be set to true
		EXPECT_TRUE((value.*true_flag)()) << it->name;
		all_flags.erase(it);
	}

	for (const auto& entry: all_flags)
	{
		if (!contains(entry.getter, ignore_flags_))
		{
			EXPECT_FALSE((value.*entry.getter)()) << entry.name;
		}
	}
}

void boost::mysql::test::validate_meta(
	const std::vector<field_metadata>& actual,
	const std::vector<meta_validator>& expected
)
{
	ASSERT_EQ(actual.size(), expected.size());
	for (std::size_t i = 0; i < actual.size(); ++i)
	{
		expected[i].validate(actual[i]);
	}
}


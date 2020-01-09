/*
 * metadata.cpp
 *
 *  Created on: Oct 27, 2019
 *      Author: ruben
 */

#include "mysql/metadata.hpp"
#include <gtest/gtest.h>
#include "mysql/impl/serialization.hpp"

using namespace testing;
using namespace mysql;
using namespace detail;

namespace
{

TEST(FieldMetadata, IntPrimaryKey)
{
	msgs::column_definition msg {
		{"def"},
		{"awesome"},
		{"test_table"},
		{"test_table"},
		{"id"},
		{"id"},
		collation::binary,
		{11},
		protocol_field_type::long_,
		{column_flags::pri_key | column_flags::auto_increment | column_flags::not_null},
		{0}
	};
	field_metadata meta (msg);

	EXPECT_EQ(meta.database(), "awesome");
	EXPECT_EQ(meta.table(), "test_table");
	EXPECT_EQ(meta.original_table(), "test_table");
	EXPECT_EQ(meta.field_name(), "id");
	EXPECT_EQ(meta.original_field_name(), "id");
	EXPECT_EQ(meta.column_length(), 11);
	EXPECT_EQ(meta.type(), field_type::int_);
	EXPECT_EQ(meta.protocol_type(), protocol_field_type::long_);
	EXPECT_EQ(meta.decimals(), 0);
    EXPECT_TRUE(meta.is_not_null());
    EXPECT_TRUE(meta.is_primary_key());
    EXPECT_FALSE(meta.is_unique_key());
    EXPECT_FALSE(meta.is_multiple_key());
    EXPECT_FALSE(meta.is_unsigned());
    EXPECT_FALSE(meta.is_zerofill());
    EXPECT_TRUE(meta.is_auto_increment());
    EXPECT_FALSE(meta.has_no_default_value());
    EXPECT_FALSE(meta.is_set_to_now_on_update());
}

TEST(FieldMetadata, VarcharWithAlias)
{
	msgs::column_definition msg {
		{"def"},
		{"awesome"},
		{"child"},
		{"child_table"},
		{"field_alias"},
		{"field_varchar"},
		collation::utf8_general_ci,
		{765},
		protocol_field_type::var_string,
		{0},
		{0}
	};
	field_metadata meta (msg);

	EXPECT_EQ(meta.database(), "awesome");
	EXPECT_EQ(meta.table(), "child");
	EXPECT_EQ(meta.original_table(), "child_table");
	EXPECT_EQ(meta.field_name(), "field_alias");
	EXPECT_EQ(meta.original_field_name(), "field_varchar");
	EXPECT_EQ(meta.column_length(), 765);
	EXPECT_EQ(meta.protocol_type(), protocol_field_type::var_string);
	EXPECT_EQ(meta.type(), field_type::varchar);
	EXPECT_EQ(meta.decimals(), 0);
	EXPECT_FALSE(meta.is_not_null());
	EXPECT_FALSE(meta.is_primary_key());
	EXPECT_FALSE(meta.is_unique_key());
	EXPECT_FALSE(meta.is_multiple_key());
	EXPECT_FALSE(meta.is_unsigned());
	EXPECT_FALSE(meta.is_zerofill());
	EXPECT_FALSE(meta.is_auto_increment());
	EXPECT_FALSE(meta.has_no_default_value());
	EXPECT_FALSE(meta.is_set_to_now_on_update());
}

TEST(FieldMetadata, FloatField)
{
	msgs::column_definition msg {
		{"def"},
		{"awesome"},
		{"test_table"},
		{"test_table"},
		{"field_float"},
		{"field_float"},
		collation::binary,
		{12},
		protocol_field_type::float_,
		{0},
		{31}
	};
	field_metadata meta (msg);

	EXPECT_EQ(meta.database(), "awesome");
	EXPECT_EQ(meta.table(), "test_table");
	EXPECT_EQ(meta.original_table(), "test_table");
	EXPECT_EQ(meta.field_name(), "field_float");
	EXPECT_EQ(meta.original_field_name(), "field_float");
	EXPECT_EQ(meta.column_length(), 12);
	EXPECT_EQ(meta.protocol_type(), protocol_field_type::float_);
	EXPECT_EQ(meta.type(), field_type::float_);
	EXPECT_EQ(meta.decimals(), 31);
    EXPECT_FALSE(meta.is_not_null());
    EXPECT_FALSE(meta.is_primary_key());
    EXPECT_FALSE(meta.is_unique_key());
    EXPECT_FALSE(meta.is_multiple_key());
    EXPECT_FALSE(meta.is_unsigned());
    EXPECT_FALSE(meta.is_zerofill());
    EXPECT_FALSE(meta.is_auto_increment());
    EXPECT_FALSE(meta.has_no_default_value());
    EXPECT_FALSE(meta.is_set_to_now_on_update());

}

} // anon namespace

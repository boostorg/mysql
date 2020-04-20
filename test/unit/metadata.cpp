//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/metadata.hpp"
#include <gtest/gtest.h>
#include "boost/mysql/detail/protocol/serialization.hpp"

using namespace testing;
using namespace boost::mysql::detail;
using boost::mysql::collation;
using boost::mysql::field_metadata;
using boost::mysql::field_type;

namespace
{

TEST(FieldMetadata, IntPrimaryKey)
{
    column_definition_packet msg {
        string_lenenc("def"),
        string_lenenc("awesome"),
        string_lenenc("test_table"),
        string_lenenc("test_table"),
        string_lenenc("id"),
        string_lenenc("id"),
        collation::binary,
        int4(11),
        protocol_field_type::long_,
        int2(column_flags::pri_key | column_flags::auto_increment | column_flags::not_null),
        int1(0)
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
    column_definition_packet msg {
        string_lenenc("def"),
        string_lenenc("awesome"),
        string_lenenc("child"),
        string_lenenc("child_table"),
        string_lenenc("field_alias"),
        string_lenenc("field_varchar"),
        collation::utf8_general_ci,
        int4(765),
        protocol_field_type::var_string,
        int2(0),
        int1(0)
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
    column_definition_packet msg {
        string_lenenc("def"),
        string_lenenc("awesome"),
        string_lenenc("test_table"),
        string_lenenc("test_table"),
        string_lenenc("field_float"),
        string_lenenc("field_float"),
        collation::binary,
        int4(12),
        protocol_field_type::float_,
        int2(0),
        int1(31)
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

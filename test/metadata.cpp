/*
 * metadata.cpp
 *
 *  Created on: Oct 27, 2019
 *      Author: ruben
 */

#include "mysql/metadata.hpp"
#include "mysql/impl/basic_serialization.hpp"
#include <gtest/gtest.h>

using namespace testing;
using namespace mysql;
using namespace detail;

namespace
{

// Note: these packets are the same to those in the (de)serialization tests
// for column_definition
struct FieldMetadataTest : public Test
{
	using metadata_type = owning_field_metadata<std::allocator<std::uint8_t>>;

	metadata_type make_metadata(std::vector<std::uint8_t>&& buffer) const
	{
		DeserializationContext ctx (boost::asio::buffer(buffer), capabilities());
		msgs::column_definition column;
		auto err = deserialize_message(column, ctx);
		if (err) throw boost::system::system_error(err);

		metadata_type meta (std::move(buffer), column);
		buffer.resize(20, 0); // Try to catch any dangling pointer error

		return meta;
	}

	metadata_type make_numeric_primary_key_metadata() const
	{
		return make_metadata({
			0x03, 0x64, 0x65, 0x66, 0x07, 0x61, 0x77, 0x65,
			0x73, 0x6f, 0x6d, 0x65, 0x0a, 0x74, 0x65, 0x73,
			0x74, 0x5f, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x0a,
			0x74, 0x65, 0x73, 0x74, 0x5f, 0x74, 0x61, 0x62,
			0x6c, 0x65, 0x02, 0x69, 0x64, 0x02, 0x69, 0x64,
			0x0c, 0x3f, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x03,
			0x03, 0x42, 0x00, 0x00, 0x00
		});
	}

	void verify_numeric_primary_key(const metadata_type& meta)
	{
		EXPECT_EQ(meta.database(), "awesome");
		EXPECT_EQ(meta.table(), "test_table");
		EXPECT_EQ(meta.original_table(), "test_table");
		EXPECT_EQ(meta.field_name(), "id");
		EXPECT_EQ(meta.original_field_name(), "id");
		EXPECT_EQ(meta.field_collation(), collation::binary);
		EXPECT_EQ(meta.column_length(), 11);
		EXPECT_EQ(meta.type(), field_type::long_);
		EXPECT_EQ(meta.decimals(), 0);
	    EXPECT_TRUE(meta.is_not_null());
	    EXPECT_TRUE(meta.is_primary_key());
	    EXPECT_FALSE(meta.is_unique_key());
	    EXPECT_FALSE(meta.is_multiple_key());
	    EXPECT_FALSE(meta.is_blob());
	    EXPECT_FALSE(meta.is_unsigned());
	    EXPECT_FALSE(meta.is_zerofill());
	    EXPECT_FALSE(meta.is_binary());
	    EXPECT_FALSE(meta.is_enum());
	    EXPECT_TRUE(meta.is_auto_increment());
	    EXPECT_FALSE(meta.is_timestamp());
	    EXPECT_FALSE(meta.is_set());
	    EXPECT_FALSE(meta.has_no_default_value());
	    EXPECT_FALSE(meta.is_set_to_now_on_update());
	}

	// Varchar field, aliased field and table names (query with a JOIN)
	metadata_type make_joined_varchar_metadata()
	{
		return make_metadata({
			0x03, 0x64, 0x65, 0x66, 0x07, 0x61, 0x77, 0x65,
			0x73, 0x6f, 0x6d, 0x65, 0x05, 0x63, 0x68, 0x69,
			0x6c, 0x64, 0x0b, 0x63, 0x68, 0x69, 0x6c, 0x64,
			0x5f, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x0b, 0x66,
			0x69, 0x65, 0x6c, 0x64, 0x5f, 0x61, 0x6c, 0x69,
			0x61, 0x73, 0x0d, 0x66, 0x69, 0x65, 0x6c, 0x64,
			0x5f, 0x76, 0x61, 0x72, 0x63, 0x68, 0x61, 0x72,
			0x0c, 0x21, 0x00, 0xfd, 0x02, 0x00, 0x00, 0xfd,
			0x00, 0x00, 0x00, 0x00, 0x00
		});
	}
};

TEST_F(FieldMetadataTest, InitializingAndMoveConstructor_NumericPrimaryKey)
{
	auto meta = make_numeric_primary_key_metadata();
	verify_numeric_primary_key(meta);
}

TEST_F(FieldMetadataTest, MoveConstructor_NumericPrimaryKey)
{
	auto meta_temp = make_numeric_primary_key_metadata();
	metadata_type meta (std::move(meta_temp));
	verify_numeric_primary_key(meta);
}

TEST_F(FieldMetadataTest, MoveAssignment_NumericPrimaryKey)
{
	metadata_type meta;
	meta = make_numeric_primary_key_metadata();
	verify_numeric_primary_key(meta);
}

TEST_F(FieldMetadataTest, InitializingAndMoveConstructor_VarcharWithAlias)
{
	auto meta = make_metadata({
		0x03, 0x64, 0x65, 0x66, 0x07, 0x61, 0x77, 0x65,
		0x73, 0x6f, 0x6d, 0x65, 0x05, 0x63, 0x68, 0x69,
		0x6c, 0x64, 0x0b, 0x63, 0x68, 0x69, 0x6c, 0x64,
		0x5f, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x0b, 0x66,
		0x69, 0x65, 0x6c, 0x64, 0x5f, 0x61, 0x6c, 0x69,
		0x61, 0x73, 0x0d, 0x66, 0x69, 0x65, 0x6c, 0x64,
		0x5f, 0x76, 0x61, 0x72, 0x63, 0x68, 0x61, 0x72,
		0x0c, 0x21, 0x00, 0xfd, 0x02, 0x00, 0x00, 0xfd,
		0x00, 0x00, 0x00, 0x00, 0x00
	});

	EXPECT_EQ(meta.database(), "awesome");
	EXPECT_EQ(meta.table(), "child");
	EXPECT_EQ(meta.original_table(), "child_table");
	EXPECT_EQ(meta.field_name(), "field_alias");
	EXPECT_EQ(meta.original_field_name(), "field_varchar");
	EXPECT_EQ(meta.field_collation(), collation::utf8_general_ci);
	EXPECT_EQ(meta.column_length(), 765);
	EXPECT_EQ(meta.type(), field_type::var_string);
	EXPECT_EQ(meta.decimals(), 0);
    EXPECT_FALSE(meta.is_not_null());
    EXPECT_FALSE(meta.is_primary_key());
    EXPECT_FALSE(meta.is_unique_key());
    EXPECT_FALSE(meta.is_multiple_key());
    EXPECT_FALSE(meta.is_blob());
    EXPECT_FALSE(meta.is_unsigned());
    EXPECT_FALSE(meta.is_zerofill());
    EXPECT_FALSE(meta.is_binary());
    EXPECT_FALSE(meta.is_enum());
    EXPECT_FALSE(meta.is_auto_increment());
    EXPECT_FALSE(meta.is_timestamp());
    EXPECT_FALSE(meta.is_set());
    EXPECT_FALSE(meta.has_no_default_value());
    EXPECT_FALSE(meta.is_set_to_now_on_update());
}

TEST_F(FieldMetadataTest, InitializingAndMoveConstructor_Float)
{
	auto meta = make_metadata({
		0x03, 0x64, 0x65, 0x66, 0x07, 0x61, 0x77, 0x65,
		0x73, 0x6f, 0x6d, 0x65, 0x0a, 0x74, 0x65, 0x73,
		0x74, 0x5f, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x0a,
		0x74, 0x65, 0x73, 0x74, 0x5f, 0x74, 0x61, 0x62,
		0x6c, 0x65, 0x0b, 0x66, 0x69, 0x65, 0x6c, 0x64,
		0x5f, 0x66, 0x6c, 0x6f, 0x61, 0x74, 0x0b, 0x66,
		0x69, 0x65, 0x6c, 0x64, 0x5f, 0x66, 0x6c, 0x6f,
		0x61, 0x74, 0x0c, 0x3f, 0x00, 0x0c, 0x00, 0x00,
		0x00, 0x04, 0x00, 0x00, 0x1f, 0x00, 0x00
	});

	EXPECT_EQ(meta.database(), "awesome");
	EXPECT_EQ(meta.table(), "test_table");
	EXPECT_EQ(meta.original_table(), "test_table");
	EXPECT_EQ(meta.field_name(), "field_float");
	EXPECT_EQ(meta.original_field_name(), "field_float");
	EXPECT_EQ(meta.field_collation(), collation::binary);
	EXPECT_EQ(meta.column_length(), 12);
	EXPECT_EQ(meta.type(), field_type::float_);
	EXPECT_EQ(meta.decimals(), 31);
    EXPECT_FALSE(meta.is_not_null());
    EXPECT_FALSE(meta.is_primary_key());
    EXPECT_FALSE(meta.is_unique_key());
    EXPECT_FALSE(meta.is_multiple_key());
    EXPECT_FALSE(meta.is_blob());
    EXPECT_FALSE(meta.is_unsigned());
    EXPECT_FALSE(meta.is_zerofill());
    EXPECT_FALSE(meta.is_binary());
    EXPECT_FALSE(meta.is_enum());
    EXPECT_FALSE(meta.is_auto_increment());
    EXPECT_FALSE(meta.is_timestamp());
    EXPECT_FALSE(meta.is_set());
    EXPECT_FALSE(meta.has_no_default_value());
    EXPECT_FALSE(meta.is_set_to_now_on_update());
}


} // anon namespace



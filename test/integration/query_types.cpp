/*
 * query_types.cpp
 *
 *  Created on: Dec 21, 2019
 *      Author: ruben
 */

#include "integration_test_common.hpp"
#include "metadata_validator.hpp"
#include "test_common.hpp"
#include <sstream>

using namespace mysql::test;
using namespace testing;
using namespace date::literals;
using mysql::value;
using mysql::field_metadata;
using mysql::field_type;

namespace
{

/**
 * These tests try to cover a range of the possible types and values MySQL support.
 * Given a table, a single field, and a row_id that will match the "id" field of the table,
 * we validate that we get the expected metadata and the expected value. The cases are
 * defined in SQL in db_setup.sql
 */
struct QueryTypesParams
{
	const char* table;
	const char* field;
	const char* row_id;
	value expected_value;
	meta_validator mvalid;

	template <typename ValueType, typename... Args>
	QueryTypesParams(const char* table, const char* field, const char* row_id,
			ValueType&& expected_value, Args&&... args) :
		table(table),
		field(field),
		row_id(row_id),
		expected_value(std::forward<ValueType>(expected_value)),
		mvalid(table, field, std::forward<Args>(args)...)
	{
	}
};

std::ostream& operator<<(std::ostream& os, const QueryTypesParams& v)
{
	return os << v.table << "." << v.field << "." << v.row_id;
}

struct QueryTypesTest : IntegTest, WithParamInterface<QueryTypesParams>
{
	QueryTypesTest()
	{
		handshake();
	}
};

TEST_P(QueryTypesTest, Query_MetadataAndValueCorrect)
{
	const auto& param = GetParam();

	// Compose the query
	std::ostringstream queryss;
	queryss << "SELECT " << param.field
			<< " FROM " << param.table
			<< " WHERE id = '" << param.row_id << "'";
	auto query = queryss.str();

	// Execute it
	auto result = conn.query(query);
	auto rows = result.fetch_all();

	// Validate the received metadata
	validate_meta(result.fields(), {param.mvalid});

	// Validate the returned value
	mysql::row expected_row ({param.expected_value});
	ASSERT_EQ(rows.size(), 1);
	EXPECT_EQ(static_cast<const mysql::row&>(rows[0]), expected_row); // make gtest pick operator<<
}

using flagsvec = std::vector<meta_validator::flag_getter>;

const flagsvec no_flags {};
const flagsvec flags_unsigned { &field_metadata::is_unsigned };
const flagsvec flags_zerofill { &field_metadata::is_unsigned, &field_metadata::is_zerofill };

// Integers
INSTANTIATE_TEST_SUITE_P(TINYINT, QueryTypesTest, Values(
	QueryTypesParams("types_tinyint", "field_signed", "regular", std::int32_t(20), field_type::tinyint),
	QueryTypesParams("types_tinyint", "field_signed", "negative", std::int32_t(-20), field_type::tinyint),
	QueryTypesParams("types_tinyint", "field_signed", "min", std::int32_t(-0x80), field_type::tinyint),
	QueryTypesParams("types_tinyint", "field_signed", "max", std::int32_t(0x7f), field_type::tinyint),

	QueryTypesParams("types_tinyint", "field_unsigned", "regular", std::uint32_t(20), field_type::tinyint, flags_unsigned),
	QueryTypesParams("types_tinyint", "field_unsigned", "min", std::uint32_t(0), field_type::tinyint, flags_unsigned),
	QueryTypesParams("types_tinyint", "field_unsigned", "max", std::uint32_t(0xff), field_type::tinyint, flags_unsigned),

	QueryTypesParams("types_tinyint", "field_width", "regular", std::int32_t(20), field_type::tinyint),
	QueryTypesParams("types_tinyint", "field_width", "negative", std::int32_t(-20), field_type::tinyint),

	QueryTypesParams("types_tinyint", "field_zerofill", "regular", std::uint32_t(20), field_type::tinyint, flags_zerofill),
	QueryTypesParams("types_tinyint", "field_zerofill", "min", std::uint32_t(0), field_type::tinyint, flags_zerofill)
));

INSTANTIATE_TEST_SUITE_P(SMALLINT, QueryTypesTest, Values(
	QueryTypesParams("types_smallint", "field_signed", "regular", std::int32_t(20), field_type::smallint),
	QueryTypesParams("types_smallint", "field_signed", "negative", std::int32_t(-20), field_type::smallint),
	QueryTypesParams("types_smallint", "field_signed", "min", std::int32_t(-0x8000), field_type::smallint),
	QueryTypesParams("types_smallint", "field_signed", "max", std::int32_t(0x7fff), field_type::smallint),

	QueryTypesParams("types_smallint", "field_unsigned", "regular", std::uint32_t(20), field_type::smallint, flags_unsigned),
	QueryTypesParams("types_smallint", "field_unsigned", "min", std::uint32_t(0), field_type::smallint, flags_unsigned),
	QueryTypesParams("types_smallint", "field_unsigned", "max", std::uint32_t(0xffff), field_type::smallint, flags_unsigned),

	QueryTypesParams("types_smallint", "field_width", "regular", std::int32_t(20), field_type::smallint),
	QueryTypesParams("types_smallint", "field_width", "negative", std::int32_t(-20), field_type::smallint),

	QueryTypesParams("types_smallint", "field_zerofill", "regular", std::uint32_t(20), field_type::smallint, flags_zerofill),
	QueryTypesParams("types_smallint", "field_zerofill", "min", std::uint32_t(0), field_type::smallint, flags_zerofill)
));

INSTANTIATE_TEST_SUITE_P(MEDIUMINT, QueryTypesTest, Values(
	QueryTypesParams("types_mediumint", "field_signed", "regular", std::int32_t(20), field_type::mediumint),
	QueryTypesParams("types_mediumint", "field_signed", "negative", std::int32_t(-20), field_type::mediumint),
	QueryTypesParams("types_mediumint", "field_signed", "min", std::int32_t(-0x800000), field_type::mediumint),
	QueryTypesParams("types_mediumint", "field_signed", "max", std::int32_t(0x7fffff), field_type::mediumint),

	QueryTypesParams("types_mediumint", "field_unsigned", "regular", std::uint32_t(20), field_type::mediumint, flags_unsigned),
	QueryTypesParams("types_mediumint", "field_unsigned", "min", std::uint32_t(0), field_type::mediumint, flags_unsigned),
	QueryTypesParams("types_mediumint", "field_unsigned", "max", std::uint32_t(0xffffff), field_type::mediumint, flags_unsigned),

	QueryTypesParams("types_mediumint", "field_width", "regular", std::int32_t(20), field_type::mediumint),
	QueryTypesParams("types_mediumint", "field_width", "negative", std::int32_t(-20), field_type::mediumint),

	QueryTypesParams("types_mediumint", "field_zerofill", "regular", std::uint32_t(20), field_type::mediumint, flags_zerofill),
	QueryTypesParams("types_mediumint", "field_zerofill", "min", std::uint32_t(0), field_type::mediumint, flags_zerofill)
));

INSTANTIATE_TEST_SUITE_P(INT, QueryTypesTest, Values(
	QueryTypesParams("types_int", "field_signed", "regular", std::int32_t(20), field_type::int_),
	QueryTypesParams("types_int", "field_signed", "negative", std::int32_t(-20), field_type::int_),
	QueryTypesParams("types_int", "field_signed", "min", std::int32_t(-0x80000000), field_type::int_),
	QueryTypesParams("types_int", "field_signed", "max", std::int32_t(0x7fffffff), field_type::int_),

	QueryTypesParams("types_int", "field_unsigned", "regular", std::uint32_t(20), field_type::int_, flags_unsigned),
	QueryTypesParams("types_int", "field_unsigned", "min", std::uint32_t(0), field_type::int_, flags_unsigned),
	QueryTypesParams("types_int", "field_unsigned", "max", std::uint32_t(0xffffffff), field_type::int_, flags_unsigned),

	QueryTypesParams("types_int", "field_width", "regular", std::int32_t(20), field_type::int_),
	QueryTypesParams("types_int", "field_width", "negative", std::int32_t(-20), field_type::int_),

	QueryTypesParams("types_int", "field_zerofill", "regular", std::uint32_t(20), field_type::int_, flags_zerofill),
	QueryTypesParams("types_int", "field_zerofill", "min", std::uint32_t(0), field_type::int_, flags_zerofill)
));

INSTANTIATE_TEST_SUITE_P(BIGINT, QueryTypesTest, Values(
	QueryTypesParams("types_bigint", "field_signed", "regular", std::int64_t(20), field_type::bigint),
	QueryTypesParams("types_bigint", "field_signed", "negative", std::int64_t(-20), field_type::bigint),
	QueryTypesParams("types_bigint", "field_signed", "min", std::int64_t(-0x8000000000000000), field_type::bigint),
	QueryTypesParams("types_bigint", "field_signed", "max", std::int64_t(0x7fffffffffffffff), field_type::bigint),

	QueryTypesParams("types_bigint", "field_unsigned", "regular", std::uint64_t(20), field_type::bigint, flags_unsigned),
	QueryTypesParams("types_bigint", "field_unsigned", "min", std::uint64_t(0), field_type::bigint, flags_unsigned),
	QueryTypesParams("types_bigint", "field_unsigned", "max", std::uint64_t(0xffffffffffffffff), field_type::bigint, flags_unsigned),

	QueryTypesParams("types_bigint", "field_width", "regular", std::int64_t(20), field_type::bigint),
	QueryTypesParams("types_bigint", "field_width", "negative", std::int64_t(-20), field_type::bigint),

	QueryTypesParams("types_bigint", "field_zerofill", "regular", std::uint64_t(20), field_type::bigint, flags_zerofill),
	QueryTypesParams("types_bigint", "field_zerofill", "min", std::uint64_t(0), field_type::bigint, flags_zerofill)
));

// Floating point
INSTANTIATE_TEST_SUITE_P(FLOAT, QueryTypesTest, Values(
	QueryTypesParams("types_float", "field_signed", "zero", 0.0f, field_type::float_, no_flags, 31),
	QueryTypesParams("types_float", "field_signed", "int_positive", 4.0f, field_type::float_, no_flags, 31),
	QueryTypesParams("types_float", "field_signed", "int_negative", -4.0f, field_type::float_, no_flags, 31),
	QueryTypesParams("types_float", "field_signed", "fractional_positive", 4.2f, field_type::float_, no_flags, 31),
	QueryTypesParams("types_float", "field_signed", "fractional_negative", -4.2f, field_type::float_, no_flags, 31),
	QueryTypesParams("types_float", "field_signed", "positive_exp_positive_int", 3e20f, field_type::float_, no_flags, 31),
	QueryTypesParams("types_float", "field_signed", "positive_exp_negative_int", -3e20f, field_type::float_, no_flags, 31),
	QueryTypesParams("types_float", "field_signed", "positive_exp_positive_fractional", 3.14e20f, field_type::float_, no_flags, 31),
	QueryTypesParams("types_float", "field_signed", "positive_exp_negative_fractional", -3.14e20f, field_type::float_, no_flags, 31),
	QueryTypesParams("types_float", "field_signed", "negative_exp_positive_fractional", 3.14e-20f, field_type::float_, no_flags, 31),

	QueryTypesParams("types_float", "field_unsigned", "zero", 0.0f, field_type::float_, flags_unsigned, 31),
	QueryTypesParams("types_float", "field_unsigned", "fractional_positive", 4.2f, field_type::float_, flags_unsigned, 31),

	QueryTypesParams("types_float", "field_width", "zero", 0.0f, field_type::float_, no_flags, 10),
	QueryTypesParams("types_float", "field_width", "fractional_positive", 4.2f, field_type::float_, no_flags, 10),
	QueryTypesParams("types_float", "field_width", "fractional_negative", -4.2f, field_type::float_, no_flags, 10),

	QueryTypesParams("types_float", "field_zerofill", "zero", 0.0f, field_type::float_, flags_zerofill, 31),
	QueryTypesParams("types_float", "field_zerofill", "fractional_positive", 4.2f, field_type::float_, flags_zerofill, 31),
	QueryTypesParams("types_float", "field_zerofill", "positive_exp_positive_fractional", 3.14e20f, field_type::float_, flags_zerofill, 31),
	QueryTypesParams("types_float", "field_zerofill", "negative_exp_positive_fractional", 3.14e-20f, field_type::float_, flags_zerofill, 31)
));

INSTANTIATE_TEST_SUITE_P(DOUBLE, QueryTypesTest, Values(
	QueryTypesParams("types_double", "field_signed", "zero", 0.0, field_type::double_, no_flags, 31),
	QueryTypesParams("types_double", "field_signed", "int_positive", 4.0, field_type::double_, no_flags, 31),
	QueryTypesParams("types_double", "field_signed", "int_negative", -4.0, field_type::double_, no_flags, 31),
	QueryTypesParams("types_double", "field_signed", "fractional_positive", 4.2, field_type::double_, no_flags, 31),
	QueryTypesParams("types_double", "field_signed", "fractional_negative", -4.2, field_type::double_, no_flags, 31),
	QueryTypesParams("types_double", "field_signed", "positive_exp_positive_int", 3e200, field_type::double_, no_flags, 31),
	QueryTypesParams("types_double", "field_signed", "positive_exp_negative_int", -3e200, field_type::double_, no_flags, 31),
	QueryTypesParams("types_double", "field_signed", "positive_exp_positive_fractional", 3.14e200, field_type::double_, no_flags, 31),
	QueryTypesParams("types_double", "field_signed", "positive_exp_negative_fractional", -3.14e200, field_type::double_, no_flags, 31),
	QueryTypesParams("types_double", "field_signed", "negative_exp_positive_fractional", 3.14e-200, field_type::double_, no_flags, 31),

	QueryTypesParams("types_double", "field_unsigned", "zero", 0.0, field_type::double_, flags_unsigned, 31),
	QueryTypesParams("types_double", "field_unsigned", "fractional_positive", 4.2, field_type::double_, flags_unsigned, 31),

	QueryTypesParams("types_double", "field_width", "zero", 0.0, field_type::double_, no_flags, 10),
	QueryTypesParams("types_double", "field_width", "fractional_positive", 4.2, field_type::double_, no_flags, 10),
	QueryTypesParams("types_double", "field_width", "fractional_negative", -4.2, field_type::double_, no_flags, 10),

	QueryTypesParams("types_double", "field_zerofill", "zero", 0.0, field_type::double_, flags_zerofill, 31),
	QueryTypesParams("types_double", "field_zerofill", "fractional_positive", 4.2, field_type::double_, flags_zerofill, 31),
	QueryTypesParams("types_double", "field_zerofill", "positive_exp_positive_fractional", 3.14e200, field_type::double_, flags_zerofill, 31),
	QueryTypesParams("types_double", "field_zerofill", "negative_exp_positive_fractional", 3.14e-200, field_type::double_, flags_zerofill, 31)
));

// Dates and times
INSTANTIATE_TEST_SUITE_P(DATE, QueryTypesTest, Values(
	QueryTypesParams("types_date", "field_date", "regular", 2010_y/3/28_d, field_type::date),
	QueryTypesParams("types_date", "field_date", "leap", 1788_y/2/29_d, field_type::date),
	QueryTypesParams("types_date", "field_date", "min", 1000_y/1/1_d, field_type::date),
	QueryTypesParams("types_date", "field_date", "max", 9999_y/12/31_d, field_type::date)
));

INSTANTIATE_TEST_SUITE_P(DATETIME, QueryTypesTest, Values(
	QueryTypesParams("types_datetime", "field_0", "date", makedt(2010, 5, 2), field_type::datetime),
	QueryTypesParams("types_datetime", "field_1", "date", makedt(2010, 5, 2), field_type::datetime, no_flags, 1),
	QueryTypesParams("types_datetime", "field_2", "date", makedt(2010, 5, 2), field_type::datetime, no_flags, 2),
	QueryTypesParams("types_datetime", "field_3", "date", makedt(2010, 5, 2), field_type::datetime, no_flags, 3),
	QueryTypesParams("types_datetime", "field_4", "date", makedt(2010, 5, 2), field_type::datetime, no_flags, 4),
	QueryTypesParams("types_datetime", "field_5", "date", makedt(2010, 5, 2), field_type::datetime, no_flags, 5),
	QueryTypesParams("types_datetime", "field_6", "date", makedt(2010, 5, 2), field_type::datetime, no_flags, 6),

	QueryTypesParams("types_datetime", "field_0", "h", makedt(2010, 5, 2, 23), field_type::datetime),
	QueryTypesParams("types_datetime", "field_1", "h", makedt(2010, 5, 2, 23), field_type::datetime, no_flags, 1),
	QueryTypesParams("types_datetime", "field_2", "h", makedt(2010, 5, 2, 23), field_type::datetime, no_flags, 2),
	QueryTypesParams("types_datetime", "field_3", "h", makedt(2010, 5, 2, 23), field_type::datetime, no_flags, 3),
	QueryTypesParams("types_datetime", "field_4", "h", makedt(2010, 5, 2, 23), field_type::datetime, no_flags, 4),
	QueryTypesParams("types_datetime", "field_5", "h", makedt(2010, 5, 2, 23), field_type::datetime, no_flags, 5),
	QueryTypesParams("types_datetime", "field_6", "h", makedt(2010, 5, 2, 23), field_type::datetime, no_flags, 6),

	QueryTypesParams("types_datetime", "field_0", "hm", makedt(2010, 5, 2, 23, 1), field_type::datetime),
	QueryTypesParams("types_datetime", "field_1", "hm", makedt(2010, 5, 2, 23, 1), field_type::datetime, no_flags, 1),
	QueryTypesParams("types_datetime", "field_2", "hm", makedt(2010, 5, 2, 23, 1), field_type::datetime, no_flags, 2),
	QueryTypesParams("types_datetime", "field_3", "hm", makedt(2010, 5, 2, 23, 1), field_type::datetime, no_flags, 3),
	QueryTypesParams("types_datetime", "field_4", "hm", makedt(2010, 5, 2, 23, 1), field_type::datetime, no_flags, 4),
	QueryTypesParams("types_datetime", "field_5", "hm", makedt(2010, 5, 2, 23, 1), field_type::datetime, no_flags, 5),
	QueryTypesParams("types_datetime", "field_6", "hm", makedt(2010, 5, 2, 23, 1), field_type::datetime, no_flags, 6),

	QueryTypesParams("types_datetime", "field_0", "hms", makedt(2010, 5, 2, 23, 1, 50), field_type::datetime),
	QueryTypesParams("types_datetime", "field_1", "hms", makedt(2010, 5, 2, 23, 1, 50), field_type::datetime, no_flags, 1),
	QueryTypesParams("types_datetime", "field_2", "hms", makedt(2010, 5, 2, 23, 1, 50), field_type::datetime, no_flags, 2),
	QueryTypesParams("types_datetime", "field_3", "hms", makedt(2010, 5, 2, 23, 1, 50), field_type::datetime, no_flags, 3),
	QueryTypesParams("types_datetime", "field_4", "hms", makedt(2010, 5, 2, 23, 1, 50), field_type::datetime, no_flags, 4),
	QueryTypesParams("types_datetime", "field_5", "hms", makedt(2010, 5, 2, 23, 1, 50), field_type::datetime, no_flags, 5),
	QueryTypesParams("types_datetime", "field_6", "hms", makedt(2010, 5, 2, 23, 1, 50), field_type::datetime, no_flags, 6),

	QueryTypesParams("types_datetime", "field_1", "hmsu", makedt(2010, 5, 2, 23, 1, 50, 100000), field_type::datetime, no_flags, 1),
	QueryTypesParams("types_datetime", "field_2", "hmsu", makedt(2010, 5, 2, 23, 1, 50, 120000), field_type::datetime, no_flags, 2),
	QueryTypesParams("types_datetime", "field_3", "hmsu", makedt(2010, 5, 2, 23, 1, 50, 123000), field_type::datetime, no_flags, 3),
	QueryTypesParams("types_datetime", "field_4", "hmsu", makedt(2010, 5, 2, 23, 1, 50, 123400), field_type::datetime, no_flags, 4),
	QueryTypesParams("types_datetime", "field_5", "hmsu", makedt(2010, 5, 2, 23, 1, 50, 123450), field_type::datetime, no_flags, 5),
	QueryTypesParams("types_datetime", "field_6", "hmsu", makedt(2010, 5, 2, 23, 1, 50, 123456), field_type::datetime, no_flags, 6),

	QueryTypesParams("types_datetime", "field_0", "min", makedt(1000, 1, 1), field_type::datetime),
	QueryTypesParams("types_datetime", "field_1", "min", makedt(1000, 1, 1), field_type::datetime, no_flags, 1),
	QueryTypesParams("types_datetime", "field_2", "min", makedt(1000, 1, 1), field_type::datetime, no_flags, 2),
	QueryTypesParams("types_datetime", "field_3", "min", makedt(1000, 1, 1), field_type::datetime, no_flags, 3),
	QueryTypesParams("types_datetime", "field_4", "min", makedt(1000, 1, 1), field_type::datetime, no_flags, 4),
	QueryTypesParams("types_datetime", "field_5", "min", makedt(1000, 1, 1), field_type::datetime, no_flags, 5),
	QueryTypesParams("types_datetime", "field_6", "min", makedt(1000, 1, 1), field_type::datetime, no_flags, 6),

	QueryTypesParams("types_datetime", "field_0", "max", makedt(9999, 12, 31, 23, 59, 59), field_type::datetime, no_flags, 0),
	QueryTypesParams("types_datetime", "field_1", "max", makedt(9999, 12, 31, 23, 59, 59, 900000), field_type::datetime, no_flags, 1),
	QueryTypesParams("types_datetime", "field_2", "max", makedt(9999, 12, 31, 23, 59, 59, 990000), field_type::datetime, no_flags, 2),
	QueryTypesParams("types_datetime", "field_3", "max", makedt(9999, 12, 31, 23, 59, 59, 999000), field_type::datetime, no_flags, 3),
	QueryTypesParams("types_datetime", "field_4", "max", makedt(9999, 12, 31, 23, 59, 59, 999900), field_type::datetime, no_flags, 4),
	QueryTypesParams("types_datetime", "field_5", "max", makedt(9999, 12, 31, 23, 59, 59, 999990), field_type::datetime, no_flags, 5),
	QueryTypesParams("types_datetime", "field_6", "max", makedt(9999, 12, 31, 23, 59, 59, 999999), field_type::datetime, no_flags, 6)

));

INSTANTIATE_TEST_SUITE_P(TIMESTAMP, QueryTypesTest, Values(
	QueryTypesParams("types_timestamp", "field_0", "date", makedt(2010, 5, 2), field_type::timestamp),
	QueryTypesParams("types_timestamp", "field_1", "date", makedt(2010, 5, 2), field_type::timestamp, no_flags, 1),
	QueryTypesParams("types_timestamp", "field_2", "date", makedt(2010, 5, 2), field_type::timestamp, no_flags, 2),
	QueryTypesParams("types_timestamp", "field_3", "date", makedt(2010, 5, 2), field_type::timestamp, no_flags, 3),
	QueryTypesParams("types_timestamp", "field_4", "date", makedt(2010, 5, 2), field_type::timestamp, no_flags, 4),
	QueryTypesParams("types_timestamp", "field_5", "date", makedt(2010, 5, 2), field_type::timestamp, no_flags, 5),
	QueryTypesParams("types_timestamp", "field_6", "date", makedt(2010, 5, 2), field_type::timestamp, no_flags, 6),

	QueryTypesParams("types_timestamp", "field_0", "h", makedt(2010, 5, 2, 23), field_type::timestamp),
	QueryTypesParams("types_timestamp", "field_1", "h", makedt(2010, 5, 2, 23), field_type::timestamp, no_flags, 1),
	QueryTypesParams("types_timestamp", "field_2", "h", makedt(2010, 5, 2, 23), field_type::timestamp, no_flags, 2),
	QueryTypesParams("types_timestamp", "field_3", "h", makedt(2010, 5, 2, 23), field_type::timestamp, no_flags, 3),
	QueryTypesParams("types_timestamp", "field_4", "h", makedt(2010, 5, 2, 23), field_type::timestamp, no_flags, 4),
	QueryTypesParams("types_timestamp", "field_5", "h", makedt(2010, 5, 2, 23), field_type::timestamp, no_flags, 5),
	QueryTypesParams("types_timestamp", "field_6", "h", makedt(2010, 5, 2, 23), field_type::timestamp, no_flags, 6),

	QueryTypesParams("types_timestamp", "field_0", "hm", makedt(2010, 5, 2, 23, 1), field_type::timestamp),
	QueryTypesParams("types_timestamp", "field_1", "hm", makedt(2010, 5, 2, 23, 1), field_type::timestamp, no_flags, 1),
	QueryTypesParams("types_timestamp", "field_2", "hm", makedt(2010, 5, 2, 23, 1), field_type::timestamp, no_flags, 2),
	QueryTypesParams("types_timestamp", "field_3", "hm", makedt(2010, 5, 2, 23, 1), field_type::timestamp, no_flags, 3),
	QueryTypesParams("types_timestamp", "field_4", "hm", makedt(2010, 5, 2, 23, 1), field_type::timestamp, no_flags, 4),
	QueryTypesParams("types_timestamp", "field_5", "hm", makedt(2010, 5, 2, 23, 1), field_type::timestamp, no_flags, 5),
	QueryTypesParams("types_timestamp", "field_6", "hm", makedt(2010, 5, 2, 23, 1), field_type::timestamp, no_flags, 6),

	QueryTypesParams("types_timestamp", "field_0", "hms", makedt(2010, 5, 2, 23, 1, 50), field_type::timestamp),
	QueryTypesParams("types_timestamp", "field_1", "hms", makedt(2010, 5, 2, 23, 1, 50), field_type::timestamp, no_flags, 1),
	QueryTypesParams("types_timestamp", "field_2", "hms", makedt(2010, 5, 2, 23, 1, 50), field_type::timestamp, no_flags, 2),
	QueryTypesParams("types_timestamp", "field_3", "hms", makedt(2010, 5, 2, 23, 1, 50), field_type::timestamp, no_flags, 3),
	QueryTypesParams("types_timestamp", "field_4", "hms", makedt(2010, 5, 2, 23, 1, 50), field_type::timestamp, no_flags, 4),
	QueryTypesParams("types_timestamp", "field_5", "hms", makedt(2010, 5, 2, 23, 1, 50), field_type::timestamp, no_flags, 5),
	QueryTypesParams("types_timestamp", "field_6", "hms", makedt(2010, 5, 2, 23, 1, 50), field_type::timestamp, no_flags, 6),

	QueryTypesParams("types_timestamp", "field_1", "hmsu", makedt(2010, 5, 2, 23, 1, 50, 100000), field_type::timestamp, no_flags, 1),
	QueryTypesParams("types_timestamp", "field_2", "hmsu", makedt(2010, 5, 2, 23, 1, 50, 120000), field_type::timestamp, no_flags, 2),
	QueryTypesParams("types_timestamp", "field_3", "hmsu", makedt(2010, 5, 2, 23, 1, 50, 123000), field_type::timestamp, no_flags, 3),
	QueryTypesParams("types_timestamp", "field_4", "hmsu", makedt(2010, 5, 2, 23, 1, 50, 123400), field_type::timestamp, no_flags, 4),
	QueryTypesParams("types_timestamp", "field_5", "hmsu", makedt(2010, 5, 2, 23, 1, 50, 123450), field_type::timestamp, no_flags, 5),
	QueryTypesParams("types_timestamp", "field_6", "hmsu", makedt(2010, 5, 2, 23, 1, 50, 123456), field_type::timestamp, no_flags, 6)
));

INSTANTIATE_TEST_SUITE_P(TIME, QueryTypesTest, Values(
	QueryTypesParams("types_time", "field_0", "h", maket(1, 0, 0), field_type::time),
	QueryTypesParams("types_time", "field_1", "h", maket(1, 0, 0), field_type::time, no_flags, 1),
	QueryTypesParams("types_time", "field_2", "h", maket(1, 0, 0), field_type::time, no_flags, 2),
	QueryTypesParams("types_time", "field_3", "h", maket(1, 0, 0), field_type::time, no_flags, 3),
	QueryTypesParams("types_time", "field_4", "h", maket(1, 0, 0), field_type::time, no_flags, 4),
	QueryTypesParams("types_time", "field_5", "h", maket(1, 0, 0), field_type::time, no_flags, 5),
	QueryTypesParams("types_time", "field_6", "h", maket(1, 0, 0), field_type::time, no_flags, 6),

	QueryTypesParams("types_time", "field_0", "hm", maket(1, 2, 0), field_type::time),
	QueryTypesParams("types_time", "field_1", "hm", maket(1, 2, 0), field_type::time, no_flags, 1),
	QueryTypesParams("types_time", "field_2", "hm", maket(1, 2, 0), field_type::time, no_flags, 2),
	QueryTypesParams("types_time", "field_3", "hm", maket(1, 2, 0), field_type::time, no_flags, 3),
	QueryTypesParams("types_time", "field_4", "hm", maket(1, 2, 0), field_type::time, no_flags, 4),
	QueryTypesParams("types_time", "field_5", "hm", maket(1, 2, 0), field_type::time, no_flags, 5),
	QueryTypesParams("types_time", "field_6", "hm", maket(1, 2, 0), field_type::time, no_flags, 6),

	QueryTypesParams("types_time", "field_0", "hms", maket(120, 2, 3), field_type::time),
	QueryTypesParams("types_time", "field_1", "hms", maket(120, 2, 3), field_type::time, no_flags, 1),
	QueryTypesParams("types_time", "field_2", "hms", maket(120, 2, 3), field_type::time, no_flags, 2),
	QueryTypesParams("types_time", "field_3", "hms", maket(120, 2, 3), field_type::time, no_flags, 3),
	QueryTypesParams("types_time", "field_4", "hms", maket(120, 2, 3), field_type::time, no_flags, 4),
	QueryTypesParams("types_time", "field_5", "hms", maket(120, 2, 3), field_type::time, no_flags, 5),
	QueryTypesParams("types_time", "field_6", "hms", maket(120, 2, 3), field_type::time, no_flags, 6),

	QueryTypesParams("types_time", "field_1", "hmsu", maket(120, 2, 3, 100000), field_type::time, no_flags, 1),
	QueryTypesParams("types_time", "field_2", "hmsu", maket(120, 2, 3, 120000), field_type::time, no_flags, 2),
	QueryTypesParams("types_time", "field_3", "hmsu", maket(120, 2, 3, 123000), field_type::time, no_flags, 3),
	QueryTypesParams("types_time", "field_4", "hmsu", maket(120, 2, 3, 123400), field_type::time, no_flags, 4),
	QueryTypesParams("types_time", "field_5", "hmsu", maket(120, 2, 3, 123450), field_type::time, no_flags, 5),
	QueryTypesParams("types_time", "field_6", "hmsu", maket(120, 2, 3, 123456), field_type::time, no_flags, 6),

	QueryTypesParams("types_time", "field_0", "s", maket(0, 0, 21), field_type::time),
	QueryTypesParams("types_time", "field_1", "s", maket(0, 0, 21, 100000), field_type::time, no_flags, 1),
	QueryTypesParams("types_time", "field_2", "s", maket(0, 0, 21, 120000), field_type::time, no_flags, 2),
	QueryTypesParams("types_time", "field_3", "s", maket(0, 0, 21, 123000), field_type::time, no_flags, 3),
	QueryTypesParams("types_time", "field_4", "s", maket(0, 0, 21, 123400), field_type::time, no_flags, 4),
	QueryTypesParams("types_time", "field_5", "s", maket(0, 0, 21, 123450), field_type::time, no_flags, 5),
	QueryTypesParams("types_time", "field_6", "s", maket(0, 0, 21, 123456), field_type::time, no_flags, 6),

	QueryTypesParams("types_time", "field_0", "negative_hmsu", maket(-120, -2, -3), field_type::time),
	QueryTypesParams("types_time", "field_1", "negative_hmsu", maket(-120, -2, -3, -100000), field_type::time, no_flags, 1),
	QueryTypesParams("types_time", "field_2", "negative_hmsu", maket(-120, -2, -3, -20000), field_type::time, no_flags, 2),
	QueryTypesParams("types_time", "field_3", "negative_hmsu", maket(-120, -2, -3, -23000), field_type::time, no_flags, 3),
	QueryTypesParams("types_time", "field_4", "negative_hmsu", maket(-120, -2, -3, -23400), field_type::time, no_flags, 4),
	QueryTypesParams("types_time", "field_5", "negative_hmsu", maket(-120, -2, -3, -23450), field_type::time, no_flags, 5),
	QueryTypesParams("types_time", "field_6", "negative_hmsu", maket(-120, -2, -3, -23456), field_type::time, no_flags, 6),

	QueryTypesParams("types_time", "field_0", "min", maket(-838, -59, -59), field_type::time),
	QueryTypesParams("types_time", "field_1", "min", maket(-838, -59, -58, -900000), field_type::time, no_flags, 1),
	QueryTypesParams("types_time", "field_2", "min", maket(-838, -59, -58, -990000), field_type::time, no_flags, 2),
	QueryTypesParams("types_time", "field_3", "min", maket(-838, -59, -58, -999000), field_type::time, no_flags, 3),
	QueryTypesParams("types_time", "field_4", "min", maket(-838, -59, -58, -999900), field_type::time, no_flags, 4),
	QueryTypesParams("types_time", "field_5", "min", maket(-838, -59, -58, -999990), field_type::time, no_flags, 5),
	QueryTypesParams("types_time", "field_6", "min", maket(-838, -59, -58, -999999), field_type::time, no_flags, 6),

	QueryTypesParams("types_time", "field_0", "max", maket(838, 59, 59), field_type::time),
	QueryTypesParams("types_time", "field_1", "max", maket(838, 59, 58, 900000), field_type::time, no_flags, 1),
	QueryTypesParams("types_time", "field_2", "max", maket(838, 59, 58, 990000), field_type::time, no_flags, 2),
	QueryTypesParams("types_time", "field_3", "max", maket(838, 59, 58, 999000), field_type::time, no_flags, 3),
	QueryTypesParams("types_time", "field_4", "max", maket(838, 59, 58, 999900), field_type::time, no_flags, 4),
	QueryTypesParams("types_time", "field_5", "max", maket(838, 59, 58, 999990), field_type::time, no_flags, 5),
	QueryTypesParams("types_time", "field_6", "max", maket(838, 59, 58, 999999), field_type::time, no_flags, 6),

	QueryTypesParams("types_time", "field_0", "zero", maket(0, 0, 0), field_type::time),
	QueryTypesParams("types_time", "field_1", "zero", maket(0, 0, 0), field_type::time, no_flags, 1),
	QueryTypesParams("types_time", "field_2", "zero", maket(0, 0, 0), field_type::time, no_flags, 2),
	QueryTypesParams("types_time", "field_3", "zero", maket(0, 0, 0), field_type::time, no_flags, 3),
	QueryTypesParams("types_time", "field_4", "zero", maket(0, 0, 0), field_type::time, no_flags, 4),
	QueryTypesParams("types_time", "field_5", "zero", maket(0, 0, 0), field_type::time, no_flags, 5),
	QueryTypesParams("types_time", "field_6", "zero", maket(0, 0, 0), field_type::time, no_flags, 6)
));

INSTANTIATE_TEST_SUITE_P(YEAR, QueryTypesTest, Values(
	QueryTypesParams("types_year", "field_default", "regular", std::uint32_t(2019), field_type::year, flags_zerofill),
	QueryTypesParams("types_year", "field_default", "min", std::uint32_t(1901), field_type::year, flags_zerofill),
	QueryTypesParams("types_year", "field_default", "max", std::uint32_t(2155), field_type::year, flags_zerofill),
	QueryTypesParams("types_year", "field_default", "zero", std::uint32_t(0), field_type::year, flags_zerofill)
));

INSTANTIATE_TEST_SUITE_P(STRING, QueryTypesTest, Values(
	QueryTypesParams("types_string", "field_char", "regular", "test_char", field_type::char_),
	QueryTypesParams("types_string", "field_char", "utf8", u8"\u00F1", field_type::char_),
	QueryTypesParams("types_string", "field_char", "empty", "", field_type::char_),

	QueryTypesParams("types_string", "field_varchar", "regular", "test_varchar", field_type::varchar),
	QueryTypesParams("types_string", "field_varchar", "utf8", u8"\u00D1", field_type::varchar),
	QueryTypesParams("types_string", "field_varchar", "empty", "", field_type::varchar),

	QueryTypesParams("types_string", "field_tinytext", "regular", "test_tinytext", field_type::text),
	QueryTypesParams("types_string", "field_tinytext", "utf8", u8"\u00e1", field_type::text),
	QueryTypesParams("types_string", "field_tinytext", "empty", "", field_type::text),

	QueryTypesParams("types_string", "field_text", "regular", "test_text", field_type::text),
	QueryTypesParams("types_string", "field_text", "utf8", u8"\u00e9", field_type::text),
	QueryTypesParams("types_string", "field_text", "empty", "", field_type::text),

	QueryTypesParams("types_string", "field_mediumtext", "regular", "test_mediumtext", field_type::text),
	QueryTypesParams("types_string", "field_mediumtext", "utf8", u8"\u00ed", field_type::text),
	QueryTypesParams("types_string", "field_mediumtext", "empty", "", field_type::text),

	QueryTypesParams("types_string", "field_longtext", "regular", "test_longtext", field_type::text),
	QueryTypesParams("types_string", "field_longtext", "utf8", u8"\u00f3", field_type::text),
	QueryTypesParams("types_string", "field_longtext", "empty", "", field_type::text),

	QueryTypesParams("types_string", "field_enum", "regular", "red", field_type::enum_),

	QueryTypesParams("types_string", "field_set", "regular", "red,green", field_type::set),
	QueryTypesParams("types_string", "field_set", "empty", "", field_type::set)
));

INSTANTIATE_TEST_SUITE_P(BINARY, QueryTypesTest, Values(
	// BINARY values get padded with zeros to the declared length
	QueryTypesParams("types_binary", "field_binary", "regular", makesv("\0_binary\0\0"), field_type::binary),
	QueryTypesParams("types_binary", "field_binary", "nonascii", makesv("\0\xff" "\0\0\0\0\0\0\0\0"), field_type::binary),
	QueryTypesParams("types_binary", "field_binary", "empty", makesv("\0\0\0\0\0\0\0\0\0\0"), field_type::binary),

	QueryTypesParams("types_binary", "field_varbinary", "regular", makesv("\0_varbinary"), field_type::varbinary),
	QueryTypesParams("types_binary", "field_varbinary", "nonascii", makesv("\1\xfe"), field_type::varbinary),
	QueryTypesParams("types_binary", "field_varbinary", "empty", "", field_type::varbinary),

	QueryTypesParams("types_binary", "field_tinyblob", "regular", makesv("\0_tinyblob"), field_type::blob),
	QueryTypesParams("types_binary", "field_tinyblob", "nonascii", makesv("\2\xfd"), field_type::blob),
	QueryTypesParams("types_binary", "field_tinyblob", "empty", "", field_type::blob),

	QueryTypesParams("types_binary", "field_blob", "regular", makesv("\0_blob"), field_type::blob),
	QueryTypesParams("types_binary", "field_blob", "nonascii", makesv("\3\xfc"), field_type::blob),
	QueryTypesParams("types_binary", "field_blob", "empty", "", field_type::blob),

	QueryTypesParams("types_binary", "field_mediumblob", "regular", makesv("\0_mediumblob"), field_type::blob),
	QueryTypesParams("types_binary", "field_mediumblob", "nonascii", makesv("\4\xfb"), field_type::blob),
	QueryTypesParams("types_binary", "field_mediumblob", "empty", "", field_type::blob),

	QueryTypesParams("types_binary", "field_longblob", "regular", makesv("\0_longblob"), field_type::blob),
	QueryTypesParams("types_binary", "field_longblob", "nonascii", makesv("\5\xfa"), field_type::blob),
	QueryTypesParams("types_binary", "field_longblob", "empty", "", field_type::blob)
));

// These types do not have a more concrete representation in the library yet.
// Check we get them as strings and we get the metadata correctly
std::uint8_t geometry_value [] = {
	0x00, 0x00, 0x00,
	0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x40
};

INSTANTIATE_TEST_SUITE_P(NOT_IMPLEMENTED_TYPES, QueryTypesTest, Values(
	QueryTypesParams("types_not_implemented", "field_bit", "regular", "\xfe", field_type::bit, flags_unsigned),
	QueryTypesParams("types_not_implemented", "field_decimal", "regular", "300", field_type::decimal),
	QueryTypesParams("types_not_implemented", "field_geometry", "regular", makesv(geometry_value), field_type::geometry)
));

// Tests for certain metadata flags and NULL values
INSTANTIATE_TEST_SUITE_P(METADATA_FLAGS, QueryTypesTest, Values(
	QueryTypesParams("types_flags", "field_timestamp", "default", nullptr, field_type::timestamp,
					 flagsvec{&field_metadata::is_set_to_now_on_update}),
	QueryTypesParams("types_flags", "field_primary_key", "default", std::int32_t(50), field_type::int_,
					 flagsvec{&field_metadata::is_primary_key, &field_metadata::is_not_null,
							  &field_metadata::is_auto_increment}),
	QueryTypesParams("types_flags", "field_not_null", "default", "char", field_type::char_,
					 flagsvec{&field_metadata::is_not_null}),
	QueryTypesParams("types_flags", "field_unique", "default", std::int32_t(21), field_type::int_,
					 flagsvec{&field_metadata::is_unique_key}),
	QueryTypesParams("types_flags", "field_indexed", "default", std::int32_t(42), field_type::int_,
					 flagsvec{&field_metadata::is_multiple_key})
));


} // anon namespace


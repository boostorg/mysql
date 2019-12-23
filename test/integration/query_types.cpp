/*
 * query_types.cpp
 *
 *  Created on: Dec 21, 2019
 *      Author: ruben
 */

#include "integration_test_common.hpp"
#include "metadata_validator.hpp"
#include <sstream>

using namespace mysql;
using namespace mysql::test;
using namespace testing;

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
	std::vector<value> expected_values ({param.expected_value});
	ASSERT_EQ(rows.size(), 1);
	EXPECT_EQ(rows[0].values(), expected_values);
}

using flagsvec = std::vector<meta_validator::flag_getter>;

const flagsvec flags_unsigned { &field_metadata::is_unsigned };
const flagsvec flags_zerofill { &field_metadata::is_unsigned, &field_metadata::is_zerofill };

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

} // anon namespace


// All int types: signed, unsigned, unsigned zerofill
// Floating points: signed, unsigned, unsigned zerofill
// Date, year
// datetime, time, timestamp: 0-6 decimals
// text types
// missing types
// key flags
// timestamp flags
// character sets and collations



//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "integration_test_common.hpp"
#include "metadata_validator.hpp"
#include "test_common.hpp"
#include <sstream>
#include <unordered_map>
#include <bitset>

using namespace boost::mysql::test;
using namespace testing;
using namespace date::literals;
using boost::mysql::value;
using boost::mysql::field_metadata;
using boost::mysql::field_type;
using boost::mysql::row;
using boost::mysql::datetime;

namespace
{

/**
 * These tests try to cover a range of the possible types and values MySQL support.
 * Given a table, a single field, and a row_id that will match the "id" field of the table,
 * we validate that we get the expected metadata and the expected value. The cases are
 * defined in SQL in db_setup.sql
 */
struct database_types_testcase
{
	std::string table;
	std::string field;
	std::string row_id;
	value expected_value;
	meta_validator mvalid;

	template <typename ValueType, typename... Args>
	database_types_testcase(std::string table, std::string field, std::string row_id,
			ValueType&& expected_value, Args&&... args) :
		table(table),
		field(field),
		row_id(move(row_id)),
		expected_value(std::forward<ValueType>(expected_value)),
		mvalid(move(table), move(field), std::forward<Args>(args)...)
	{
	}
};

std::ostream& operator<<(std::ostream& os, const database_types_testcase& v)
{
	return os << v.table << "." << v.field << "." << v.row_id;
}

// Note: NetworkTest with do_handshake=true requires GetParam() to have an ssl data member
struct DatabaseTypesTest :
	NetworkTest<boost::asio::ip::tcp::socket, database_types_testcase, false>
{
	DatabaseTypesTest()
	{
		handshake(boost::mysql::ssl_mode::disable);
	}
};

TEST_P(DatabaseTypesTest, Query_MetadataAndValueCorrect)
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
	row expected_row ({param.expected_value});
	ASSERT_EQ(rows.size(), 1);
	EXPECT_EQ(static_cast<const row&>(rows[0]), expected_row); // make gtest pick operator<<
}

TEST_P(DatabaseTypesTest, PreparedStatementExecuteResult_MetadataAndValueCorrect)
{
	// Parameter
	const auto& param = GetParam();

	// Prepare the statement
	std::ostringstream stmtss;
	stmtss << "SELECT " << param.field
			<< " FROM " << param.table
			<< " WHERE id = ?";
	auto stmt_sql = stmtss.str();
	auto stmt = conn.prepare_statement(stmt_sql);

	// Execute it with the provided parameters
	auto result = stmt.execute(makevalues(param.row_id));
	auto rows = result.fetch_all();

	// Validate the received metadata
	validate_meta(result.fields(), {param.mvalid});

	// Validate the returned value
	row expected_row ({param.expected_value});
	ASSERT_EQ(rows.size(), 1);
	EXPECT_EQ(static_cast<const row&>(rows[0]), expected_row); // make gtest pick operator<<
}

TEST_P(DatabaseTypesTest, PreparedStatementExecuteParam_ValueSerializedCorrectly)
{
	// This test is not applicable (yet) to nullptr values or bit values.
	// Doing "field = ?" where ? is nullptr never matches anything.
	// Bit values are returned as strings bit need to be sent as integers in
	// prepared statements. TODO: make this better.
	const auto& param = GetParam();
	if (param.expected_value == value(nullptr) ||
		param.mvalid.type() == field_type::bit)
	{
		return;
	}

	// Prepare the statement
	std::ostringstream stmtss;
	stmtss << "SELECT " << param.field
			<< " FROM " << param.table
			<< " WHERE id = ? AND " << param.field << " = ?";
	auto stmt_sql = stmtss.str();
	auto stmt = conn.prepare_statement(stmt_sql);

	// Execute it with the provided parameters
	auto result = stmt.execute(makevalues(param.row_id, param.expected_value));
	auto rows = result.fetch_all();

	// Validate the returned value
	row expected_row ({param.expected_value});
	ASSERT_EQ(rows.size(), 1);
	EXPECT_EQ(static_cast<const row&>(rows[0]), expected_row); // make gtest pick operator<<
}

using flagsvec = std::vector<meta_validator::flag_getter>;

const flagsvec no_flags {};
const flagsvec flags_unsigned { &field_metadata::is_unsigned };
const flagsvec flags_zerofill { &field_metadata::is_unsigned, &field_metadata::is_zerofill };

// Integers
INSTANTIATE_TEST_SUITE_P(TINYINT, DatabaseTypesTest, Values(
	database_types_testcase("types_tinyint", "field_signed", "regular", std::int32_t(20), field_type::tinyint),
	database_types_testcase("types_tinyint", "field_signed", "negative", std::int32_t(-20), field_type::tinyint),
	database_types_testcase("types_tinyint", "field_signed", "min", std::int32_t(-0x80), field_type::tinyint),
	database_types_testcase("types_tinyint", "field_signed", "max", std::int32_t(0x7f), field_type::tinyint),

	database_types_testcase("types_tinyint", "field_unsigned", "regular", std::uint32_t(20), field_type::tinyint, flags_unsigned),
	database_types_testcase("types_tinyint", "field_unsigned", "min", std::uint32_t(0), field_type::tinyint, flags_unsigned),
	database_types_testcase("types_tinyint", "field_unsigned", "max", std::uint32_t(0xff), field_type::tinyint, flags_unsigned),

	database_types_testcase("types_tinyint", "field_width", "regular", std::int32_t(20), field_type::tinyint),
	database_types_testcase("types_tinyint", "field_width", "negative", std::int32_t(-20), field_type::tinyint),

	database_types_testcase("types_tinyint", "field_zerofill", "regular", std::uint32_t(20), field_type::tinyint, flags_zerofill),
	database_types_testcase("types_tinyint", "field_zerofill", "min", std::uint32_t(0), field_type::tinyint, flags_zerofill)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(SMALLINT, DatabaseTypesTest, Values(
	database_types_testcase("types_smallint", "field_signed", "regular", std::int32_t(20), field_type::smallint),
	database_types_testcase("types_smallint", "field_signed", "negative", std::int32_t(-20), field_type::smallint),
	database_types_testcase("types_smallint", "field_signed", "min", std::int32_t(-0x8000), field_type::smallint),
	database_types_testcase("types_smallint", "field_signed", "max", std::int32_t(0x7fff), field_type::smallint),

	database_types_testcase("types_smallint", "field_unsigned", "regular", std::uint32_t(20), field_type::smallint, flags_unsigned),
	database_types_testcase("types_smallint", "field_unsigned", "min", std::uint32_t(0), field_type::smallint, flags_unsigned),
	database_types_testcase("types_smallint", "field_unsigned", "max", std::uint32_t(0xffff), field_type::smallint, flags_unsigned),

	database_types_testcase("types_smallint", "field_width", "regular", std::int32_t(20), field_type::smallint),
	database_types_testcase("types_smallint", "field_width", "negative", std::int32_t(-20), field_type::smallint),

	database_types_testcase("types_smallint", "field_zerofill", "regular", std::uint32_t(20), field_type::smallint, flags_zerofill),
	database_types_testcase("types_smallint", "field_zerofill", "min", std::uint32_t(0), field_type::smallint, flags_zerofill)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(MEDIUMINT, DatabaseTypesTest, Values(
	database_types_testcase("types_mediumint", "field_signed", "regular", std::int32_t(20), field_type::mediumint),
	database_types_testcase("types_mediumint", "field_signed", "negative", std::int32_t(-20), field_type::mediumint),
	database_types_testcase("types_mediumint", "field_signed", "min", std::int32_t(-0x800000), field_type::mediumint),
	database_types_testcase("types_mediumint", "field_signed", "max", std::int32_t(0x7fffff), field_type::mediumint),

	database_types_testcase("types_mediumint", "field_unsigned", "regular", std::uint32_t(20), field_type::mediumint, flags_unsigned),
	database_types_testcase("types_mediumint", "field_unsigned", "min", std::uint32_t(0), field_type::mediumint, flags_unsigned),
	database_types_testcase("types_mediumint", "field_unsigned", "max", std::uint32_t(0xffffff), field_type::mediumint, flags_unsigned),

	database_types_testcase("types_mediumint", "field_width", "regular", std::int32_t(20), field_type::mediumint),
	database_types_testcase("types_mediumint", "field_width", "negative", std::int32_t(-20), field_type::mediumint),

	database_types_testcase("types_mediumint", "field_zerofill", "regular", std::uint32_t(20), field_type::mediumint, flags_zerofill),
	database_types_testcase("types_mediumint", "field_zerofill", "min", std::uint32_t(0), field_type::mediumint, flags_zerofill)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(INT, DatabaseTypesTest, Values(
	database_types_testcase("types_int", "field_signed", "regular", std::int32_t(20), field_type::int_),
	database_types_testcase("types_int", "field_signed", "negative", std::int32_t(-20), field_type::int_),
	database_types_testcase("types_int", "field_signed", "min", std::int32_t(-0x80000000), field_type::int_),
	database_types_testcase("types_int", "field_signed", "max", std::int32_t(0x7fffffff), field_type::int_),

	database_types_testcase("types_int", "field_unsigned", "regular", std::uint32_t(20), field_type::int_, flags_unsigned),
	database_types_testcase("types_int", "field_unsigned", "min", std::uint32_t(0), field_type::int_, flags_unsigned),
	database_types_testcase("types_int", "field_unsigned", "max", std::uint32_t(0xffffffff), field_type::int_, flags_unsigned),

	database_types_testcase("types_int", "field_width", "regular", std::int32_t(20), field_type::int_),
	database_types_testcase("types_int", "field_width", "negative", std::int32_t(-20), field_type::int_),

	database_types_testcase("types_int", "field_zerofill", "regular", std::uint32_t(20), field_type::int_, flags_zerofill),
	database_types_testcase("types_int", "field_zerofill", "min", std::uint32_t(0), field_type::int_, flags_zerofill)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(BIGINT, DatabaseTypesTest, Values(
	database_types_testcase("types_bigint", "field_signed", "regular", std::int64_t(20), field_type::bigint),
	database_types_testcase("types_bigint", "field_signed", "negative", std::int64_t(-20), field_type::bigint),
	database_types_testcase("types_bigint", "field_signed", "min", std::int64_t(-0x8000000000000000), field_type::bigint),
	database_types_testcase("types_bigint", "field_signed", "max", std::int64_t(0x7fffffffffffffff), field_type::bigint),

	database_types_testcase("types_bigint", "field_unsigned", "regular", std::uint64_t(20), field_type::bigint, flags_unsigned),
	database_types_testcase("types_bigint", "field_unsigned", "min", std::uint64_t(0), field_type::bigint, flags_unsigned),
	database_types_testcase("types_bigint", "field_unsigned", "max", std::uint64_t(0xffffffffffffffff), field_type::bigint, flags_unsigned),

	database_types_testcase("types_bigint", "field_width", "regular", std::int64_t(20), field_type::bigint),
	database_types_testcase("types_bigint", "field_width", "negative", std::int64_t(-20), field_type::bigint),

	database_types_testcase("types_bigint", "field_zerofill", "regular", std::uint64_t(20), field_type::bigint, flags_zerofill),
	database_types_testcase("types_bigint", "field_zerofill", "min", std::uint64_t(0), field_type::bigint, flags_zerofill)
), test_name_generator);

// Floating point
INSTANTIATE_TEST_SUITE_P(FLOAT, DatabaseTypesTest, Values(
	database_types_testcase("types_float", "field_signed", "zero", 0.0f, field_type::float_, no_flags, 31),
	database_types_testcase("types_float", "field_signed", "int_positive", 4.0f, field_type::float_, no_flags, 31),
	database_types_testcase("types_float", "field_signed", "int_negative", -4.0f, field_type::float_, no_flags, 31),
	database_types_testcase("types_float", "field_signed", "fractional_positive", 4.2f, field_type::float_, no_flags, 31),
	database_types_testcase("types_float", "field_signed", "fractional_negative", -4.2f, field_type::float_, no_flags, 31),
	database_types_testcase("types_float", "field_signed", "positive_exp_positive_int", 3e20f, field_type::float_, no_flags, 31),
	database_types_testcase("types_float", "field_signed", "positive_exp_negative_int", -3e20f, field_type::float_, no_flags, 31),
	database_types_testcase("types_float", "field_signed", "positive_exp_positive_fractional", 3.14e20f, field_type::float_, no_flags, 31),
	database_types_testcase("types_float", "field_signed", "positive_exp_negative_fractional", -3.14e20f, field_type::float_, no_flags, 31),
	database_types_testcase("types_float", "field_signed", "negative_exp_positive_fractional", 3.14e-20f, field_type::float_, no_flags, 31),

	database_types_testcase("types_float", "field_unsigned", "zero", 0.0f, field_type::float_, flags_unsigned, 31),
	database_types_testcase("types_float", "field_unsigned", "fractional_positive", 4.2f, field_type::float_, flags_unsigned, 31),

	database_types_testcase("types_float", "field_width", "zero", 0.0f, field_type::float_, no_flags, 10),
	database_types_testcase("types_float", "field_width", "fractional_positive", 4.2f, field_type::float_, no_flags, 10),
	database_types_testcase("types_float", "field_width", "fractional_negative", -4.2f, field_type::float_, no_flags, 10),

	database_types_testcase("types_float", "field_zerofill", "zero", 0.0f, field_type::float_, flags_zerofill, 31),
	database_types_testcase("types_float", "field_zerofill", "fractional_positive", 4.2f, field_type::float_, flags_zerofill, 31),
	database_types_testcase("types_float", "field_zerofill", "positive_exp_positive_fractional", 3.14e20f, field_type::float_, flags_zerofill, 31),
	database_types_testcase("types_float", "field_zerofill", "negative_exp_positive_fractional", 3.14e-20f, field_type::float_, flags_zerofill, 31)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(DOUBLE, DatabaseTypesTest, Values(
	database_types_testcase("types_double", "field_signed", "zero", 0.0, field_type::double_, no_flags, 31),
	database_types_testcase("types_double", "field_signed", "int_positive", 4.0, field_type::double_, no_flags, 31),
	database_types_testcase("types_double", "field_signed", "int_negative", -4.0, field_type::double_, no_flags, 31),
	database_types_testcase("types_double", "field_signed", "fractional_positive", 4.2, field_type::double_, no_flags, 31),
	database_types_testcase("types_double", "field_signed", "fractional_negative", -4.2, field_type::double_, no_flags, 31),
	database_types_testcase("types_double", "field_signed", "positive_exp_positive_int", 3e200, field_type::double_, no_flags, 31),
	database_types_testcase("types_double", "field_signed", "positive_exp_negative_int", -3e200, field_type::double_, no_flags, 31),
	database_types_testcase("types_double", "field_signed", "positive_exp_positive_fractional", 3.14e200, field_type::double_, no_flags, 31),
	database_types_testcase("types_double", "field_signed", "positive_exp_negative_fractional", -3.14e200, field_type::double_, no_flags, 31),
	database_types_testcase("types_double", "field_signed", "negative_exp_positive_fractional", 3.14e-200, field_type::double_, no_flags, 31),

	database_types_testcase("types_double", "field_unsigned", "zero", 0.0, field_type::double_, flags_unsigned, 31),
	database_types_testcase("types_double", "field_unsigned", "fractional_positive", 4.2, field_type::double_, flags_unsigned, 31),

	database_types_testcase("types_double", "field_width", "zero", 0.0, field_type::double_, no_flags, 10),
	database_types_testcase("types_double", "field_width", "fractional_positive", 4.2, field_type::double_, no_flags, 10),
	database_types_testcase("types_double", "field_width", "fractional_negative", -4.2, field_type::double_, no_flags, 10),

	database_types_testcase("types_double", "field_zerofill", "zero", 0.0, field_type::double_, flags_zerofill, 31),
	database_types_testcase("types_double", "field_zerofill", "fractional_positive", 4.2, field_type::double_, flags_zerofill, 31),
	database_types_testcase("types_double", "field_zerofill", "positive_exp_positive_fractional", 3.14e200, field_type::double_, flags_zerofill, 31),
	database_types_testcase("types_double", "field_zerofill", "negative_exp_positive_fractional", 3.14e-200, field_type::double_, flags_zerofill, 31)
), test_name_generator);

// Dates and times
INSTANTIATE_TEST_SUITE_P(DATE, DatabaseTypesTest, Values(
	database_types_testcase("types_date", "field_date", "regular", 2010_y/3/28_d, field_type::date),
	database_types_testcase("types_date", "field_date", "leap", 1788_y/2/29_d, field_type::date),
	database_types_testcase("types_date", "field_date", "min", 1000_y/1/1_d, field_type::date),
	database_types_testcase("types_date", "field_date", "max", 9999_y/12/31_d, field_type::date)
), test_name_generator);

// Infrastructure to generate DATETIME and TIMESTAMP test cases

// Given a number of microseconds, removes the least significant part according to decimals
int round_micros(int input, int decimals)
{
	assert(decimals >= 0 && decimals <= 6);
	if (decimals == 0) return 0;
	auto modulus = static_cast<int>(std::pow(10, 6 - decimals));
	return (input / modulus) * modulus;
}

std::chrono::microseconds round_micros(std::chrono::microseconds input, int decimals)
{
	return std::chrono::microseconds(round_micros(static_cast<int>(input.count()), decimals));
}

std::pair<std::string, datetime> datetime_from_id(std::bitset<4> id, int decimals)
{
	// id represents which components (h, m, s, u) should the test case have
	constexpr struct
	{
		char letter;
		std::chrono::microseconds offset;
	} bit_meaning [] = {
		{ 'h', std::chrono::hours(23) }, // bit 0
		{ 'm', std::chrono::minutes(1) },
		{ 's', std::chrono::seconds(50) },
		{ 'u', std::chrono::microseconds(123456) }
	};

	std::string name;
	datetime dt = makedt(2010, 5, 2); // components all tests have

	for (std::size_t i = 0; i < id.size(); ++i)
	{
		if (id[i]) // component present
		{
			char letter = bit_meaning[i].letter;
			auto offset = bit_meaning[i].offset;
			name.push_back(letter); // add to name
			dt += letter == 'u' ? round_micros(offset, decimals) : offset; // add to value
		}
	}
	if (name.empty()) name = "date";

	return {name, dt};
}

database_types_testcase create_datetime_testcase(
	int decimals,
	std::string id,
	value expected,
	field_type type
)
{
	static std::unordered_map<field_type, const char*> table_map {
		{ field_type::datetime, "types_datetime" },
		{ field_type::timestamp, "types_timestamp" },
		{ field_type::time, "types_time" }
	};
	// Inconsistencies between Maria and MySQL in the unsigned flag
	// we don't really care here about signedness of timestamps
	return database_types_testcase(
		table_map.at(type),
		"field_" + std::to_string(decimals),
		std::move(id),
		expected,
		type,
		no_flags,
		decimals,
		flagsvec{ &field_metadata::is_unsigned }
	);
}

std::pair<std::string, boost::mysql::time> time_from_id(std::bitset<6> id, int decimals)
{
	// id represents which components (h, m, s, u) should the test case have
	constexpr struct
	{
		char letter;
		std::chrono::microseconds offset;
	} bit_meaning [] = {
		{ 'n', std::chrono::hours(0) }, // bit 0: negative bit
		{ 'd', std::chrono::hours(48) }, // bit 1
		{ 'h', std::chrono::hours(23) }, // bit 2
		{ 'm', std::chrono::minutes(1) },
		{ 's', std::chrono::seconds(50) },
		{ 'u', std::chrono::microseconds(123456) }
	};

	std::string name;
	boost::mysql::time t {0};

	for (std::size_t i = 1; i < id.size(); ++i)
	{
		if (id[i]) // component present
		{
			char letter = bit_meaning[i].letter;
			auto offset = bit_meaning[i].offset;
			name.push_back(letter); // add to name
			t += letter == 'u' ? round_micros(offset, decimals) : offset; // add to value
		}
	}
	if (name.empty()) name = "zero";
	if (id[0]) // bit sign
	{
		name = "negative_" + name;
		t = -t;
	}

	return {name, t};
}

// shared between DATETIME and TIMESTAMP
std::vector<database_types_testcase> generate_common_datetime_cases(
	field_type type
)
{
	std::vector<database_types_testcase> res;

	for (int decimals = 0; decimals <= 6; ++decimals)
	{
		// Regular values
		auto max_int_id = static_cast<std::size_t>(std::pow(2, 4)); // 4 components can be varied
		for (std::size_t int_id = 0; int_id < max_int_id; ++int_id)
		{
			std::bitset<4> bitset_id (int_id);
			if (bitset_id[3] && decimals == 0) continue; // cases with micros don't make sense for fields with no decimals
			auto [id, value] = datetime_from_id(int_id, decimals);
			res.push_back(create_datetime_testcase(decimals, move(id), value, type));
		}
	}

	return res;
}

std::vector<database_types_testcase> generate_datetime_cases()
{
	auto res = generate_common_datetime_cases(field_type::datetime);

	// min and max
	for (int decimals = 0; decimals <= 6; ++decimals)
	{
		res.push_back(create_datetime_testcase(decimals, "min",
				makedt(1000, 1, 1), field_type::datetime));
		res.push_back(create_datetime_testcase(decimals, "max",
				makedt(9999, 12, 31, 23, 59, 59, round_micros(999999, decimals)), field_type::datetime));
	}

	return res;
}

std::vector<database_types_testcase> generate_timestamp_cases()
{
	return generate_common_datetime_cases(field_type::timestamp);
}

std::vector<database_types_testcase> generate_time_cases()
{
	std::vector<database_types_testcase> res;

	for (int decimals = 0; decimals <= 6; ++decimals)
	{
		// Regular values
		auto max_int_id = static_cast<std::size_t>(std::pow(2, 6)); // 6 components can be varied
		for (std::size_t int_id = 0; int_id < max_int_id; ++int_id)
		{
			std::bitset<6> bitset_id (int_id);
			if (bitset_id[5] && decimals == 0) continue; // cases with micros don't make sense for fields with no decimals
			if (bitset_id.to_ulong() == 1) continue; // negative zero does not make sense
			auto [id, value] = time_from_id(int_id, decimals);
			res.push_back(create_datetime_testcase(decimals, move(id), value, field_type::time));
		}

		// min and max
		auto max_value = decimals == 0 ? maket(838, 59, 59) : maket(838, 59, 58, round_micros(999999, decimals));
		res.push_back(create_datetime_testcase(decimals, "min", -max_value, field_type::time));
		res.push_back(create_datetime_testcase(decimals, "max",  max_value, field_type::time));
	}

	return res;

}

INSTANTIATE_TEST_SUITE_P(DATETIME,  DatabaseTypesTest, ValuesIn(generate_datetime_cases()),  test_name_generator);
INSTANTIATE_TEST_SUITE_P(TIMESTAMP, DatabaseTypesTest, ValuesIn(generate_timestamp_cases()), test_name_generator);
INSTANTIATE_TEST_SUITE_P(TIME,      DatabaseTypesTest, ValuesIn(generate_time_cases()),      test_name_generator);

INSTANTIATE_TEST_SUITE_P(YEAR, DatabaseTypesTest, Values(
	database_types_testcase("types_year", "field_default", "regular", std::uint32_t(2019), field_type::year, flags_zerofill),
	database_types_testcase("types_year", "field_default", "min", std::uint32_t(1901), field_type::year, flags_zerofill),
	database_types_testcase("types_year", "field_default", "max", std::uint32_t(2155), field_type::year, flags_zerofill),
	database_types_testcase("types_year", "field_default", "zero", std::uint32_t(0), field_type::year, flags_zerofill)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(STRING, DatabaseTypesTest, Values(
	database_types_testcase("types_string", "field_char", "regular", "test_char", field_type::char_),
	database_types_testcase("types_string", "field_char", "utf8", u8"\u00F1", field_type::char_),
	database_types_testcase("types_string", "field_char", "empty", "", field_type::char_),

	database_types_testcase("types_string", "field_varchar", "regular", "test_varchar", field_type::varchar),
	database_types_testcase("types_string", "field_varchar", "utf8", u8"\u00D1", field_type::varchar),
	database_types_testcase("types_string", "field_varchar", "empty", "", field_type::varchar),

	database_types_testcase("types_string", "field_tinytext", "regular", "test_tinytext", field_type::text),
	database_types_testcase("types_string", "field_tinytext", "utf8", u8"\u00e1", field_type::text),
	database_types_testcase("types_string", "field_tinytext", "empty", "", field_type::text),

	database_types_testcase("types_string", "field_text", "regular", "test_text", field_type::text),
	database_types_testcase("types_string", "field_text", "utf8", u8"\u00e9", field_type::text),
	database_types_testcase("types_string", "field_text", "empty", "", field_type::text),

	database_types_testcase("types_string", "field_mediumtext", "regular", "test_mediumtext", field_type::text),
	database_types_testcase("types_string", "field_mediumtext", "utf8", u8"\u00ed", field_type::text),
	database_types_testcase("types_string", "field_mediumtext", "empty", "", field_type::text),

	database_types_testcase("types_string", "field_longtext", "regular", "test_longtext", field_type::text),
	database_types_testcase("types_string", "field_longtext", "utf8", u8"\u00f3", field_type::text),
	database_types_testcase("types_string", "field_longtext", "empty", "", field_type::text),

	database_types_testcase("types_string", "field_enum", "regular", "red", field_type::enum_),

	database_types_testcase("types_string", "field_set", "regular", "red,green", field_type::set),
	database_types_testcase("types_string", "field_set", "empty", "", field_type::set)
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(BINARY, DatabaseTypesTest, Values(
	// BINARY values get padded with zeros to the declared length
	database_types_testcase("types_binary", "field_binary", "regular", makesv("\0_binary\0\0"), field_type::binary),
	database_types_testcase("types_binary", "field_binary", "nonascii", makesv("\0\xff" "\0\0\0\0\0\0\0\0"), field_type::binary),
	database_types_testcase("types_binary", "field_binary", "empty", makesv("\0\0\0\0\0\0\0\0\0\0"), field_type::binary),

	database_types_testcase("types_binary", "field_varbinary", "regular", makesv("\0_varbinary"), field_type::varbinary),
	database_types_testcase("types_binary", "field_varbinary", "nonascii", makesv("\1\xfe"), field_type::varbinary),
	database_types_testcase("types_binary", "field_varbinary", "empty", "", field_type::varbinary),

	database_types_testcase("types_binary", "field_tinyblob", "regular", makesv("\0_tinyblob"), field_type::blob),
	database_types_testcase("types_binary", "field_tinyblob", "nonascii", makesv("\2\xfd"), field_type::blob),
	database_types_testcase("types_binary", "field_tinyblob", "empty", "", field_type::blob),

	database_types_testcase("types_binary", "field_blob", "regular", makesv("\0_blob"), field_type::blob),
	database_types_testcase("types_binary", "field_blob", "nonascii", makesv("\3\xfc"), field_type::blob),
	database_types_testcase("types_binary", "field_blob", "empty", "", field_type::blob),

	database_types_testcase("types_binary", "field_mediumblob", "regular", makesv("\0_mediumblob"), field_type::blob),
	database_types_testcase("types_binary", "field_mediumblob", "nonascii", makesv("\4\xfb"), field_type::blob),
	database_types_testcase("types_binary", "field_mediumblob", "empty", "", field_type::blob),

	database_types_testcase("types_binary", "field_longblob", "regular", makesv("\0_longblob"), field_type::blob),
	database_types_testcase("types_binary", "field_longblob", "nonascii", makesv("\5\xfa"), field_type::blob),
	database_types_testcase("types_binary", "field_longblob", "empty", "", field_type::blob)
), test_name_generator);

// These types do not have a more concrete representation in the library yet.
// Check we get them as strings and we get the metadata correctly
std::uint8_t geometry_value [] = {
	0x00, 0x00, 0x00,
	0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x40
};

INSTANTIATE_TEST_SUITE_P(NOT_IMPLEMENTED_TYPES, DatabaseTypesTest, Values(
	database_types_testcase("types_not_implemented", "field_bit", "regular", "\xfe", field_type::bit, flags_unsigned),
	database_types_testcase("types_not_implemented", "field_decimal", "regular", "300", field_type::decimal),
	database_types_testcase("types_not_implemented", "field_geometry", "regular", makesv(geometry_value), field_type::geometry)
), test_name_generator);

// Tests for certain metadata flags and NULL values
INSTANTIATE_TEST_SUITE_P(METADATA_FLAGS, DatabaseTypesTest, Values(
	database_types_testcase("types_flags", "field_timestamp", "default", nullptr, field_type::timestamp,
					 flagsvec{&field_metadata::is_set_to_now_on_update}, 0, flagsvec{&field_metadata::is_unsigned}),
	database_types_testcase("types_flags", "field_primary_key", "default", std::int32_t(50), field_type::int_,
					 flagsvec{&field_metadata::is_primary_key, &field_metadata::is_not_null,
							  &field_metadata::is_auto_increment}),
	database_types_testcase("types_flags", "field_not_null", "default", "char", field_type::char_,
					 flagsvec{&field_metadata::is_not_null}),
	database_types_testcase("types_flags", "field_unique", "default", std::int32_t(21), field_type::int_,
					 flagsvec{&field_metadata::is_unique_key}),
	database_types_testcase("types_flags", "field_indexed", "default", std::int32_t(42), field_type::int_,
					 flagsvec{&field_metadata::is_multiple_key})
), test_name_generator);


} // anon namespace


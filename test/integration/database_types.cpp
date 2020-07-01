//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "integration_test_common.hpp"
#include "metadata_validator.hpp"
#include "test_common.hpp"
#include <boost/test/data/monomorphic/collection.hpp>
#include <boost/test/data/test_case.hpp>
#include <map>
#include <bitset>

using namespace boost::mysql::test;
using namespace boost::unit_test;
using boost::mysql::value;
using boost::mysql::field_metadata;
using boost::mysql::field_type;
using boost::mysql::row;
using boost::mysql::datetime;
using boost::mysql::make_values;

BOOST_AUTO_TEST_SUITE(test_database_types)

/**
 * These tests try to cover a range of the possible types and values MySQL support.
 * Given a table, a single field, and a row_id that will match the "id" field of the table,
 * we validate that we get the expected metadata and the expected value. The cases are
 * defined in SQL in db_setup.sql
 */
struct database_types_sample
{
    std::string table;
    std::string field;
    std::string row_id;
    value expected_value;
    meta_validator mvalid;

    template <class ValueType, class... Args>
    database_types_sample(std::string table, std::string field, std::string row_id,
            ValueType&& expected_value, Args&&... args) :
        table(table),
        field(field),
        row_id(move(row_id)),
        expected_value(std::forward<ValueType>(expected_value)),
        mvalid(move(table), move(field), std::forward<Args>(args)...)
    {
    }
};

std::ostream& operator<<(std::ostream& os, const database_types_sample& input)
{
    return os << input.table << '.' << input.field << '.' << input.row_id;
}

// Fixture
struct database_types_fixture : network_fixture<boost::asio::ip::tcp::socket>
{
    database_types_fixture()
    {
        connect(boost::mysql::ssl_mode::disable);
    }
};

// Helpers
using flagsvec = std::vector<meta_validator::flag_getter>;

const flagsvec no_flags {};
const flagsvec flags_unsigned { &field_metadata::is_unsigned };
const flagsvec flags_zerofill { &field_metadata::is_unsigned, &field_metadata::is_zerofill };

// Int cases
void add_int_samples_helper(
    const char* table_name,
    field_type type,
    std::int64_t signed_min,
    std::int64_t signed_max,
    std::uint64_t unsigned_max,
    std::vector<database_types_sample>& output
)
{
    output.emplace_back(table_name, "field_signed", "regular", std::int64_t(20), type);
    output.emplace_back(table_name, "field_signed", "negative", std::int64_t(-20), type);
    output.emplace_back(table_name, "field_signed", "min", signed_min, type);
    output.emplace_back(table_name, "field_signed", "max", signed_max, type);

    output.emplace_back(table_name, "field_unsigned", "regular", std::uint64_t(20), type, flags_unsigned);
    output.emplace_back(table_name, "field_unsigned", "min", std::uint64_t(0), type, flags_unsigned);
    output.emplace_back(table_name, "field_unsigned", "max", unsigned_max, type, flags_unsigned);

    output.emplace_back(table_name, "field_width", "regular", std::int64_t(20), type);
    output.emplace_back(table_name, "field_width", "negative", std::int64_t(-20), type);

    output.emplace_back(table_name, "field_zerofill", "regular", std::uint64_t(20), type, flags_zerofill);
    output.emplace_back(table_name, "field_zerofill", "min", std::uint64_t(0), type, flags_zerofill);
}

void add_int_samples(std::vector<database_types_sample>& output)
{
    add_int_samples_helper("types_tinyint", field_type::tinyint,
        -0x80, 0x7f, 0xff, output);
    add_int_samples_helper("types_smallint", field_type::smallint,
        -0x8000, 0x7fff, 0xffff, output);
    add_int_samples_helper("types_mediumint", field_type::mediumint,
        -0x800000, 0x7fffff, 0xffffff, output);
    add_int_samples_helper("types_int", field_type::int_,
        -0x80000000LL, 0x7fffffff, 0xffffffff, output);
    add_int_samples_helper("types_bigint", field_type::bigint,
        -0x7fffffffffffffff - 1, 0x7fffffffffffffff, 0xffffffffffffffff, output);
}

// Floating point cases
void add_float_samples(std::vector<database_types_sample>& output)
{
    output.emplace_back("types_float", "field_signed", "zero",
        0.0f, field_type::float_, no_flags, 31);
    output.emplace_back("types_float", "field_signed", "int_positive",
        4.0f, field_type::float_, no_flags, 31);
    output.emplace_back("types_float", "field_signed", "int_negative",
        -4.0f, field_type::float_, no_flags, 31);
    output.emplace_back("types_float", "field_signed", "fractional_positive",
        4.2f, field_type::float_, no_flags, 31);
    output.emplace_back("types_float", "field_signed", "fractional_negative",
        -4.2f, field_type::float_, no_flags, 31);
    output.emplace_back("types_float", "field_signed", "positive_exp_positive_int",
        3e20f, field_type::float_, no_flags, 31);
    output.emplace_back("types_float", "field_signed", "positive_exp_negative_int",
        -3e20f, field_type::float_, no_flags, 31);
    output.emplace_back("types_float", "field_signed", "positive_exp_positive_fractional",
        3.14e20f, field_type::float_, no_flags, 31);
    output.emplace_back("types_float", "field_signed", "positive_exp_negative_fractional",
        -3.14e20f, field_type::float_, no_flags, 31);
    output.emplace_back("types_float", "field_signed", "negative_exp_positive_fractional",
        3.14e-20f, field_type::float_, no_flags, 31);

    output.emplace_back("types_float", "field_unsigned", "zero",
        0.0f, field_type::float_, flags_unsigned, 31);
    output.emplace_back("types_float", "field_unsigned", "fractional_positive",
        4.2f, field_type::float_, flags_unsigned, 31);

    output.emplace_back("types_float", "field_width", "zero",
        0.0f, field_type::float_, no_flags, 10);
    output.emplace_back("types_float", "field_width", "fractional_positive",
        4.2f, field_type::float_, no_flags, 10);
    output.emplace_back("types_float", "field_width", "fractional_negative",
        -4.2f, field_type::float_, no_flags, 10);

    output.emplace_back("types_float", "field_zerofill", "zero",
        0.0f, field_type::float_, flags_zerofill, 31);
    output.emplace_back("types_float", "field_zerofill", "fractional_positive",
        4.2f, field_type::float_, flags_zerofill, 31);
    output.emplace_back("types_float", "field_zerofill", "positive_exp_positive_fractional",
        3.14e20f, field_type::float_, flags_zerofill, 31);
    output.emplace_back("types_float", "field_zerofill", "negative_exp_positive_fractional",
        3.14e-20f, field_type::float_, flags_zerofill, 31);
}

void add_double_samples(std::vector<database_types_sample>& output)
{
    output.emplace_back("types_double", "field_signed", "zero",
        0.0, field_type::double_, no_flags, 31);
    output.emplace_back("types_double", "field_signed", "int_positive",
        4.0, field_type::double_, no_flags, 31);
    output.emplace_back("types_double", "field_signed", "int_negative",
        -4.0, field_type::double_, no_flags, 31);
    output.emplace_back("types_double", "field_signed", "fractional_positive",
        4.2, field_type::double_, no_flags, 31);
    output.emplace_back("types_double", "field_signed", "fractional_negative",
        -4.2, field_type::double_, no_flags, 31);
    output.emplace_back("types_double", "field_signed", "positive_exp_positive_int",
        3e200, field_type::double_, no_flags, 31);
    output.emplace_back("types_double", "field_signed", "positive_exp_negative_int",
        -3e200, field_type::double_, no_flags, 31);
    output.emplace_back("types_double", "field_signed", "positive_exp_positive_fractional",
        3.14e200, field_type::double_, no_flags, 31);
    output.emplace_back("types_double", "field_signed", "positive_exp_negative_fractional",
        -3.14e200, field_type::double_, no_flags, 31);
    output.emplace_back("types_double", "field_signed", "negative_exp_positive_fractional",
        3.14e-200, field_type::double_, no_flags, 31);

    output.emplace_back("types_double", "field_unsigned", "zero",
        0.0, field_type::double_, flags_unsigned, 31);
    output.emplace_back("types_double", "field_unsigned", "fractional_positive",
        4.2, field_type::double_, flags_unsigned, 31);

    output.emplace_back("types_double", "field_width", "zero",
        0.0, field_type::double_, no_flags, 10);
    output.emplace_back("types_double", "field_width", "fractional_positive",
        4.2, field_type::double_, no_flags, 10);
    output.emplace_back("types_double", "field_width", "fractional_negative",
        -4.2, field_type::double_, no_flags, 10);

    output.emplace_back("types_double", "field_zerofill", "zero",
        0.0, field_type::double_, flags_zerofill, 31);
    output.emplace_back("types_double", "field_zerofill", "fractional_positive",
        4.2, field_type::double_, flags_zerofill, 31);
    output.emplace_back("types_double", "field_zerofill", "positive_exp_positive_fractional",
        3.14e200, field_type::double_, flags_zerofill, 31);
    output.emplace_back("types_double", "field_zerofill", "negative_exp_positive_fractional",
        3.14e-200, field_type::double_, flags_zerofill, 31);
}

// Date cases
// MySQL accepts zero and invalid dates. We represent them as NULL
constexpr const char* invalid_date_cases [] = {
    "zero",
    "yzero_mzero_dregular",
    "yzero_mregular_dzero",
    "yzero_invalid_date",
    "yregular_mzero_dzero",
    "yregular_mzero_dregular",
    "yregular_mregular_dzero",
    "yregular_invalid_date",
    "yregular_invalid_date_leapregular",
    "yregular_invalid_date_leap100"
};

void add_date_samples(std::vector<database_types_sample>& output)
{
    constexpr auto type = field_type::date;
    constexpr const char* table = "types_date";
    constexpr const char* field = "field_date";

    // Regular cases
    output.emplace_back(table, field, "regular", makedate(2010, 3, 28), type);
    output.emplace_back(table, field, "leap_regular", makedate(1788, 2, 29), type);
    output.emplace_back(table, field, "leap_400", makedate(2000, 2, 29), type);
    output.emplace_back(table, field, "min", makedate(0, 1, 1), type);
    output.emplace_back(table, field, "max", makedate(9999, 12, 31), type);

    // Invalid DATEs
    for (const char* invalid_case: invalid_date_cases)
    {
        output.emplace_back(table, field, invalid_case, nullptr, type);
    }
}

// Datetime and time cases

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

database_types_sample create_datetime_sample(
    int decimals,
    std::string id,
    value expected,
    field_type type
)
{
    static std::map<field_type, const char*> table_map {
        { field_type::datetime, "types_datetime" },
        { field_type::timestamp, "types_timestamp" },
        { field_type::time, "types_time" }
    };
    // Inconsistencies between Maria and MySQL in the unsigned flag
    // we don't really care here about signedness of timestamps
    return database_types_sample(
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
    if (name.empty())
        name = "zero";
    if (id[0]) // bit sign
    {
        name = "negative_" + name;
        t = -t;
    }

    return {name, t};
}

// shared between DATETIME and TIMESTAMP
void add_common_datetime_samples(
    field_type type,
    std::vector<database_types_sample>& output
)
{
    for (int decimals = 0; decimals <= 6; ++decimals)
    {
        // Regular values
        auto max_int_id = static_cast<std::size_t>(std::pow(2, 4)); // 4 components can be varied
        for (std::size_t int_id = 0; int_id < max_int_id; ++int_id)
        {
            std::bitset<4> bitset_id (int_id);
            if (bitset_id[3] && decimals == 0)
                continue; // cases with micros don't make sense for fields with no decimals
            auto dt = datetime_from_id(int_id, decimals);
            output.push_back(create_datetime_sample(decimals, move(dt.first), value(dt.second), type));
        }

        // Tests for leap years (valid dates)
        output.push_back(create_datetime_sample(
            decimals, "date_leap4", value(makedt(2004, 2, 29)), type));
        output.push_back(create_datetime_sample(
            decimals, "date_leap400", value(makedt(2000, 2, 29)), type));
    }
}

void add_datetime_samples(std::vector<database_types_sample>& output)
{
    add_common_datetime_samples(field_type::datetime, output);

    for (int decimals = 0; decimals <= 6; ++decimals)
    {
        // min and max
        output.push_back(create_datetime_sample(decimals, "min",
                value(makedt(0, 1, 1)), field_type::datetime));
        output.push_back(create_datetime_sample(decimals, "max",
                value(makedt(9999, 12, 31, 23, 59, 59,
                    round_micros(999999, decimals))), field_type::datetime));

        // invalid dates
        const char* lengths [] = {"date", "hms", decimals ? "hmsu" : nullptr };
        for (const char* length:  lengths)
        {
            if (!length)
                continue;
            for (const char* invalid_date_case: invalid_date_cases)
            {
                output.push_back(create_datetime_sample(
                    decimals,
                    stringize(length, '_', invalid_date_case),
                    value(nullptr),
                    field_type::datetime
                ));
            }
        }
    }
}

void add_timestamp_samples(std::vector<database_types_sample>& output)
{
    add_common_datetime_samples(field_type::timestamp, output);

    // Only the full-zero TIMESTAMP is allowed - timestamps with
    // invalid date parts are converted to the zero TIMESTAMP
    for (int decimals = 0; decimals <= 6; ++decimals)
    {
        output.push_back(create_datetime_sample(decimals,
            "zero", value(nullptr), field_type::timestamp));
    }
}

void add_time_samples(std::vector<database_types_sample>& output)
{
    for (int decimals = 0; decimals <= 6; ++decimals)
    {
        // Regular values
        auto max_int_id = static_cast<std::size_t>(std::pow(2, 6)); // 6 components can be varied
        for (std::size_t int_id = 0; int_id < max_int_id; ++int_id)
        {
            std::bitset<6> bitset_id (int_id);
            if (bitset_id[5] && decimals == 0) continue; // cases with micros don't make sense for fields with no decimals
            if (bitset_id.to_ulong() == 1) continue; // negative zero does not make sense
            auto t = time_from_id(int_id, decimals);
            output.push_back(create_datetime_sample(
                decimals, move(t.first), value(t.second), field_type::time));
        }

        // min and max
        auto max_value = decimals == 0 ? maket(838, 59, 59) : maket(838, 59, 58, round_micros(999999, decimals));
        output.push_back(create_datetime_sample(
            decimals, "min", value(-max_value), field_type::time));
        output.push_back(create_datetime_sample(
            decimals, "max",  value(max_value), field_type::time));
    }
}

// Year
void add_year_samples(std::vector<database_types_sample>& output)
{
    output.emplace_back("types_year", "field_default", "regular",
        std::uint64_t(2019), field_type::year, flags_zerofill);
    output.emplace_back("types_year", "field_default", "min",
        std::uint64_t(1901), field_type::year, flags_zerofill);
    output.emplace_back("types_year", "field_default", "max",
        std::uint64_t(2155), field_type::year, flags_zerofill);
    output.emplace_back("types_year", "field_default", "zero",
        std::uint64_t(0), field_type::year, flags_zerofill);
}

// String types
void add_string_samples(std::vector<database_types_sample>& output)
{
    output.emplace_back("types_string", "field_char", "regular", "test_char", field_type::char_);
    output.emplace_back("types_string", "field_char", "utf8", "\xc3\xb1", field_type::char_);
    output.emplace_back("types_string", "field_char", "empty", "", field_type::char_);

    output.emplace_back("types_string", "field_varchar", "regular", "test_varchar", field_type::varchar);
    output.emplace_back("types_string", "field_varchar", "utf8", "\xc3\x91", field_type::varchar);
    output.emplace_back("types_string", "field_varchar", "empty", "", field_type::varchar);

    output.emplace_back("types_string", "field_tinytext", "regular", "test_tinytext", field_type::text);
    output.emplace_back("types_string", "field_tinytext", "utf8", "\xc3\xa1", field_type::text);
    output.emplace_back("types_string", "field_tinytext", "empty", "", field_type::text);

    output.emplace_back("types_string", "field_text", "regular", "test_text", field_type::text);
    output.emplace_back("types_string", "field_text", "utf8", "\xc3\xa9", field_type::text);
    output.emplace_back("types_string", "field_text", "empty", "", field_type::text);

    output.emplace_back("types_string", "field_mediumtext", "regular", "test_mediumtext", field_type::text);
    output.emplace_back("types_string", "field_mediumtext", "utf8", "\xc3\xad", field_type::text);
    output.emplace_back("types_string", "field_mediumtext", "empty", "", field_type::text);

    output.emplace_back("types_string", "field_longtext", "regular", "test_longtext", field_type::text);
    output.emplace_back("types_string", "field_longtext", "utf8", "\xc3\xb3", field_type::text);
    output.emplace_back("types_string", "field_longtext", "empty", "", field_type::text);

    output.emplace_back("types_string", "field_enum", "regular", "red", field_type::enum_);

    output.emplace_back("types_string", "field_set", "regular", "red,green", field_type::set);
    output.emplace_back("types_string", "field_set", "empty", "", field_type::set);
}

void add_binary_samples(std::vector<database_types_sample>& output)
{
    // BINARY values get padded with zeros to the declared length
    output.emplace_back("types_binary", "field_binary", "regular", makesv("\0_binary\0\0"), field_type::binary);
    output.emplace_back("types_binary", "field_binary", "nonascii", makesv("\0\xff" "\0\0\0\0\0\0\0\0"), field_type::binary);
    output.emplace_back("types_binary", "field_binary", "empty", makesv("\0\0\0\0\0\0\0\0\0\0"), field_type::binary);

    output.emplace_back("types_binary", "field_varbinary", "regular", makesv("\0_varbinary"), field_type::varbinary);
    output.emplace_back("types_binary", "field_varbinary", "nonascii", makesv("\1\xfe"), field_type::varbinary);
    output.emplace_back("types_binary", "field_varbinary", "empty", "", field_type::varbinary);

    output.emplace_back("types_binary", "field_tinyblob", "regular", makesv("\0_tinyblob"), field_type::blob);
    output.emplace_back("types_binary", "field_tinyblob", "nonascii", makesv("\2\xfd"), field_type::blob);
    output.emplace_back("types_binary", "field_tinyblob", "empty", "", field_type::blob);

    output.emplace_back("types_binary", "field_blob", "regular", makesv("\0_blob"), field_type::blob);
    output.emplace_back("types_binary", "field_blob", "nonascii", makesv("\3\xfc"), field_type::blob);
    output.emplace_back("types_binary", "field_blob", "empty", "", field_type::blob);

    output.emplace_back("types_binary", "field_mediumblob", "regular", makesv("\0_mediumblob"), field_type::blob);
    output.emplace_back("types_binary", "field_mediumblob", "nonascii", makesv("\4\xfb"), field_type::blob);
    output.emplace_back("types_binary", "field_mediumblob", "empty", "", field_type::blob);

    output.emplace_back("types_binary", "field_longblob", "regular", makesv("\0_longblob"), field_type::blob);
    output.emplace_back("types_binary", "field_longblob", "nonascii", makesv("\5\xfa"), field_type::blob);
    output.emplace_back("types_binary", "field_longblob", "empty", "", field_type::blob);
}

// Tests for not implemented types
// These types do not have a more concrete representation in the library yet.
// Check we get them as strings and we get the metadata correctly
std::uint8_t geometry_value [] = {
    0x00, 0x00, 0x00,
    0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40
};

void add_not_implemented_samples(std::vector<database_types_sample>& output)
{
    output.emplace_back("types_not_implemented", "field_bit", "regular",
        "\xfe", field_type::bit, flags_unsigned);
    output.emplace_back("types_not_implemented", "field_decimal", "regular",
        "300", field_type::decimal);
    output.emplace_back("types_not_implemented", "field_geometry", "regular",
        makesv(geometry_value), field_type::geometry);
}

// Tests for certain metadata flags and NULL values
void add_flags_samples(std::vector<database_types_sample>& output)
{
    output.emplace_back("types_flags", "field_timestamp", "default",
        nullptr, field_type::timestamp, flagsvec{&field_metadata::is_set_to_now_on_update},
            0, flagsvec{&field_metadata::is_unsigned});
    output.emplace_back("types_flags", "field_primary_key", "default",
        std::int64_t(50), field_type::int_,
            flagsvec{&field_metadata::is_primary_key, &field_metadata::is_not_null,
                &field_metadata::is_auto_increment});
    output.emplace_back("types_flags", "field_not_null", "default",
        "char", field_type::char_, flagsvec{&field_metadata::is_not_null});
    output.emplace_back("types_flags", "field_unique", "default",
        std::int64_t(21), field_type::int_, flagsvec{&field_metadata::is_unique_key});
    output.emplace_back("types_flags", "field_indexed", "default",
        std::int64_t(42), field_type::int_, flagsvec{&field_metadata::is_multiple_key});
}

std::vector<database_types_sample> make_all_samples()
{
    std::vector<database_types_sample> res;
    add_int_samples(res);
    add_float_samples(res);
    add_double_samples(res);
    add_date_samples(res);
    add_datetime_samples(res);
    add_timestamp_samples(res);
    add_time_samples(res);
    add_year_samples(res);
    add_string_samples(res);
    add_binary_samples(res);
    add_not_implemented_samples(res);
    add_flags_samples(res);
    return res;
}

std::vector<database_types_sample> all_samples = make_all_samples();

BOOST_DATA_TEST_CASE_F(database_types_fixture, query, data::make(all_samples))
{
    // Compose the query
    auto query = stringize(
        "SELECT ", sample.field,
        " FROM ", sample.table,
        " WHERE id = '", sample.row_id, "'"
    );

    // Execute it
    auto result = conn.query(query);
    auto rows = result.fetch_all();

    // Validate the received metadata
    validate_meta(result.fields(), {sample.mvalid});

    // Validate the returned value
    BOOST_TEST_REQUIRE(rows.size() == 1);
    BOOST_TEST_REQUIRE(rows[0].values().size() == 1);
    BOOST_TEST(rows[0].values()[0] == sample.expected_value);
}

BOOST_DATA_TEST_CASE_F(database_types_fixture, prepared_statement, data::make(all_samples))
{
    // Prepare the statement
    auto stmt_sql = stringize(
        "SELECT ", sample.field,
        " FROM ", sample.table,
        " WHERE id = ?"
    );
    auto stmt = conn.prepare_statement(stmt_sql);

    // Execute it with the provided parameters
    auto result = stmt.execute(make_values(sample.row_id));
    auto rows = result.fetch_all();

    // Validate the received metadata
    validate_meta(result.fields(), {sample.mvalid});

    // Validate the returned value
    BOOST_TEST_REQUIRE(rows.size() == 1);
    BOOST_TEST_REQUIRE(rows[0].values().size() == 1);
    BOOST_TEST(rows[0].values()[0] == sample.expected_value);
}

// The prepared statement param tests binary serialization.
// This test is not applicable (yet) to nullptr values or bit values.
// Doing "field = ?" where ? is nullptr never matches anything.
// Bit values are returned as strings bit need to be sent as integers in
// prepared statements. Filter the cases to remove the ones that
// are not applicable
std::vector<database_types_sample>
make_prepared_stmt_param_samples()
{
    std::vector<database_types_sample> res;
    res.reserve(all_samples.size());
    for (const auto& test : all_samples)
    {
        if (!test.expected_value.is_null() &&
             test.mvalid.type() != field_type::bit)
        {
            res.push_back(test);
        }
    }
    return res;
}

BOOST_DATA_TEST_CASE_F(database_types_fixture,
    prepared_statement_execute_param,
    data::make(make_prepared_stmt_param_samples()))
{
    // Prepare the statement
    auto stmt_sql = stringize(
        "SELECT ", sample.field,
        " FROM ", sample.table,
        " WHERE id = ? AND ", sample.field, " = ?"
    );
    auto stmt = conn.prepare_statement(stmt_sql);

    // Execute it with the provided parameters
    auto result = stmt.execute(make_values(sample.row_id, sample.expected_value));
    auto rows = result.fetch_all();

    // Validate the returned value
    BOOST_TEST_REQUIRE(rows.size() == 1);
    BOOST_TEST_REQUIRE(rows[0].values().size() == 1);
    BOOST_TEST(rows[0].values()[0] == sample.expected_value);
}

// Validate that the metadata we retrieve with certain queries is correct
BOOST_FIXTURE_TEST_CASE(aliased_table_metadata, database_types_fixture)
{
    auto result = conn.query(
        "SELECT field_varchar AS field_alias FROM empty_table table_alias");
    std::vector<meta_validator> validators {
        { "table_alias", "empty_table", "field_alias",
            "field_varchar", field_type::varchar }
    };
    validate_meta(result.fields(), validators);
}


BOOST_AUTO_TEST_SUITE_END() // test_database_types

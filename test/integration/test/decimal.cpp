//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/detail/config.hpp>

#ifdef BOOST_MYSQL_CXX14

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/decimal.hpp>
#include <boost/mysql/static_results.hpp>

#include <boost/decimal/iostream.hpp>
#include <boost/decimal/literals.hpp>
#include <boost/describe/operators.hpp>
#include <boost/optional/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/test/unit_test.hpp>

#include <vector>

#include "test_common/network_result.hpp"
#include "test_integration/any_connection_fixture.hpp"
#include "test_integration/metadata_validator.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using boost::optional;
using boost::test_tools::per_element;
using boost::describe::operators::operator==;
using boost::describe::operators::operator<<;
namespace decimal = boost::decimal;
using namespace decimal::literals;

// For now, we don't support decimals as statement parameters
// (only when reading rows)
struct decimal_row
{
    std::string id;
    optional<decimal::decimal32_t> field_4;
    optional<decimal::decimal_fast32_t> field_7;
    optional<decimal::decimal64_t> field_10;
    optional<decimal::decimal_fast64_t> field_16;
    optional<decimal::decimal128_t> field_20;
    optional<decimal::decimal_fast128_t> field_34;
};
BOOST_DESCRIBE_STRUCT(decimal_row, (), (id, field_4, field_7, field_10, field_16, field_20, field_34))

namespace {

BOOST_AUTO_TEST_SUITE(test_decimal)

BOOST_FIXTURE_TEST_CASE(select, any_connection_fixture)
{
    // Setup
    connect();

    // Issue the query
    static_results<decimal_row> result;
    conn.async_execute("SELECT * FROM types_decimal ORDER BY id", result, as_netresult).validate_no_error();

    // Validate metadata
    std::vector<meta_validator::flag_getter> id_flags{
        &metadata::is_primary_key,
        &metadata::is_not_null,
        &metadata::has_no_default_value,
    };
    std::vector<meta_validator> expected_meta{
        {"types_decimal", "id",       column_type::varchar, std::move(id_flags),      0u,  {}, {} },
        {"types_decimal", "field_4",  column_type::decimal, {},                       0u,  {}, 5u },
        {"types_decimal", "field_7",  column_type::decimal, {},                       7u,  {}, 9u },
        {"types_decimal", "field_10", column_type::decimal, {},                       0u,  {}, 11u},
        {"types_decimal", "field_16", column_type::decimal, {},                       4u,  {}, 18u},
        {"types_decimal", "field_20", column_type::decimal, {&metadata::is_unsigned}, 2u,  {}, 21u},
        {"types_decimal", "field_34", column_type::decimal, {},                       30u, {}, 36u},
    };
    validate_meta(result.meta(), expected_meta);

    // Validate rows
    // clang-format off
    const decimal_row expected_rows [] = {
        {"max",       9999_df,  0.9999999_dff,  9999999999_dd,  999999999999.9999_ddf, 999999999999999999.99_dl, -9999.999999999999999999999999999999_dlf},
        {"min",      -9999_df, -0.9999999_dff, -9999999999_dd, -999999999999.9999_ddf, 0_dl,                     -9999.999999999999999999999999999999_dlf},
        {"negative", -213_df,  -0.1214295_dff, -9000_dd,       -20.1234_ddf,           {},                       -1234.567890123456789012345678901234_dlf},
        {"regular",   213_df,   0.1214295_dff,  9000_dd,        20.1234_ddf,           121.20_dl,                 1234.567890123456789012345678901234_dlf},
    };
    // clang-format on
    BOOST_TEST(result.rows() == expected_rows, per_element());
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

#endif

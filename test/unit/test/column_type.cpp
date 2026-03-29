//
// Copyright (c) 2026 Vladislav Soulgard (vsoulgard at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/column_type.hpp>

#include <boost/test/unit_test.hpp>

#include <string>
#include <sstream>
#include <iostream>

using namespace boost::mysql;

BOOST_AUTO_TEST_SUITE(test_column_type)

std::string to_string(column_type t)
{
    std::ostringstream oss;
    auto& result = oss << t;
    BOOST_TEST(!oss.fail());
    // Check if operator<< return same stream object
    BOOST_TEST(&result == &oss);
    return oss.str();
}

BOOST_AUTO_TEST_CASE(stream_operator_basic)
{
    // Numeric types
    BOOST_TEST(to_string(column_type::tinyint) == "tinyint");
    BOOST_TEST(to_string(column_type::smallint) == "smallint");
    BOOST_TEST(to_string(column_type::mediumint) == "mediumint");
    BOOST_TEST(to_string(column_type::int_) == "int_");
    BOOST_TEST(to_string(column_type::bigint) == "bigint");
    
    // Floating-point types
    BOOST_TEST(to_string(column_type::float_) == "float_");
    BOOST_TEST(to_string(column_type::double_) == "double_");
    BOOST_TEST(to_string(column_type::decimal) == "decimal");
    
    // Special types
    BOOST_TEST(to_string(column_type::bit) == "bit");
    BOOST_TEST(to_string(column_type::year) == "year");
    
    // Date/time types
    BOOST_TEST(to_string(column_type::time) == "time");
    BOOST_TEST(to_string(column_type::date) == "date");
    BOOST_TEST(to_string(column_type::datetime) == "datetime");
    BOOST_TEST(to_string(column_type::timestamp) == "timestamp");
    
    // String types
    BOOST_TEST(to_string(column_type::char_) == "char_");
    BOOST_TEST(to_string(column_type::varchar) == "varchar");
    BOOST_TEST(to_string(column_type::binary) == "binary");
    BOOST_TEST(to_string(column_type::varbinary) == "varbinary");
    
    // Large object types
    BOOST_TEST(to_string(column_type::text) == "text");
    BOOST_TEST(to_string(column_type::blob) == "blob");
    
    // Special MySQL types
    BOOST_TEST(to_string(column_type::enum_) == "enum_");
    BOOST_TEST(to_string(column_type::set) == "set");
    BOOST_TEST(to_string(column_type::json) == "json");
    BOOST_TEST(to_string(column_type::geometry) == "geometry");
}

BOOST_AUTO_TEST_CASE(stream_operator_unknown)
{
    // Unknown type
    BOOST_TEST(to_string(column_type::unknown) == "<unknown column type>");

    // Incorrect value
    BOOST_TEST(to_string(static_cast<column_type>(999)) == "<unknown column type>");
}

BOOST_AUTO_TEST_SUITE_END()

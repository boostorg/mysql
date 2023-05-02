//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/typing/row_traits.hpp>

#include <boost/describe/class.hpp>
#include <boost/test/unit_test.hpp>

#include <cstddef>

#include "creation/create_meta.hpp"
#include "printing.hpp"
#include "test_common.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using boost::mysql::detail::get_row_field_names;
using boost::mysql::detail::get_row_size;
using boost::mysql::detail::is_static_row;
using boost::mysql::detail::meta_check;
using boost::mysql::detail::parse;

namespace {

BOOST_AUTO_TEST_SUITE(test_row_traits)

// type definitions: describe structs
struct sempty
{
};
BOOST_DESCRIBE_STRUCT(sempty, (), ())

struct s1
{
    std::int32_t i;
};
BOOST_DESCRIBE_STRUCT(s1, (), (i))

struct s2
{
    std::int32_t i;
    float f;
};
BOOST_DESCRIBE_STRUCT(s2, (), (i, f))

struct sinherit : s2
{
    double double_field;
};
BOOST_DESCRIBE_STRUCT(sinherit, (s2), (double_field))

// a struct without any relationship with this lib and no describe data
struct unrelated
{
};

struct sbad
{
    int i;
    unrelated f;
    double d;
};
BOOST_DESCRIBE_STRUCT(sbad, (), (i, f, d))

// type definitions: tuples
using tempty = std::tuple<>;
using t1 = std::tuple<double>;
using t2 = std::tuple<std::int32_t, float>;
using t3 = std::tuple<std::string, std::int32_t, double>;
using tbad = std::tuple<int, unrelated, double>;

// is_row_type concept: doesn't inspect individual fields
static_assert(is_static_row<sempty>::value, "");
static_assert(is_static_row<s1>::value, "");
static_assert(is_static_row<s2>::value, "");
static_assert(is_static_row<sinherit>::value, "");
static_assert(is_static_row<sbad>::value, "");

static_assert(is_static_row<tempty>::value, "");
static_assert(is_static_row<t1>::value, "");
static_assert(is_static_row<t2>::value, "");
static_assert(is_static_row<t3>::value, "");
static_assert(is_static_row<tbad>::value, "");

static_assert(!is_static_row<unrelated>::value, "");
static_assert(!is_static_row<int>::value, "");
static_assert(!is_static_row<row>::value, "");

BOOST_AUTO_TEST_SUITE(describe_structs)

// size
static_assert(get_row_size<sempty>() == 0u, "");
static_assert(get_row_size<s1>() == 1u, "");
static_assert(get_row_size<s2>() == 2u, "");
static_assert(get_row_size<sinherit>() == 3u, "");

// names
BOOST_AUTO_TEST_CASE(get_row_field_names_)
{
    BOOST_TEST(get_row_field_names<sempty>() == nullptr);
    BOOST_TEST(get_row_field_names<s1>()[0] == "i");
    BOOST_TEST(get_row_field_names<s2>()[0] == "i");
    BOOST_TEST(get_row_field_names<s2>()[1] == "f");
    BOOST_TEST(get_row_field_names<sinherit>()[0] == "i");
    BOOST_TEST(get_row_field_names<sinherit>()[1] == "f");
    BOOST_TEST(get_row_field_names<sinherit>()[2] == "double_field");
}

// meta check
BOOST_AUTO_TEST_CASE(meta_check_ok)
{
    const metadata meta[] = {
        meta_builder().type(column_type::float_).nullable(false).build(),
        meta_builder().type(column_type::double_).nullable(false).build(),
        meta_builder().type(column_type::smallint).nullable(false).build(),
    };
    const std::size_t pos_map[] = {2, 0, 1};
    diagnostics diag;
    auto err = meta_check<sinherit>(meta, pos_map, diag);
    BOOST_TEST(err == error_code());
    BOOST_TEST(diag.client_message() == "");
}

BOOST_AUTO_TEST_CASE(meta_check_fail)
{
    const metadata meta[] = {
        meta_builder().type(column_type::tinyint).nullable(false).build(),
        meta_builder().type(column_type::double_).nullable(false).build(),
        meta_builder().type(column_type::double_).nullable(false).build(),
    };
    const std::size_t pos_map[] = {0, 1, 2};
    diagnostics diag;
    auto err = meta_check<sinherit>(meta, pos_map, diag);
    BOOST_TEST(err == client_errc::metadata_check_failed);
    BOOST_TEST(
        diag.client_message() ==
        "Incompatible types for field 'f': C++ type 'float' is not compatible with DB type 'DOUBLE'"
    );
}

BOOST_AUTO_TEST_CASE(meta_check_empty_struct)
{
    diagnostics diag;
    auto err = meta_check<sempty>(metadata_collection_view(), nullptr, diag);
    BOOST_TEST(err == error_code());
    BOOST_TEST(diag.client_message() == "");
}

// parsing
BOOST_AUTO_TEST_CASE(parse_success)
{
    // int, float, double
    auto fv = make_fv_arr(8.1, "abc", 42, 4.3f);
    const std::size_t pos_map[] = {2, 3, 0};
    sinherit value;
    auto err = parse(fv.data(), pos_map, value);
    BOOST_TEST(err == error_code());
    BOOST_TEST(value.i == 42);
    BOOST_TEST(value.f == 4.3f);
    BOOST_TEST(value.double_field == 8.1);
}

BOOST_AUTO_TEST_CASE(parse_one_error)
{
    // int, float, double
    auto fv = make_fv_arr(8.1, "abc", nullptr, 4.3f);
    const std::size_t pos_map[] = {2, 3, 0};
    sinherit value;
    auto err = parse(fv.data(), pos_map, value);
    BOOST_TEST(err == client_errc::is_null);
}

BOOST_AUTO_TEST_CASE(parse_several_errors)
{
    // int, float, double
    // we return the first error only
    auto fv = make_fv_arr(8.1, "abc", 0xffffffffffffffff, nullptr);
    const std::size_t pos_map[] = {2, 3, 0};
    sinherit value;
    auto err = parse(fv.data(), pos_map, value);
    BOOST_TEST(err == client_errc::protocol_value_error);
}

BOOST_AUTO_TEST_CASE(parse_empty_struct)
{
    sempty value;
    auto err = parse(nullptr, nullptr, value);
    BOOST_TEST(err == error_code());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(tuples)

// size
static_assert(get_row_size<tempty>() == 0, "");
static_assert(get_row_size<t1>() == 1, "");
static_assert(get_row_size<t2>() == 2, "");
static_assert(get_row_size<t3>() == 3, "");

// name tables
static_assert(get_row_field_names<tempty>() == nullptr, "");
static_assert(get_row_field_names<t1>() == nullptr, "");
static_assert(get_row_field_names<t2>() == nullptr, "");
static_assert(get_row_field_names<t3>() == nullptr, "");

// meta check
BOOST_AUTO_TEST_CASE(meta_check_ok)
{
    const metadata meta[] = {
        meta_builder().type(column_type::varchar).nullable(false).build(),
        meta_builder().type(column_type::int_).nullable(false).build(),
        meta_builder().type(column_type::double_).nullable(false).build(),
    };
    const std::size_t pos_map[] = {0, 1, 2};
    diagnostics diag;
    auto err = meta_check<t3>(meta, pos_map, diag);
    BOOST_TEST(err == error_code());
    BOOST_TEST(diag.client_message() == "");
}

BOOST_AUTO_TEST_CASE(meta_check_fail)
{
    const metadata meta[] = {
        meta_builder().type(column_type::varchar).nullable(false).build(),
        meta_builder().type(column_type::bigint).nullable(false).build(),
        meta_builder().type(column_type::double_).nullable(false).build(),
    };
    const std::size_t pos_map[] = {0, 1, 2};
    diagnostics diag;
    auto err = meta_check<t3>(meta, pos_map, diag);
    BOOST_TEST(err == client_errc::metadata_check_failed);
    BOOST_TEST(
        diag.client_message() ==
        "Incompatible types for field in position 1: C++ type 'int32_t' is not compatible with DB type "
        "'BIGINT'"
    );
}

BOOST_AUTO_TEST_CASE(meta_check_empty)
{
    diagnostics diag;
    auto err = meta_check<tempty>(metadata_collection_view(), nullptr, diag);
    BOOST_TEST(err == error_code());
    BOOST_TEST(diag.client_message() == "");
}

// parsing
BOOST_AUTO_TEST_CASE(parse_success)
{
    // string, int, double
    auto fv = make_fv_arr("abc", 42, 9.1, "jkl");
    const std::size_t pos_map[] = {0, 1, 2};
    t3 value;
    auto err = parse(fv.data(), pos_map, value);
    BOOST_TEST(err == error_code());
    BOOST_TEST(std::get<0>(value) == "abc");
    BOOST_TEST(std::get<1>(value) == 42);
    BOOST_TEST(std::get<2>(value) == 9.1);
}

BOOST_AUTO_TEST_CASE(parse_one_error)
{
    // string, int, double
    auto fv = make_fv_arr("abc", nullptr, 4.3, "jkl");
    const std::size_t pos_map[] = {0, 1, 2};
    t3 value;
    auto err = parse(fv.data(), pos_map, value);
    BOOST_TEST(err == client_errc::is_null);
}

BOOST_AUTO_TEST_CASE(parse_several_errors)
{
    // string, int, double
    // we return the first error only
    auto fv = make_fv_arr(nullptr, 0xffffffffffffffff, 4.2);
    const std::size_t pos_map[] = {0, 1, 2};
    t3 value;
    auto err = parse(fv.data(), pos_map, value);
    BOOST_TEST(err == client_errc::is_null);
}

BOOST_AUTO_TEST_CASE(parse_empty_tuple)
{
    tempty value;
    auto err = parse(nullptr, nullptr, value);
    BOOST_TEST(err == error_code());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

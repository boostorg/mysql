//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/detail/config.hpp>

#ifdef BOOST_MYSQL_CXX14

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/typing/pos_map.hpp>
#include <boost/mysql/detail/typing/row_traits.hpp>

#include <boost/core/span.hpp>
#include <boost/describe/class.hpp>
#include <boost/test/unit_test.hpp>

#include <cstddef>
#include <tuple>

#include "test_common/create_basic.hpp"
#include "test_common/printing.hpp"
#include "test_unit/create_meta.hpp"
#include "test_unit/row_identity.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using boost::span;
using detail::get_row_name_table;
using detail::get_row_size;
using detail::is_static_row;
using detail::meta_check_impl;
using detail::name_table_t;
using detail::parse;
using detail::pos_absent;

namespace boost {
namespace mysql {
namespace test {

template <class Inner>
struct test_marker;

}  // namespace test

namespace detail {

template <class Inner>
class row_traits<test_marker<Inner>, false>
{
public:
    using underlying_row_type = typename Inner::underlying_row_type;
    using field_types = typename Inner::field_types;
    name_table_t name_table() const noexcept { return {}; }

    template <class F>
    static void for_each_member(underlying_row_type& to, F&& f)
    {
        Inner::for_each_member(to, std::forward<F>(f));
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

BOOST_AUTO_TEST_SUITE(test_row_traits)

void compare_name_tables(name_table_t lhs, name_table_t rhs)
{
    std::vector<string_view> lhsvec(lhs.begin(), lhs.end());
    std::vector<string_view> rhsvec(rhs.begin(), rhs.end());
    BOOST_TEST(lhsvec == rhsvec);
}

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
static_assert(is_static_row<sempty>, "");
static_assert(is_static_row<s1>, "");
static_assert(is_static_row<s2>, "");
static_assert(is_static_row<sinherit>, "");
static_assert(is_static_row<sbad>, "");

static_assert(is_static_row<tempty>, "");
static_assert(is_static_row<t1>, "");
static_assert(is_static_row<t2>, "");
static_assert(is_static_row<t3>, "");
static_assert(is_static_row<tbad>, "");

static_assert(is_static_row<row_identity<tempty>>, "");
static_assert(is_static_row<row_identity<t1>>, "");
static_assert(is_static_row<row_identity<tbad>>, "");

static_assert(!is_static_row<unrelated>, "");
static_assert(!is_static_row<int>, "");
static_assert(!is_static_row<row>, "");
static_assert(!is_static_row<s1&>, "");
static_assert(!is_static_row<const s1&>, "");
static_assert(!is_static_row<s1&&>, "");
static_assert(!is_static_row<const s1&&>, "");
static_assert(!is_static_row<s1*>, "");

// We perform metadata checks correctly given a generic, compliant row traits class
BOOST_AUTO_TEST_SUITE(meta_check_)

const metadata meta[] = {
    meta_builder().type(column_type::tinyint).unsigned_flag(false).nullable(false).build(),
    meta_builder().type(column_type::varchar).nullable(false).build(),
    meta_builder().type(column_type::float_).nullable(false).build(),
};

BOOST_AUTO_TEST_CASE(positional_success)
{
    // meta is: TINYINT, VARCHAR, FLOAT
    using types = std::tuple<int, std::string, float>;
    const std::size_t pos_map[] = {0, 1, 2};
    diagnostics diag;

    auto err = detail::meta_check_impl<types>(name_table_t(), pos_map, meta, diag);

    BOOST_TEST(err == error_code());
    BOOST_TEST(diag.client_message() == "");
}

BOOST_AUTO_TEST_CASE(positional_success_trailing_fields)
{
    // meta is: TINYINT, VARCHAR, FLOAT
    using types = std::tuple<int, std::string>;
    const std::size_t pos_map[] = {0, 1};
    diagnostics diag;

    auto err = meta_check_impl<types>(name_table_t(), pos_map, meta, diag);

    BOOST_TEST(err == error_code());
    BOOST_TEST(diag.client_message() == "");
}

BOOST_AUTO_TEST_CASE(positional_missing_fields)
{
    // meta is: TINYINT, VARCHAR, FLOAT
    using types = std::tuple<int, std::string, float, int, int>;
    const std::size_t pos_map[] = {0, 1, 2, pos_absent, pos_absent};
    const char* expected_msg =
        "Field in position 3 can't be mapped: there are more fields in your C++ data type than in your query"
        "\n"
        "Field in position 4 can't be mapped: there are more fields in your C++ data type than in your query";
    diagnostics diag;

    auto err = meta_check_impl<types>(name_table_t(), pos_map, meta, diag);

    BOOST_TEST(err == client_errc::metadata_check_failed);
    BOOST_TEST(diag.client_message() == expected_msg);
}

BOOST_AUTO_TEST_CASE(positional_no_fields)
{
    using types = std::tuple<int, std::string>;
    const std::size_t pos_map[] = {pos_absent, pos_absent};
    const char* expected_msg =
        "Field in position 0 can't be mapped: there are more fields in your C++ data type than in your query"
        "\n"
        "Field in position 1 can't be mapped: there are more fields in your C++ data type than in your query";
    diagnostics diag;

    auto err = meta_check_impl<types>(name_table_t(), pos_map, metadata_collection_view(), diag);

    BOOST_TEST(err == client_errc::metadata_check_failed);
    BOOST_TEST(diag.client_message() == expected_msg);
}

BOOST_AUTO_TEST_CASE(named_success)
{
    // meta is: TINYINT, VARCHAR, FLOAT
    using types = std::tuple<float, int, std::string>;
    const std::size_t pos_map[] = {2, 0, 1};
    const string_view names[] = {"f1", "f2", "f3"};
    diagnostics diag;

    auto err = meta_check_impl<types>(names, pos_map, meta, diag);

    BOOST_TEST(err == error_code());
    BOOST_TEST(diag.client_message() == "");
}

BOOST_AUTO_TEST_CASE(named_success_extra_fields)
{
    // meta is: TINYINT, VARCHAR, FLOAT
    using types = std::tuple<std::string, int>;
    const std::size_t pos_map[] = {1, 0};
    const string_view names[] = {"f1", "f2"};
    diagnostics diag;

    auto err = meta_check_impl<types>(names, pos_map, meta, diag);

    BOOST_TEST(err == error_code());
    BOOST_TEST(diag.client_message() == "");
}

BOOST_AUTO_TEST_CASE(named_absent_fields)
{
    // meta is: TINYINT, VARCHAR, FLOAT
    using types = std::tuple<std::string, int, float>;
    const std::size_t pos_map[] = {pos_absent, 0, pos_absent};
    const string_view names[] = {"f1", "f2", "f3"};
    const char* expected_msg =
        "Field 'f1' is not present in the data returned by the server"
        "\n"
        "Field 'f3' is not present in the data returned by the server";
    diagnostics diag;

    auto err = meta_check_impl<types>(names, pos_map, meta, diag);

    BOOST_TEST(err == client_errc::metadata_check_failed);
    BOOST_TEST(diag.client_message() == expected_msg);
}

BOOST_AUTO_TEST_CASE(named_no_fields)
{
    using types = std::tuple<int, int>;
    const std::size_t pos_map[] = {pos_absent, pos_absent};
    const string_view names[] = {"f1", "f2"};
    const char* expected_msg =
        "Field 'f1' is not present in the data returned by the server"
        "\n"
        "Field 'f2' is not present in the data returned by the server";
    diagnostics diag;

    auto err = meta_check_impl<types>(names, pos_map, metadata_collection_view(), diag);

    BOOST_TEST(err == client_errc::metadata_check_failed);
    BOOST_TEST(diag.client_message() == expected_msg);
}

BOOST_AUTO_TEST_CASE(failed_checks)
{
    // meta is: TINYINT, VARCHAR, FLOAT
    using types = std::tuple<float, float, float>;
    const std::size_t pos_map[] = {2, 1, 0};
    const string_view names[] = {"f1", "f2", "f3"};
    const char* expected_msg =
        "Incompatible types for field 'f2': C++ type 'float' is not compatible with DB type 'VARCHAR'"
        "\n"
        "Incompatible types for field 'f3': C++ type 'float' is not compatible with DB type 'TINYINT'";
    diagnostics diag;

    auto err = meta_check_impl<types>(names, pos_map, meta, diag);

    BOOST_TEST(err == client_errc::metadata_check_failed);
    BOOST_TEST(diag.client_message() == expected_msg);
}

BOOST_AUTO_TEST_CASE(all_fields_discarded)
{
    using types = std::tuple<>;
    diagnostics diag;

    auto err = meta_check_impl<types>(name_table_t(), span<const std::size_t>(), meta, diag);

    BOOST_TEST(err == error_code());
    BOOST_TEST(diag.client_message() == "");
}

BOOST_AUTO_TEST_CASE(empty)
{
    using types = std::tuple<>;
    diagnostics diag;

    auto err = meta_check_impl<types>(
        name_table_t(),
        boost::span<const std::size_t>(),
        metadata_collection_view(),
        diag
    );

    BOOST_TEST(err == error_code());
    BOOST_TEST(diag.client_message() == "");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(parse_)

struct test_row
{
    struct underlying_row_type
    {
        std::int32_t i{};
        float f{};
        double double_field{};
    };

    using field_types = std::tuple<int, float, double>;

    template <class F>
    static void for_each_member(underlying_row_type& u, F&& f)
    {
        f(u.i);
        f(u.f);
        f(u.double_field);
    }
};

struct test_empty_row
{
    struct underlying_row_type
    {
    };

    using field_types = std::tuple<>;

    template <class F>
    static void for_each_member(underlying_row_type&, F&&)
    {
    }
};

BOOST_AUTO_TEST_CASE(parse_success)
{
    // int, float, double
    const auto fv = make_fv_arr(8.1, "abc", 42, 4.3f);
    const std::size_t pos_map[] = {2, 3, 0};
    test_row::underlying_row_type value;
    auto err = parse<test_marker<test_row>>(pos_map, fv, value);
    BOOST_TEST(err == error_code());
    BOOST_TEST(value.i == 42);
    BOOST_TEST(value.f == 4.3f);
    BOOST_TEST(value.double_field == 8.1);
}

BOOST_AUTO_TEST_CASE(parse_one_error)
{
    // int, float, double
    const auto fv = make_fv_arr(8.1, "abc", nullptr, 4.3f);
    const std::size_t pos_map[] = {2, 3, 0};
    test_row::underlying_row_type value;
    auto err = parse<test_marker<test_row>>(pos_map, fv, value);
    BOOST_TEST(err == client_errc::static_row_parsing_error);
}

BOOST_AUTO_TEST_CASE(parse_several_errors)
{
    // int, float, double
    // we return the first error only
    const auto fv = make_fv_arr(8.1, "abc", 0xffffffffffffffff, nullptr);
    const std::size_t pos_map[] = {2, 3, 0};
    test_row::underlying_row_type value;
    auto err = parse<test_marker<test_row>>(pos_map, fv, value);
    BOOST_TEST(err == client_errc::static_row_parsing_error);
}

BOOST_AUTO_TEST_CASE(parse_error_success_interleaved)
{
    // int, float, double
    const auto fv = make_fv_arr(8.1, "abc", 42, nullptr);
    const std::size_t pos_map[] = {2, 3, 0};
    test_row::underlying_row_type value;
    auto err = parse<test_marker<test_row>>(pos_map, fv, value);
    BOOST_TEST(err == client_errc::static_row_parsing_error);
}

BOOST_AUTO_TEST_CASE(parse_all_fields_discarded)
{
    // int, float, double
    const auto fv = make_fv_arr(8.1, "abc", 42, nullptr);
    test_empty_row::underlying_row_type value;
    auto err = parse<test_marker<test_empty_row>>(span<const std::size_t>(), fv, value);
    BOOST_TEST(err == error_code());
}

BOOST_AUTO_TEST_CASE(parse_no_fields)
{
    test_empty_row::underlying_row_type value;
    auto err = parse<test_marker<test_empty_row>>(span<const std::size_t>(), span<const field_view>(), value);
    BOOST_TEST(err == error_code());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(describe_structs)

// size
static_assert(get_row_size<sempty>() == 0u, "");
static_assert(get_row_size<s1>() == 1u, "");
static_assert(get_row_size<s2>() == 2u, "");
static_assert(get_row_size<sinherit>() == 3u, "");

BOOST_AUTO_TEST_CASE(get_row_name_table_)
{
    const string_view expected_s1[] = {"i"};
    const string_view expected_s2[] = {"i", "f"};
    const string_view expected_sinherit[] = {"i", "f", "double_field"};

    compare_name_tables(get_row_name_table<sempty>(), name_table_t());
    compare_name_tables(get_row_name_table<s1>(), expected_s1);
    compare_name_tables(get_row_name_table<s2>(), expected_s2);
    compare_name_tables(get_row_name_table<sinherit>(), expected_sinherit);
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
    auto err = detail::meta_check<sinherit>(pos_map, meta, diag);
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
    auto err = detail::meta_check<sinherit>(pos_map, meta, diag);
    BOOST_TEST(err == client_errc::metadata_check_failed);
    BOOST_TEST(
        diag.client_message() ==
        "Incompatible types for field 'f': C++ type 'float' is not compatible with DB type 'DOUBLE'"
    );
}

BOOST_AUTO_TEST_CASE(meta_check_empty_struct)
{
    diagnostics diag;
    auto err = detail::meta_check<sempty>(span<const std::size_t>(), metadata_collection_view(), diag);
    BOOST_TEST(err == error_code());
    BOOST_TEST(diag.client_message() == "");
}

// parsing
BOOST_AUTO_TEST_CASE(parse_success)
{
    // int, float, double
    const auto fv = make_fv_arr(8.1, "abc", 42, 4.3f);
    const std::size_t pos_map[] = {2, 3, 0};
    sinherit value;
    auto err = parse<sinherit>(pos_map, fv, value);
    BOOST_TEST(err == error_code());
    BOOST_TEST(value.i == 42);
    BOOST_TEST(value.f == 4.3f);
    BOOST_TEST(value.double_field == 8.1);
}

BOOST_AUTO_TEST_CASE(parse_error)
{
    // int, float, double
    const auto fv = make_fv_arr(8.1, "abc", nullptr, 4.3f);
    const std::size_t pos_map[] = {2, 3, 0};
    sinherit value;
    auto err = parse<sinherit>(pos_map, fv, value);
    BOOST_TEST(err == client_errc::static_row_parsing_error);
}

BOOST_AUTO_TEST_CASE(parse_empty_struct)
{
    sempty value;
    auto err = parse<sempty>(span<const std::size_t>(), span<const field_view>(), value);
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
BOOST_AUTO_TEST_CASE(get_row_name_table_)
{
    compare_name_tables(get_row_name_table<tempty>(), name_table_t());
    compare_name_tables(get_row_name_table<t1>(), name_table_t());
    compare_name_tables(get_row_name_table<t2>(), name_table_t());
    compare_name_tables(get_row_name_table<t3>(), name_table_t());
}

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
    auto err = detail::meta_check<t3>(pos_map, meta, diag);
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
    auto err = detail::meta_check<t3>(pos_map, meta, diag);
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
    auto err = detail::meta_check<tempty>(span<const std::size_t>(), metadata_collection_view(), diag);
    BOOST_TEST(err == error_code());
    BOOST_TEST(diag.client_message() == "");
}

// parsing
BOOST_AUTO_TEST_CASE(parse_success)
{
    // string, int, double
    const auto fv = make_fv_arr("abc", 42, 9.1, "jkl");
    const std::size_t pos_map[] = {0, 1, 2};
    t3 value;
    auto err = parse<t3>(pos_map, fv, value);
    BOOST_TEST(err == error_code());
    BOOST_TEST(std::get<0>(value) == "abc");
    BOOST_TEST(std::get<1>(value) == 42);
    BOOST_TEST(std::get<2>(value) == 9.1);
}

BOOST_AUTO_TEST_CASE(parse_error)
{
    // string, int, double
    const auto fv = make_fv_arr("abc", nullptr, 4.3, "jkl");
    const std::size_t pos_map[] = {0, 1, 2};
    t3 value;
    auto err = parse<t3>(pos_map, fv, value);
    BOOST_TEST(err == client_errc::static_row_parsing_error);
}

BOOST_AUTO_TEST_CASE(parse_empty_tuple)
{
    tempty value;
    auto err = parse<tempty>(span<const std::size_t>(), span<const field_view>(), value);
    BOOST_TEST(err == error_code());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

#endif

//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/field_view.hpp>
#include <boost/mysql/rows.hpp>
#include <boost/mysql/rows_view.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>

#include <stdexcept>

#include "test_common.hpp"

using boost::mysql::make_field_views;
using boost::mysql::rows;
using boost::mysql::rows_view;
using boost::mysql::test::makerow;
using boost::mysql::test::makerows;

namespace {

BOOST_AUTO_TEST_SUITE(test_rows)

BOOST_AUTO_TEST_CASE(default_ctor)
{
    rows r;
    BOOST_TEST(r.empty());
}

BOOST_AUTO_TEST_SUITE(ctor_from_rows_view)
BOOST_AUTO_TEST_CASE(empty)
{
    rows_view v;
    rows r(v);
    BOOST_TEST(r.empty());
}

BOOST_AUTO_TEST_CASE(non_strings)
{
    auto fields = make_field_views(20u, 1.0f, nullptr, -1);
    rows_view v(fields.data(), fields.size(), 2);
    rows r(v);
    fields = make_field_views(0, 0, 0, 0);  // r should be independent of the original fields

    BOOST_TEST(r.size() == 2u);
    BOOST_TEST(r[0] == makerow(20u, 1.0f));
    BOOST_TEST(r[1] == makerow(nullptr, -1));
}

BOOST_AUTO_TEST_CASE(strings)
{
    std::string s1("abc"), s2("");
    auto fields = make_field_views(s1, 1.0f, s2, -1);
    rows_view v(fields.data(), fields.size(), 2);
    rows r(v);

    // r should be independent of the original fields/strings
    fields = make_field_views(0, 0, 0, 0);
    s1 = "other";
    s2 = "yet_another";

    BOOST_TEST(r.size() == 2u);
    BOOST_TEST(r[0] == makerow("abc", 1.0f));
    BOOST_TEST(r[1] == makerow("", -1));
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(copy_ctor)
BOOST_AUTO_TEST_CASE(empty)
{
    rows r1;
    rows r2(r1);
    BOOST_TEST(r2.empty());
}

BOOST_AUTO_TEST_CASE(non_strings)
{
    rows r1 = makerows(3, 1, 21.0f, nullptr, 2, 22.0f, -1);
    rows r2(r1);
    r1 = makerows(2, 0, 0, 0, 0);  // r2 should be independent of r1

    BOOST_TEST(r2.size() == 2u);
    BOOST_TEST(r2[0] == makerow(1, 21.0f, nullptr));
    BOOST_TEST(r2[1] == makerow(2, 22.0f, -1));
}

BOOST_AUTO_TEST_CASE(strings)
{
    rows r1 = makerows(3, "abc", 21.0f, "", "cdefg", 22.0f, "aaa");
    rows r2(r1);
    r1 = makerows(2, 0, 0, 0, 0);  // r2 should be independent of r1

    BOOST_TEST(r2.size() == 2u);
    BOOST_TEST(r2[0] == makerow("abc", 21.0f, ""));
    BOOST_TEST(r2[1] == makerow("cdefg", 22.0f, "aaa"));
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(move_ctor)
BOOST_AUTO_TEST_CASE(empty)
{
    rows r1;
    rows_view rv(r1);
    rows r2(std::move(r1));
    BOOST_TEST(r2.empty());
    BOOST_TEST(rv == r2);
}

BOOST_AUTO_TEST_CASE(non_strings)
{
    rows r1 = makerows(3, 1, 21.0f, nullptr, 2, 22.0f, -1);
    rows_view rv(r1);
    rows r2(std::move(r1));
    r1 = makerows(2, 0, 0, 0, 0);  // r2 should be independent of r1

    BOOST_TEST(r2.size() == 2u);
    BOOST_TEST(r2[0] == makerow(1, 21.0f, nullptr));
    BOOST_TEST(r2[1] == makerow(2, 22.0f, -1));
    BOOST_TEST(rv == r2);
}

BOOST_AUTO_TEST_CASE(strings)
{
    rows r1 = makerows(3, "abc", 21.0f, "", "cdefg", 22.0f, "aaa");
    rows_view rv(r1);
    rows r2(std::move(r1));
    r1 = makerows(2, 0, 0, 0, 0);  // r2 should be independent of r1

    BOOST_TEST(r2.size() == 2u);
    BOOST_TEST(r2[0] == makerow("abc", 21.0f, ""));
    BOOST_TEST(r2[1] == makerow("cdefg", 22.0f, "aaa"));
    BOOST_TEST(rv == r2);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(copy_assignment)
BOOST_AUTO_TEST_CASE(empty_to_empty)
{
    rows r1;
    rows r2;
    r1 = r2;
    r2 = makerows(2, 90, nullptr);  // r1 is independent of r2
    BOOST_TEST(r1.empty());
}

BOOST_AUTO_TEST_CASE(empty_to_nonempty)
{
    rows r1 = makerows(2, 42, "abcdef");
    rows r2;
    r1 = r2;
    r2 = makerows(2, 90, nullptr);  // r1 is independent of r2
    BOOST_TEST(r1.empty());
}

BOOST_AUTO_TEST_CASE(non_strings)
{
    rows r1 = makerows(2, 42, "abcdef");
    rows r2 = makerows(1, 50.0f, nullptr);
    r1 = r2;
    r2 = makerows(1, "abc", 80, nullptr);  // r1 is independent of r2

    BOOST_TEST(r1.size() == 2u);
    BOOST_TEST(r1[0] == makerow(50.0f));
    BOOST_TEST(r1[1] == makerow(nullptr));
}

BOOST_AUTO_TEST_CASE(strings)
{
    rows r1 = makerows(1, 42, "abcdef");
    rows r2 = makerows(2, "a_very_long_string", nullptr, "", "abc");
    r1 = r2;
    r2 = makerows(1, "another_string", 90, "yet_another");  // r1 is independent of r2

    BOOST_TEST(r1.size() == 2u);
    BOOST_TEST(r1[0] == makerow("a_very_long_string", nullptr));
    BOOST_TEST(r1[1] == makerow("", "abc"));
}

BOOST_AUTO_TEST_CASE(strings_empty_to)
{
    rows r1;
    rows r2 = makerows(1, "abc", nullptr, "");
    r1 = r2;

    BOOST_TEST(r1.size() == 3u);
    BOOST_TEST(r1[0] == makerow("abc"));
    BOOST_TEST(r1[1] == makerow(nullptr));
    BOOST_TEST(r1[2] == makerow(""));
}

BOOST_AUTO_TEST_CASE(self_assignment_empty)
{
    rows r;
    const rows& ref = r;
    r = ref;

    BOOST_TEST(r.empty());
}

BOOST_AUTO_TEST_CASE(self_assignment_non_empty)
{
    rows r = makerows(2, "abc", 50u, "fgh", "");
    const rows& ref = r;
    r = ref;

    BOOST_TEST(r.size() == 2u);
    BOOST_TEST(r[0] == makerow("abc", 50u));
    BOOST_TEST(r[1] == makerow("fgh", ""));
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(move_assignment)
BOOST_AUTO_TEST_CASE(empty_to_empty)
{
    rows r1;
    rows r2;
    rows_view rv(r2);
    r1 = std::move(r2);
    BOOST_TEST(r1.empty());
    BOOST_TEST(rv == r1);
}

BOOST_AUTO_TEST_CASE(empty_to_nonempty)
{
    rows r1 = makerows(1, 42, "abcdef");
    rows r2;
    rows_view rv(r2);
    r1 = std::move(r2);
    r2 = makerows(2, 90, nullptr);  // r1 is independent of r2
    BOOST_TEST(r1.empty());
    BOOST_TEST(rv == r1);
}

BOOST_AUTO_TEST_CASE(non_strings)
{
    rows r1 = makerows(2, 42, "abcdef");
    rows r2 = makerows(3, 50.0f, nullptr, 80u);
    rows_view rv(r2);
    r1 = std::move(r2);
    r2 = makerows(1, "abc", 80, nullptr);  // r1 is independent of r2

    BOOST_TEST(r1.size() == 1u);
    BOOST_TEST(r1[0] == makerow(50.0f, nullptr, 80u));
    BOOST_TEST(rv == r1);
}

BOOST_AUTO_TEST_CASE(strings)
{
    rows r1 = makerows(1, 42, "abcdef");
    rows r2 = makerows(2, "a_very_long_string", nullptr, "", "ppp");
    rows_view rv(r2);
    r1 = std::move(r2);
    r2 = makerows(1, "another_string", 90, "yet_another");  // r1 is independent of r2

    BOOST_TEST(r1.size() == 2u);
    BOOST_TEST(r1[0] == makerow("a_very_long_string", nullptr));
    BOOST_TEST(r1[1] == makerow("", "ppp"));
    BOOST_TEST(rv == r1);
}

BOOST_AUTO_TEST_CASE(strings_empty_to)
{
    rows r1;
    rows r2 = makerows(1, "abc", nullptr, "bcd");
    rows_view rv(r2);
    r1 = std::move(r2);

    BOOST_TEST(r1.size() == 3u);
    BOOST_TEST(r1[0] == makerow("abc"));
    BOOST_TEST(r1[1] == makerow(nullptr));
    BOOST_TEST(r1[2] == makerow("bcd"));
    BOOST_TEST(rv == r1);
}

BOOST_AUTO_TEST_CASE(self_assignment_empty)
{
    rows r;
    rows&& ref = std::move(r);
    r = std::move(ref);

    // r is in a valid but unspecified state; can be assigned to
    r = makerows(1, "abcdef");
    BOOST_TEST(r.size() == 1u);
    BOOST_TEST(r[0] == makerow("abcdef"));
}

BOOST_AUTO_TEST_CASE(self_assignment_non_empty)
{
    rows r = makerows(3, "abc", 50u, "fgh");
    rows&& ref = std::move(r);
    r = std::move(ref);  // this should leave r in a valid but unspecified state

    // r is in a valid but unspecified state; can be assigned to
    r = makerows(1, "abcdef");
    BOOST_TEST(r.size() == 1u);
    BOOST_TEST(r[0] == makerow("abcdef"));
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(assignment_from_view)
BOOST_AUTO_TEST_CASE(empty_to_empty)
{
    rows r;
    r = rows_view();
    BOOST_TEST(r.empty());
    BOOST_TEST(r.num_columns() == 0u);
}

BOOST_AUTO_TEST_CASE(empty_to_nonempty)
{
    rows r = makerows(1, 42, "abcdef");
    r = rows_view();
    BOOST_TEST(r.empty());
    BOOST_TEST(r.num_columns() == 0u);
}

BOOST_AUTO_TEST_CASE(empty_different_num_columns)
{
    rows r;
    r = rows_view(nullptr, 0, 2);

    BOOST_TEST(r.empty());
    BOOST_TEST(r.size() == 0u);
    BOOST_TEST(r.num_columns() == 2u);

    r = rows_view(nullptr, 0, 3);

    BOOST_TEST(r.empty());
    BOOST_TEST(r.size() == 0u);
    BOOST_TEST(r.num_columns() == 3u);
}

BOOST_AUTO_TEST_CASE(non_strings)
{
    rows r = makerows(1, 42, "abcdef");
    auto fields = make_field_views(90, nullptr, 4.2f, 1u);
    r = rows_view(fields.data(), fields.size(), 2);

    BOOST_TEST_REQUIRE(r.size() == 2u);
    BOOST_TEST(r[0] == makerow(90, nullptr));
    BOOST_TEST(r[1] == makerow(4.2f, 1u));
    BOOST_TEST(r.num_columns() == 2u);
}

BOOST_AUTO_TEST_CASE(strings)
{
    std::string s1("a_very_long_string"), s2("");
    rows r = makerows(1, 42, "abcdef", 90, "hij");
    auto fields = make_field_views(s1, nullptr, s2, "bec");
    r = rows_view(fields.data(), fields.size(), 2);
    fields = make_field_views("abc", 42u, 9, 0);  // r should be independent of the original fields
    s1 = "another_string";                        // r should be independent of the original strings
    s2 = "yet_another";

    BOOST_TEST_REQUIRE(r.size() == 2u);
    BOOST_TEST(r[0] == makerow("a_very_long_string", nullptr));
    BOOST_TEST(r[1] == makerow("", "bec"));
    BOOST_TEST(r.num_columns() == 2u);
}

BOOST_AUTO_TEST_CASE(strings_empty_to)
{
    rows r;
    auto fields = make_field_views("abc", nullptr, "bcd", 8.2f);
    r = rows_view(fields.data(), fields.size(), 4);

    BOOST_TEST_REQUIRE(r.size() == 1u);
    BOOST_TEST(r[0] == makerow("abc", nullptr, "bcd", 8.2f));
}

BOOST_AUTO_TEST_CASE(self_assignment)
{
    rows r = makerows(2, "abcdef", 42, "plk", "uv");
    r = rows_view(r);

    BOOST_TEST_REQUIRE(r.size() == 2u);
    BOOST_TEST(r[0] == makerow("abcdef", 42));
    BOOST_TEST(r[1] == makerow("plk", "uv"));
}

BOOST_AUTO_TEST_CASE(self_assignment_empty)
{
    rows r;
    r = rows_view(r);

    BOOST_TEST(r.empty());
    BOOST_TEST(r.size() == 0u);
}

BOOST_AUTO_TEST_CASE(self_assignment_cleared)
{
    rows r = makerows(2, "abcdef", 42, "plk", "uv");
    r.clear();
    r = rows_view(r);

    BOOST_TEST_REQUIRE(r.size() == 0u);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(at)
BOOST_AUTO_TEST_CASE(empty)
{
    rows v;
    BOOST_CHECK_THROW(v.at(0), std::out_of_range);
}

BOOST_AUTO_TEST_CASE(one_column_one_row)
{
    rows r = makerows(1, 42u);
    BOOST_TEST(r.at(0) == makerow(42u));
    BOOST_CHECK_THROW(r.at(1), std::out_of_range);
}

BOOST_AUTO_TEST_CASE(one_column_several_rows)
{
    rows r = makerows(1, 42u, "abc");
    BOOST_TEST(r.at(0) == makerow(42u));
    BOOST_TEST(r.at(1) == makerow("abc"));
    BOOST_CHECK_THROW(r.at(2), std::out_of_range);
}

BOOST_AUTO_TEST_CASE(several_columns_one_row)
{
    rows r = makerows(2, 42u, "abc");
    BOOST_TEST(r.at(0) == makerow(42u, "abc"));
    BOOST_CHECK_THROW(r.at(1), std::out_of_range);
}

BOOST_AUTO_TEST_CASE(several_columns_several_rows)
{
    rows r = makerows(2, 42u, "abc", nullptr, "bcd", 90u, nullptr);
    BOOST_TEST(r.at(0) == makerow(42u, "abc"));
    BOOST_TEST(r.at(1) == makerow(nullptr, "bcd"));
    BOOST_TEST(r.at(2) == makerow(90u, nullptr));
    BOOST_CHECK_THROW(r.at(3), std::out_of_range);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(operator_square_brackets)
BOOST_AUTO_TEST_CASE(one_column_one_row)
{
    rows r = makerows(1, 42u);
    BOOST_TEST(r[0] == makerow(42u));
}

BOOST_AUTO_TEST_CASE(one_column_several_rows)
{
    rows r = makerows(1, 42u, "abc");
    BOOST_TEST(r[0] == makerow(42u));
    BOOST_TEST(r[1] == makerow("abc"));
}

BOOST_AUTO_TEST_CASE(several_columns_one_row)
{
    rows r = makerows(2, 42u, "abc");
    BOOST_TEST(r[0] == makerow(42u, "abc"));
}

BOOST_AUTO_TEST_CASE(several_columns_several_rows)
{
    rows r = makerows(2, 42u, "abc", nullptr, "bcd", 90u, nullptr);
    BOOST_TEST(r[0] == makerow(42u, "abc"));
    BOOST_TEST(r[1] == makerow(nullptr, "bcd"));
    BOOST_TEST(r[2] == makerow(90u, nullptr));
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(front)
{
    rows r = makerows(2, 42u, "abc", nullptr, "bcde");
    BOOST_TEST(r.front() == makerow(42u, "abc"));
}

BOOST_AUTO_TEST_CASE(back)
{
    rows r = makerows(2, 70.0f, "abc", nullptr, "bcde");
    BOOST_TEST(r.back() == makerow(nullptr, "bcde"));
}

BOOST_AUTO_TEST_CASE(empty)
{
    BOOST_TEST(rows().empty());
    BOOST_TEST(!makerows(1, 42u).empty());
}

BOOST_AUTO_TEST_SUITE(size)
BOOST_AUTO_TEST_CASE(empty)
{
    rows r;
    BOOST_TEST(r.size() == 0u);
}

BOOST_AUTO_TEST_CASE(one_column_one_row)
{
    rows r = makerows(1, 42u);
    BOOST_TEST(r.size() == 1u);
}

BOOST_AUTO_TEST_CASE(one_column_several_rows)
{
    rows r = makerows(1, 42u, "abc");
    BOOST_TEST(r.size() == 2u);
}

BOOST_AUTO_TEST_CASE(several_columns_one_row)
{
    rows r = makerows(2, 42u, "abc");
    BOOST_TEST(r.size() == 1u);
}

BOOST_AUTO_TEST_CASE(several_columns_several_rows)
{
    rows r = makerows(3, 42u, "abc", nullptr, "bcd", 90u, nullptr);
    BOOST_TEST(r.size() == 2u);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(operator_rows_view)
BOOST_AUTO_TEST_CASE(empty)
{
    rows r;
    auto rv = static_cast<rows_view>(r);
    BOOST_TEST(rv.size() == 0u);
}

BOOST_AUTO_TEST_CASE(non_empty)
{
    rows r = makerows(3, 42u, 4.2f, "abcde", 90u, nullptr, "def");
    auto rv = static_cast<rows_view>(r);
    BOOST_TEST(rv.size() == 2u);
    BOOST_TEST(rv[0] == makerow(42u, 4.2f, "abcde"));
    BOOST_TEST(rv[1] == makerow(90u, nullptr, "def"));
}

BOOST_AUTO_TEST_CASE(cleared)
{
    rows r = makerows(3, 42u, 4.2f, "abcde", 90u, nullptr, "def");
    r = rows();
    auto rv = static_cast<rows_view>(r);
    BOOST_TEST(rv.empty());
    BOOST_TEST(rv.size() == 0u);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
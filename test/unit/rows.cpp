//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/field_view.hpp>
#include <boost/mysql/rows.hpp>
#include <boost/mysql/rows_view.hpp>

#include <boost/test/unit_test.hpp>

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
    rows r2(std::move(r1));
    BOOST_TEST(r2.empty());
}

BOOST_AUTO_TEST_CASE(non_strings)
{
    rows r1 = makerows(3, 1, 21.0f, nullptr, 2, 22.0f, -1);
    rows r2(std::move(r1));
    r1 = makerows(2, 0, 0, 0, 0);  // r2 should be independent of r1

    BOOST_TEST(r2.size() == 2u);
    BOOST_TEST(r2[0] == makerow(1, 21.0f, nullptr));
    BOOST_TEST(r2[1] == makerow(2, 22.0f, -1));
}

BOOST_AUTO_TEST_CASE(strings)
{
    rows r1 = makerows(3, "abc", 21.0f, "", "cdefg", 22.0f, "aaa");
    rows r2(std::move(r1));
    r1 = makerows(2, 0, 0, 0, 0);  // r2 should be independent of r1

    BOOST_TEST(r2.size() == 2u);
    BOOST_TEST(r2[0] == makerow("abc", 21.0f, ""));
    BOOST_TEST(r2[1] == makerow("cdefg", 22.0f, "aaa"));
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(copy_assignment)
BOOST_AUTO_TEST_CASE(empty)
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
BOOST_AUTO_TEST_CASE(empty)
{
    rows r1 = makerows(1, 42, "abcdef");
    rows r2;
    r1 = std::move(r2);
    r2 = makerows(2, 90, nullptr);  // r1 is independent of r2
    BOOST_TEST(r1.empty());
}

BOOST_AUTO_TEST_CASE(non_strings)
{
    rows r1 = makerows(2, 42, "abcdef");
    rows r2 = makerows(3, 50.0f, nullptr, 80u);
    r1 = std::move(r2);
    r2 = makerows(1, "abc", 80, nullptr);  // r1 is independent of r2

    BOOST_TEST(r1.size() == 1u);
    BOOST_TEST(r1[0] == makerow(50.0f, nullptr, 80u));
}

BOOST_AUTO_TEST_CASE(strings)
{
    rows r1 = makerows(1, 42, "abcdef");
    rows r2 = makerows(2, "a_very_long_string", nullptr, "", "ppp");
    r1 = std::move(r2);
    r2 = makerows(1, "another_string", 90, "yet_another");  // r1 is independent of r2

    BOOST_TEST(r1.size() == 2u);
    BOOST_TEST(r1[0] == makerow("a_very_long_string", nullptr));
    BOOST_TEST(r1[1] == makerow("", "ppp"));
}

BOOST_AUTO_TEST_CASE(strings_empty_to)
{
    rows r1;
    rows r2 = makerows(1, "abc", nullptr, "bcd");
    r1 = std::move(r2);

    BOOST_TEST(r1.size() == 3u);
    BOOST_TEST(r1[0] == makerow("abc"));
    BOOST_TEST(r1[1] == makerow(nullptr));
    BOOST_TEST(r1[2] == makerow("bcd"));
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

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/field_view.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/row_view.hpp>

#include <boost/mysql/detail/auxiliar/stringize.hpp>

#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

#include "test_common.hpp"

using boost::mysql::field;
using boost::mysql::field_view;
using boost::mysql::make_field_views;
using boost::mysql::row;
using boost::mysql::row_view;
using boost::mysql::detail::stringize;
using boost::mysql::test::makerow;

BOOST_AUTO_TEST_SUITE(test_row)

BOOST_AUTO_TEST_CASE(default_ctor)
{
    row r;
    BOOST_TEST(r.empty());
}

BOOST_AUTO_TEST_SUITE(ctor_from_row_view)
BOOST_AUTO_TEST_CASE(empty)
{
    row_view v;
    row r(v);
    BOOST_TEST(r.empty());
}

BOOST_AUTO_TEST_CASE(non_strings)
{
    auto fields = make_field_views(42, 5.0f);
    row r(row_view(fields.data(), fields.size()));

    // Fields still valid even when the original source of the view changed
    fields = make_field_views(90, 2.0);
    BOOST_TEST(r.size() == 2u);
    BOOST_TEST(r[0] == field_view(42));
    BOOST_TEST(r[1] == field_view(5.0f));
}

BOOST_AUTO_TEST_CASE(strings)
{
    std::string s1("test"), s2("");
    auto fields = make_field_views(s1, s2, 50);
    row r(row_view(fields.data(), fields.size()));

    // Fields still valid even when the original strings changed
    s1 = "other";
    s2 = "abcdef";
    BOOST_TEST(r.size() == 3u);
    BOOST_TEST(r[0] == field_view("test"));
    BOOST_TEST(r[1] == field_view(""));
    BOOST_TEST(r[2] == field_view(50));
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(copy_ctor)
BOOST_AUTO_TEST_CASE(empty)
{
    row r1;
    row r2(r1);
    r1 = makerow(42, "test");  // r2 should be independent of r1

    BOOST_TEST(r2.empty());
}

BOOST_AUTO_TEST_CASE(non_strings)
{
    row r1 = makerow(42, 5.0f);
    row r2(r1);
    r1 = makerow(42, "test");  // r2 should be independent of r1

    BOOST_TEST(r2.size() == 2u);
    BOOST_TEST(r2[0] == field_view(42));
    BOOST_TEST(r2[1] == field_view(5.0f));
}

BOOST_AUTO_TEST_CASE(strings)
{
    row r1 = makerow("", 42, "test");
    row r2(r1);
    r1 = makerow("another_string", 4.2f, "");  // r2 should be independent of r1

    BOOST_TEST(r2.size() == 3u);
    BOOST_TEST(r2[0] == field_view(""));
    BOOST_TEST(r2[1] == field_view(42));
    BOOST_TEST(r2[2] == field_view("test"));
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(move_ctor)
BOOST_AUTO_TEST_CASE(empty)
{
    row r1;
    row_view rv(r1);
    row r2(std::move(r1));
    r1 = makerow(42, "test");  // r2 should be independent of r1

    BOOST_TEST(r2.empty());
    BOOST_TEST(rv.empty());  // references still valid
}

BOOST_AUTO_TEST_CASE(non_strings)
{
    row r1 = makerow(42, 5.0f);
    row_view rv(r1);
    const field_view* begin_before = r1.begin();  // iterators are not invalidated by move
    row r2(std::move(r1));
    r1 = makerow(42, "test");  // r2 should be independent of r1

    BOOST_TEST(r2.size() == 2u);
    BOOST_TEST(r2[0] == field_view(42));
    BOOST_TEST(r2[1] == field_view(5.0f));
    BOOST_TEST(r2.begin() == begin_before);
    BOOST_TEST(rv == r2);  // references still valid
}

BOOST_AUTO_TEST_CASE(strings)
{
    row r1 = makerow("", 42, "test");
    row_view rv(r1);
    const char* str_begin_before = r1[2].as_string().data();  // ptrs to strs not invalidated
    row r2(std::move(r1));
    r1 = makerow("another_string", 4.2f, "");  // r2 should be independent of r1

    BOOST_TEST(r2.size() == 3u);
    BOOST_TEST(r2[0] == field_view(""));
    BOOST_TEST(r2[1] == field_view(42));
    BOOST_TEST(r2[2] == field_view("test"));
    BOOST_TEST(rv == r2);  // references still valid

    // Check that pointers to strings are not invalidated.
    // Cast them to void* so that UTF doesn't try to print them and
    // generate valgrind errors
    BOOST_TEST(
        static_cast<const void*>(r2[2].as_string().data()) ==
        static_cast<const void*>(str_begin_before)
    );
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(copy_assignment)
BOOST_AUTO_TEST_CASE(empty)
{
    row r1 = makerow(42, "abcdef");
    row r2;
    r1 = r2;
    r2 = makerow(90, nullptr);  // r1 is independent of r2
    BOOST_TEST(r1.empty());
}

BOOST_AUTO_TEST_CASE(non_strings)
{
    row r1 = makerow(42, "abcdef");
    row r2 = makerow(50.0f, nullptr, 80u);
    r1 = r2;
    r2 = makerow("abc", 80, nullptr);  // r1 is independent of r2

    BOOST_TEST(r1.size() == 3u);
    BOOST_TEST(r1[0] == field_view(50.0f));
    BOOST_TEST(r1[1] == field_view());
    BOOST_TEST(r1[2] == field_view(80u));
}

BOOST_AUTO_TEST_CASE(strings)
{
    row r1 = makerow(42, "abcdef");
    row r2 = makerow("a_very_long_string", nullptr, "");
    r1 = r2;
    r2 = makerow("another_string", 90, "yet_another");  // r1 is independent of r2

    BOOST_TEST(r1.size() == 3u);
    BOOST_TEST(r1[0] == field_view("a_very_long_string"));
    BOOST_TEST(r1[1] == field_view());
    BOOST_TEST(r1[2] == field_view(""));
}

BOOST_AUTO_TEST_CASE(strings_empty_to)
{
    row r1;
    row r2 = makerow("abc", nullptr, "bcd");
    r1 = r2;

    BOOST_TEST(r1.size() == 3u);
    BOOST_TEST(r1[0] == field_view("abc"));
    BOOST_TEST(r1[1] == field_view());
    BOOST_TEST(r1[2] == field_view("bcd"));
}

BOOST_AUTO_TEST_CASE(self_assignment_empty)
{
    row r;
    const row& ref = r;
    r = ref;

    BOOST_TEST(r.empty());
}

BOOST_AUTO_TEST_CASE(self_assignment_non_empty)
{
    row r = makerow("abc", 50u, "fgh");
    const row& ref = r;
    r = ref;

    BOOST_TEST(r.size() == 3u);
    BOOST_TEST(r[0] == field_view("abc"));
    BOOST_TEST(r[1] == field_view(50u));
    BOOST_TEST(r[2] == field_view("fgh"));
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(move_assignment)
BOOST_AUTO_TEST_CASE(empty)
{
    row r1 = makerow(42, "abcdef");
    row r2;
    row_view rv(r2);
    r1 = std::move(r2);
    r2 = makerow(90, nullptr);  // r1 is independent of r2
    BOOST_TEST(r1.empty());
    BOOST_TEST(rv == r1);
}

BOOST_AUTO_TEST_CASE(non_strings)
{
    row r1 = makerow(42, "abcdef");
    row r2 = makerow(50.0f, nullptr, 80u);
    row_view rv(r2);
    r1 = std::move(r2);
    r2 = makerow("abc", 80, nullptr);  // r1 is independent of r2

    BOOST_TEST(r1.size() == 3u);
    BOOST_TEST(r1[0] == field_view(50.0f));
    BOOST_TEST(r1[1] == field_view());
    BOOST_TEST(r1[2] == field_view(80u));
    BOOST_TEST(rv == r1);
}

BOOST_AUTO_TEST_CASE(strings)
{
    row r1 = makerow(42, "abcdef");
    row r2 = makerow("a_very_long_string", nullptr, "");
    row_view rv(r2);
    r1 = std::move(r2);
    r2 = makerow("another_string", 90, "yet_another");  // r1 is independent of r2

    BOOST_TEST(r1.size() == 3u);
    BOOST_TEST(r1[0] == field_view("a_very_long_string"));
    BOOST_TEST(r1[1] == field_view());
    BOOST_TEST(r1[2] == field_view(""));
    BOOST_TEST(rv == r1);
}

BOOST_AUTO_TEST_CASE(strings_empty_to)
{
    row r1;
    row r2 = makerow("abc", nullptr, "bcd");
    row_view rv(r2);
    r1 = std::move(r2);

    BOOST_TEST(r1.size() == 3u);
    BOOST_TEST(r1[0] == field_view("abc"));
    BOOST_TEST(r1[1] == field_view());
    BOOST_TEST(r1[2] == field_view("bcd"));
    BOOST_TEST(rv == r1);
}

BOOST_AUTO_TEST_CASE(self_assignment_empty)
{
    row r;
    row&& ref = std::move(r);
    r = std::move(ref);

    // r is in a valid but unspecified state; can be assigned to
    r = makerow("abcdef");
    BOOST_TEST(r.size() == 1u);
    BOOST_TEST(r[0] == field_view("abcdef"));
}

BOOST_AUTO_TEST_CASE(self_assignment_non_empty)
{
    row r = makerow("abc", 50u, "fgh");
    row&& ref = std::move(r);
    r = std::move(ref);  // this should leave r in a valid but unspecified state

    // r is in a valid but unspecified state; can be assigned to
    r = makerow("abcdef");
    BOOST_TEST(r.size() == 1u);
    BOOST_TEST(r[0] == field_view("abcdef"));
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(assignment_from_view)
BOOST_AUTO_TEST_CASE(empty)
{
    row r = makerow(42, "abcdef");
    r = row();
    BOOST_TEST(r.empty());
}

BOOST_AUTO_TEST_CASE(non_strings)
{
    row r = makerow(42, "abcdef");
    auto fields = make_field_views(90, nullptr);
    r = row_view(fields.data(), fields.size());
    fields = make_field_views("abc", 42u);  // r should be independent of the original fields

    BOOST_TEST(r.size() == 2u);
    BOOST_TEST(r[0] == field_view(90));
    BOOST_TEST(r[1] == field_view());
}

BOOST_AUTO_TEST_CASE(strings)
{
    std::string s1("a_very_long_string"), s2("");
    row r = makerow(42, "abcdef");
    auto fields = make_field_views(s1, nullptr, s2);
    r = row_view(fields.data(), fields.size());
    fields = make_field_views("abc", 42u, 9);  // r should be independent of the original fields
    s1 = "another_string";                     // r should be independent of the original strings
    s2 = "yet_another";

    BOOST_TEST(r.size() == 3u);
    BOOST_TEST(r[0] == field_view("a_very_long_string"));
    BOOST_TEST(r[1] == field_view());
    BOOST_TEST(r[2] == field_view(""));
}

BOOST_AUTO_TEST_CASE(strings_empty_to)
{
    row r;
    auto fields = make_field_views("abc", nullptr, "bcd");
    r = row_view(fields.data(), fields.size());

    BOOST_TEST(r.size() == 3u);
    BOOST_TEST(r[0] == field_view("abc"));
    BOOST_TEST(r[1] == field_view());
    BOOST_TEST(r[2] == field_view("bcd"));
}

BOOST_AUTO_TEST_CASE(self_assignment)
{
    row r = makerow("abcdef", 42, "plk");
    r = row_view(r);

    BOOST_TEST(r.size() == 3u);
    BOOST_TEST(r[0] == field_view("abcdef"));
    BOOST_TEST(r[1] == field_view(42));
    BOOST_TEST(r[2] == field_view("plk"));
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(at)
BOOST_AUTO_TEST_CASE(empty)
{
    row r;
    BOOST_CHECK_THROW(r.at(0), std::out_of_range);
}

BOOST_AUTO_TEST_CASE(in_range)
{
    row r = makerow(42, 50u, "test");
    BOOST_TEST(r.at(0) == field_view(42));
    BOOST_TEST(r.at(1) == field_view(50u));
    BOOST_TEST(r.at(2) == field_view("test"));
}

BOOST_AUTO_TEST_CASE(out_of_range)
{
    row r = makerow(42, 50u, "test");
    BOOST_CHECK_THROW(r.at(3), std::out_of_range);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(front)
{
    auto r = makerow(42, 50u, "test");
    BOOST_TEST(r.front() == field_view(42));
}

BOOST_AUTO_TEST_CASE(back)
{
    BOOST_TEST(makerow(42, 50u, "test").front() == field_view(42));
    BOOST_TEST(makerow(42).back() == field_view(42));
}

BOOST_AUTO_TEST_CASE(empty)
{
    BOOST_TEST(row().empty());
    BOOST_TEST(!makerow(42).empty());
    BOOST_TEST(!makerow(42, 50u).empty());
}

BOOST_AUTO_TEST_CASE(size)
{
    BOOST_TEST(row().size() == 0u);
    BOOST_TEST(makerow(42).size() == 1u);
    BOOST_TEST(makerow(50, nullptr).size() == 2u);
}

// As iterators are regular pointers, we don't perform
// exhaustive testing on iteration
BOOST_AUTO_TEST_SUITE(iterators)
BOOST_AUTO_TEST_CASE(empty)
{
    const row r{};  // can be called on const objects
    BOOST_TEST(r.begin() == nullptr);
    BOOST_TEST(r.end() == nullptr);
    std::vector<field_view> vec{r.begin(), r.end()};
    BOOST_TEST(vec.empty());
}

BOOST_AUTO_TEST_CASE(multiple_elms)
{
    const row r = makerow(42, 50u, "test");  // can be called on const objects
    BOOST_TEST(r.begin() != nullptr);
    BOOST_TEST(r.end() != nullptr);
    BOOST_TEST(std::distance(r.begin(), r.end()) == 3);

    std::vector<field_view> vec{r.begin(), r.end()};
    BOOST_TEST(vec.size() == 3u);
    BOOST_TEST(vec[0] == field_view(42));
    BOOST_TEST(vec[1] == field_view(50u));
    BOOST_TEST(vec[2] == field_view("test"));
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(operator_row_view)
BOOST_AUTO_TEST_CASE(empty)
{
    row r;
    row_view rv(r);

    BOOST_TEST(rv.empty());
    BOOST_TEST(rv.size() == 0u);
    BOOST_TEST(rv.begin() == nullptr);
    BOOST_TEST(rv.end() == nullptr);
}

BOOST_AUTO_TEST_CASE(non_empty)
{
    row r = makerow("abc", 24, "def");
    row_view rv(r);

    BOOST_TEST(rv.size() == 3u);
    BOOST_TEST(rv[0] == field_view("abc"));
    BOOST_TEST(rv[1] == field_view(24));
    BOOST_TEST(rv[2] == field_view("def"));
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(as_vector)
BOOST_AUTO_TEST_CASE(empty)
{
    std::vector<field> vec{field_view("abc")};
    row r;
    r.as_vector(vec);
    BOOST_TEST(vec.empty());
}

BOOST_AUTO_TEST_CASE(non_empty)
{
    std::vector<field> vec{field_view("abc")};
    row r = makerow(42u, "abc");
    r.as_vector(vec);
    BOOST_TEST(vec.size() == 2u);
    BOOST_TEST(vec[0].as_uint64() == 42u);
    BOOST_TEST(vec[1].as_string() == "abc");
}

BOOST_AUTO_TEST_CASE(return_value)
{
    auto vec = makerow(42u, "abc").as_vector();
    BOOST_TEST(vec.size() == 2u);
    BOOST_TEST(vec[0].as_uint64() == 42u);
    BOOST_TEST(vec[1].as_string() == "abc");
}
BOOST_AUTO_TEST_SUITE_END()

// operator== relies on row_view's operator==, so only
// a small subset of tests here
BOOST_AUTO_TEST_SUITE(operator_equals)
BOOST_AUTO_TEST_CASE(row_row)
{
    row r1 = makerow("abc", 4);
    row r2 = r1;
    row r3 = makerow(nullptr, 4);

    BOOST_TEST(r1 == r2);
    BOOST_TEST(!(r1 != r2));

    BOOST_TEST(!(r1 == r3));
    BOOST_TEST(r1 != r3);
}

BOOST_AUTO_TEST_CASE(row_rowview)
{
    row r1 = makerow("abc", 4);
    row r2 = makerow(nullptr, 4);
    auto fields = make_field_views("abc", 4);
    row_view rv(fields.data(), fields.size());

    BOOST_TEST(r1 == rv);
    BOOST_TEST(!(r1 != rv));
    BOOST_TEST(rv == r1);
    BOOST_TEST(!(rv != r1));

    BOOST_TEST(!(r2 == rv));
    BOOST_TEST(r2 != rv);
    BOOST_TEST(!(rv == r2));
    BOOST_TEST(rv != r2);
}
BOOST_AUTO_TEST_SUITE_END()

// operator<< relies on row_view's operator<<, so only
// a small subset of tests here
BOOST_AUTO_TEST_CASE(operator_stream)
{
    row r = makerow("abc", nullptr);
    BOOST_TEST(stringize(r) == "{abc, <NULL>}");
}

BOOST_AUTO_TEST_SUITE_END()  // test_row

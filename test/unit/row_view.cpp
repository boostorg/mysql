//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/row_view.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/detail/auxiliar/stringize.hpp>
#include <boost/test/tools/context.hpp>
#include <boost/test/unit_test.hpp>
#include <cstddef>
#include <stdexcept>
#include <vector>
#include "test_common.hpp"

using boost::mysql::row_view;
using boost::mysql::field_view;
using boost::mysql::detail::stringize;
using boost::mysql::make_field_views;
using boost::mysql::test::make_fv_vector;

namespace
{

BOOST_AUTO_TEST_SUITE(test_row_view)

BOOST_AUTO_TEST_CASE(default_ctor)
{
    row_view v;
    BOOST_TEST(v.empty());
}

BOOST_AUTO_TEST_SUITE(at)
BOOST_AUTO_TEST_CASE(empty)
{
    row_view v;
    BOOST_CHECK_THROW(v.at(0), std::out_of_range);
}

BOOST_AUTO_TEST_CASE(in_range)
{
    auto fields = make_field_views(42, 50u, "test");
    row_view v (fields.begin(), fields.size());
    BOOST_TEST(v.at(0) == field_view(42));
    BOOST_TEST(v.at(1) == field_view(50u));
    BOOST_TEST(v.at(2) == field_view("test"));
}

BOOST_AUTO_TEST_CASE(out_of_range)
{
    auto fields = make_field_views(42, 50u, "test");
    row_view v (fields.begin(), fields.size());
    BOOST_CHECK_THROW(v.at(3), std::out_of_range);
}
BOOST_AUTO_TEST_SUITE_END()



BOOST_AUTO_TEST_CASE(operator_square_brackets)
{
    auto fields = make_field_views(42, 50u, "test");
    row_view v (fields.begin(), fields.size());
    BOOST_TEST(v[0] == field_view(42));
    BOOST_TEST(v[1] == field_view(50u));
    BOOST_TEST(v[2] == field_view("test"));
}

BOOST_AUTO_TEST_CASE(front)
{
    auto fields = make_field_views(42, 50u, "test");
    row_view v (fields.begin(), fields.size());
    BOOST_TEST(v.front() == field_view(42));
}


BOOST_AUTO_TEST_SUITE(back)
BOOST_AUTO_TEST_CASE(multiple_elms)
{
    auto fields = make_field_views(42, 50u, "test");
    row_view v (fields.begin(), fields.size());
    BOOST_TEST(v.back() == field_view("test"));
}

BOOST_AUTO_TEST_CASE(single_elm)
{
    auto fields = make_field_views(42);
    row_view v (fields.begin(), fields.size());
    BOOST_TEST(v.back() == field_view(42));
}
BOOST_AUTO_TEST_SUITE_END()


BOOST_AUTO_TEST_SUITE(empty)
BOOST_AUTO_TEST_CASE(true_)
{
    BOOST_TEST(row_view().empty());
}

BOOST_AUTO_TEST_CASE(single_elm)
{
    auto fields = make_field_views(42);
    row_view v (fields.begin(), fields.size());
    BOOST_TEST(!v.empty());
}

BOOST_AUTO_TEST_CASE(multiple_elms)
{
    auto fields = make_field_views(42, 50u, "test");
    row_view v (fields.begin(), fields.size());
    BOOST_TEST(!v.empty());
}
BOOST_AUTO_TEST_SUITE_END()


BOOST_AUTO_TEST_SUITE(size)
BOOST_AUTO_TEST_CASE(zero)
{
    BOOST_TEST(row_view().size() == 0);
}

BOOST_AUTO_TEST_CASE(single_elm)
{
    auto fields = make_field_views(42);
    row_view v (fields.begin(), fields.size());
    BOOST_TEST(v.size() == 1);
}

BOOST_AUTO_TEST_CASE(multiple_elms)
{
    auto fields = make_field_views(42, 50u, "test");
    row_view v (fields.begin(), fields.size());
    BOOST_TEST(v.size() == 3);
}
BOOST_AUTO_TEST_SUITE_END()

// As iterators are regular pointers, we don't perform
// exhaustive testing on iteration
BOOST_AUTO_TEST_SUITE(iterators)
BOOST_AUTO_TEST_CASE(empty)
{
    const row_view v; // can be called on const objects
    BOOST_TEST(v.begin() == nullptr);
    BOOST_TEST(v.end() == nullptr);
    std::vector<field_view> vec { v.begin(), v.end() };
    BOOST_TEST(vec.empty());
}

BOOST_AUTO_TEST_CASE(multiple_elms)
{
    auto fields = make_field_views(42, 50u, "test");
    const row_view v (fields.begin(), fields.size()); // can be called on const objects
    BOOST_TEST(v.begin() != nullptr);
    BOOST_TEST(v.end() != nullptr);
    BOOST_TEST(std::distance(v.begin(), v.end()) == 3);

    std::vector<field_view> vec { v.begin(), v.end() };
    BOOST_TEST(vec.size() == 3);
    BOOST_TEST(vec[0] == field_view(42));
    BOOST_TEST(vec[1] == field_view(50u));
    BOOST_TEST(vec[2] == field_view("test"));
}
BOOST_AUTO_TEST_SUITE_END()


BOOST_AUTO_TEST_CASE(operator_equals)
{
    struct
    {
        const char* name;
        std::vector<field_view> f1;
        std::vector<field_view> f2;
        bool is_equal;
    } test_cases [] = {
        { "empty_empty", make_fv_vector(), make_fv_vector(), true },
        { "empty_nonempty", make_fv_vector(), make_fv_vector("test"), false },
        { "subset", make_fv_vector("test", 42), make_fv_vector("test"), false },
        { "same_size_different_values", make_fv_vector("test", 42), make_fv_vector("test", 50), false },
        { "same_size_and_values", make_fv_vector("test", 42), make_fv_vector("test", 42), true }
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            row_view v1 (tc.f1.data(), tc.f1.size());
            row_view v2 (tc.f2.data(), tc.f2.size());

            if (tc.is_equal)
            {
                BOOST_TEST(v1 == v2);
                BOOST_TEST(v2 == v1);
                BOOST_TEST(!(v1 != v2));
                BOOST_TEST(!(v2 != v1));
            }
            else
            {
                BOOST_TEST(!(v1 == v2));
                BOOST_TEST(!(v2 == v1));
                BOOST_TEST(v1 != v2);
                BOOST_TEST(v2 != v1);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(operator_stream)
{
    struct
    {
        const char* name;
        std::vector<field_view> fields;
        const char* expected;
    } test_cases [] = {
        { "empty", make_fv_vector(), "{}" },
        { "one_element", make_fv_vector(42), "{42}" },
        { "two_elements", make_fv_vector("test", nullptr), "{test, <NULL>}" },
        { "three_elements", make_fv_vector("value", std::uint32_t(2019), 3.14f), "{value, 2019, 3.14}" }
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            row_view v (tc.fields.data(), tc.fields.size());
            BOOST_TEST(stringize(v) == tc.expected);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

}
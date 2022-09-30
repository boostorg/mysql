//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/row_view.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/test/unit_test.hpp>
#include <cstddef>
#include <stdexcept>
#include <vector>

using boost::mysql::row_view;
using boost::mysql::field_view;
using boost::mysql::make_field_views;

namespace
{

BOOST_AUTO_TEST_SUITE(test_row_view)

BOOST_AUTO_TEST_CASE(default_ctor)
{
    row_view v;
    BOOST_TEST(v.empty());
}

BOOST_AUTO_TEST_CASE(at_empty)
{
    row_view v;
    BOOST_CHECK_THROW(v.at(0), std::out_of_range);
}

BOOST_AUTO_TEST_CASE(at_in_range)
{
    auto fields = make_field_views(42, 50u, "test");
    row_view v (fields.begin(), fields.size());
    BOOST_TEST(v.at(0) == field_view(42));
    BOOST_TEST(v.at(1) == field_view(50u));
    BOOST_TEST(v.at(2) == field_view("test"));
}

BOOST_AUTO_TEST_CASE(at_out_of_range)
{
    auto fields = make_field_views(42, 50u, "test");
    row_view v (fields.begin(), fields.size());
    BOOST_CHECK_THROW(v.at(3), std::out_of_range);
}

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

BOOST_AUTO_TEST_CASE(back_multiple_elms)
{
    auto fields = make_field_views(42, 50u, "test");
    row_view v (fields.begin(), fields.size());
    BOOST_TEST(v.back() == field_view("test"));
}

BOOST_AUTO_TEST_CASE(back_single_elm)
{
    auto fields = make_field_views(42);
    row_view v (fields.begin(), fields.size());
    BOOST_TEST(v.back() == field_view(42));
}

BOOST_AUTO_TEST_CASE(empty_true)
{
    BOOST_TEST(row_view().empty());
}

BOOST_AUTO_TEST_CASE(empty_single_elm)
{
    auto fields = make_field_views(42);
    row_view v (fields.begin(), fields.size());
    BOOST_TEST(!v.empty());
}

BOOST_AUTO_TEST_CASE(empty_multiple_elms)
{
    auto fields = make_field_views(42, 50u, "test");
    row_view v (fields.begin(), fields.size());
    BOOST_TEST(!v.empty());
}

BOOST_AUTO_TEST_CASE(size_zero)
{
    BOOST_TEST(row_view().size() == 0);
}

BOOST_AUTO_TEST_CASE(size_single_elm)
{
    auto fields = make_field_views(42);
    row_view v (fields.begin(), fields.size());
    BOOST_TEST(v.size() == 1);
}

BOOST_AUTO_TEST_CASE(size_multiple_elms)
{
    auto fields = make_field_views(42, 50u, "test");
    row_view v (fields.begin(), fields.size());
    BOOST_TEST(v.size() == 3);
}

// As iterators are regular pointers, we don't perform
// exhaustive testing on iteration
BOOST_AUTO_TEST_CASE(iterators_empty)
{
    const row_view v; // can be called on const objects
    BOOST_TEST(v.begin() == nullptr);
    BOOST_TEST(v.end() == nullptr);
    std::vector<field_view> vec { v.begin(), v.end() };
    BOOST_TEST(vec.empty());
}

BOOST_AUTO_TEST_CASE(iterators_multiple_elms)
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

}
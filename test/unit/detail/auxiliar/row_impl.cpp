//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/blob.hpp>
#include <boost/mysql/date.hpp>
#include <boost/mysql/field_view.hpp>

#include <boost/mysql/detail/auxiliar/row_impl.hpp>
#include <boost/mysql/detail/auxiliar/static_string.hpp>
#include <boost/mysql/detail/auxiliar/string_view_offset.hpp>

#include <boost/test/unit_test.hpp>

#include <algorithm>

#include "test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::blob;
using boost::mysql::date;
using boost::mysql::field_view;
using boost::mysql::detail::row_impl;

namespace {

// default ctor => empty
// copy empty, scalars, strings, blobs, empty strings, empty blobs
// move empty, scalars, strings
// copy assign empty, scalars, strings, blobs, empty strings, empty blobs, target empty, target scalars,
//    target strings, self assign scalars, self assign strings
// move assign empty, scalars, strings, target empty, target scalars, target strings, self assign, self assign
//    strings
// ctor from span: empty span, null span, scalars, strings, blobs, empty strings, empty blobs
// assign from span: empty span, null span, scalars, strings, blobs, empty strings, empty blobs, target empty,
//    target scalars, target strings, self assign

template <class... T>
row_impl makerwimpl(T&&... args)
{
    auto fields = make_fv_arr(std::forward<T>(args)...);
    return row_impl(fields.data(), fields.size());
}

template <class... T>
void add_fields(row_impl& r, T&&... args)
{
    auto fields = make_fv_arr(std::forward<T>(args)...);
    std::copy(fields.begin(), fields.end(), r.add_fields(fields.size()));
}

BOOST_AUTO_TEST_SUITE(test_row_impl)

BOOST_AUTO_TEST_SUITE(add_fields_)
BOOST_AUTO_TEST_CASE(empty_collection)
{
    row_impl r;
    field_view* storage = r.add_fields(2);
    BOOST_TEST(r.fields().size() == 2u);
    BOOST_TEST(storage == r.fields().data());
}

BOOST_AUTO_TEST_CASE(non_empty_collection)
{
    row_impl r = makerwimpl(nullptr, nullptr);
    field_view* storage = r.add_fields(3);
    BOOST_TEST(r.fields().size() == 5u);
    BOOST_TEST(storage == r.fields().data() + 2);
}

BOOST_AUTO_TEST_CASE(zero_fields)
{
    row_impl r = makerwimpl(nullptr, nullptr);
    field_view* storage = r.add_fields(0);
    BOOST_TEST(r.fields().size() == 2u);
    BOOST_TEST(storage == r.fields().data() + 2);
}

BOOST_AUTO_TEST_CASE(empty_collection_zero_fields)
{
    row_impl r;
    field_view* storage = r.add_fields(0);
    BOOST_TEST(r.fields().size() == 0u);
    BOOST_TEST(storage == r.fields().data());
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(copy_strings_as_offsets)
BOOST_AUTO_TEST_CASE(scalars)
{
    row_impl r;
    add_fields(r, nullptr, 42, 10.0f, date(2020, 10, 1));
    r.copy_strings_as_offsets(0, 4);
    r.offsets_to_string_views();
    BOOST_TEST(r.fields() == make_fv_vector(nullptr, 42, 10.f, date(2020, 10, 1)));
}

BOOST_AUTO_TEST_CASE(strings_blobs)
{
    row_impl r;
    std::string s = "abc";
    blob b{0x01, 0x02, 0x03};
    add_fields(r, nullptr, s, 10.f, b);
    r.copy_strings_as_offsets(1, 3);
    s = "ghi";
    b = {0xff, 0xff, 0xff};
    r.offsets_to_string_views();
    BOOST_TEST(r.fields() == make_fv_vector(nullptr, "abc", 10.f, makebv("\1\2\3")));
}

BOOST_AUTO_TEST_CASE(empty_strings_blobs)
{
    row_impl r;
    std::string s = "";
    blob b{};
    add_fields(r, nullptr, s, 10.f, b);
    r.copy_strings_as_offsets(1, 3);
    s = "ghi";
    b = {0xff, 0xff, 0xff};
    r.offsets_to_string_views();
    BOOST_TEST(r.fields() == make_fv_vector(nullptr, "", 10.f, makebv("")));
}

BOOST_AUTO_TEST_CASE(buffer_relocation)
{
    row_impl r;
    std::string s = "abc";
    add_fields(r, nullptr, s);
    r.copy_strings_as_offsets(0, 2);
    s = "ghi";

    blob b{0x01, 0x02, 0x03};
    add_fields(r, 10.f, b);
    r.copy_strings_as_offsets(2, 2);

    s = "";
    b = {};
    add_fields(r, s, b);
    r.copy_strings_as_offsets(4, 2);
    b = {0x01, 0x02};

    s = "this is a long string";
    add_fields(r, s);
    r.copy_strings_as_offsets(6, 1);
    s = "another long string";

    r.offsets_to_string_views();
    BOOST_TEST(
        r.fields() ==
        make_fv_vector(nullptr, "abc", 10.f, makebv("\1\2\3"), "", makebv(""), "this is a long string")
    );
}

BOOST_AUTO_TEST_CASE(empty_range)
{
    std::string s = "abc";
    row_impl r = makerwimpl(nullptr, 42);
    r.copy_strings_as_offsets(0, 0);
    r.offsets_to_string_views();
    BOOST_TEST(r.fields() == make_fv_vector(nullptr, 42));
}

BOOST_AUTO_TEST_CASE(empty_collection)
{
    row_impl r;
    r.copy_strings_as_offsets(0, 0);
    r.offsets_to_string_views();
    BOOST_TEST(r.fields().empty());
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

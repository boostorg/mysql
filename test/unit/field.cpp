//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/field.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/detail/auxiliar/stringize.hpp>
#include <boost/test/tools/context.hpp>
#include <boost/test/tools/interface.hpp>
#include <boost/test/unit_test_suite.hpp>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include "test_common.hpp"

BOOST_TEST_DONT_PRINT_LOG_VALUE(boost::mysql::date)
BOOST_TEST_DONT_PRINT_LOG_VALUE(boost::mysql::datetime)
BOOST_TEST_DONT_PRINT_LOG_VALUE(boost::mysql::time)

using namespace boost::mysql::test;
using boost::mysql::field;
using boost::mysql::field_view;
using boost::mysql::field_kind;
using boost::mysql::detail::stringize;

namespace
{

BOOST_AUTO_TEST_SUITE(test_field)

BOOST_AUTO_TEST_SUITE(constructors)

BOOST_AUTO_TEST_CASE(default_constructor)
{
    field v;
    BOOST_TEST(v.is_null());
}

BOOST_AUTO_TEST_CASE(copy_scalar)
{
    field v (42);
    field v2 (v);
    BOOST_TEST(v2.as_int64() == 42);
}

BOOST_AUTO_TEST_CASE(copy_string)
{
    std::string s ("test");
    field v (s);
    field v2 (v);
    BOOST_TEST(v2.as_string() == "test");

    // Changing the value of v doesn't affect v2
    v = "other";
    BOOST_TEST(v2.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(move)
{
    field v (field("test"));
    BOOST_TEST(v.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(from_nullptr)
{
    field v(nullptr);
    BOOST_TEST(v.is_null());
}

BOOST_AUTO_TEST_CASE(from_u8)
{
    field v(std::uint8_t(0xfe));
    BOOST_TEST(v.as_uint64() == 0xfe);
}

BOOST_AUTO_TEST_CASE(from_u16)
{
    field v(std::uint16_t(0xfefe));
    BOOST_TEST(v.as_uint64() == 0xfefe);
}

BOOST_AUTO_TEST_CASE(from_u32)
{
    field v(std::uint32_t(0xfefefefe));
    BOOST_TEST(v.as_uint64() == 0xfefefefe);
}

BOOST_AUTO_TEST_CASE(from_u64)
{
    field v(std::uint64_t(0xfefefefefefefefe));
    BOOST_TEST(v.as_uint64() == 0xfefefefefefefefe);
}

BOOST_AUTO_TEST_CASE(from_s8)
{
    field v(std::int8_t(-1));
    BOOST_TEST(v.as_int64() == -1);
}

BOOST_AUTO_TEST_CASE(from_s16)
{
    field v(std::int16_t(-1));
    BOOST_TEST(v.as_int64() == -1);
}

BOOST_AUTO_TEST_CASE(from_s32)
{
    field v(std::int32_t(-1));
    BOOST_TEST(v.as_int64() == -1);
}

BOOST_AUTO_TEST_CASE(from_s64)
{
    field v(std::int64_t(-1));
    BOOST_TEST(v.as_int64() == -1);
}

BOOST_AUTO_TEST_CASE(from_char_array)
{
    field v("test");
    BOOST_TEST(v.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(from_c_str)
{
    const char* str = "test";
    field v(str);
    BOOST_TEST(v.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(from_string_view)
{
    boost::string_view sv ("test123", 4);
    field v(sv);
    BOOST_TEST(v.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(from_string_rvalue)
{
    field v(std::string("test"));
    BOOST_TEST(v.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(from_string_lvalue)
{
    std::string s ("test");
    field v(s);
    BOOST_TEST(v.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(from_float)
{
    field v(4.2f);
    BOOST_TEST(v.as_float() == 4.2f);
}

BOOST_AUTO_TEST_CASE(from_double)
{
    field v(4.2);
    BOOST_TEST(v.as_double() == 4.2);
}

BOOST_AUTO_TEST_CASE(from_date)
{
    auto d = makedate(2022, 4, 1);
    field v (d);
    BOOST_TEST(v.as_date() == d);
}

BOOST_AUTO_TEST_CASE(from_datetime)
{
    auto d = makedt(2022, 4, 1, 21);
    field v (d);
    BOOST_TEST(v.as_datetime() == d);
}

BOOST_AUTO_TEST_CASE(from_time)
{
    auto t = maket(20, 10, 1);
    field v (t);
    BOOST_TEST(v.as_time() == t);
}

BOOST_AUTO_TEST_CASE(from_field_view_null)
{
    field_view fv;
    field f (fv);
    BOOST_TEST(f.is_null());
}

BOOST_AUTO_TEST_CASE(from_field_view_int64)
{
    field_view fv (-1);
    field f (fv);
    BOOST_TEST(f.as_int64() == -1);
}

BOOST_AUTO_TEST_CASE(from_field_view_uint64)
{
    field_view fv (42u);
    field f (fv);
    BOOST_TEST(f.as_uint64() == 42);
}

BOOST_AUTO_TEST_CASE(from_field_view_string)
{
    std::string s ("test");
    field_view fv (s);
    field f (fv);
    s = "other"; // changing the source string shouldn't modify the value
    BOOST_TEST(f.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(from_field_view_float)
{
    field_view fv (4.2f);
    field f (fv);
    BOOST_TEST(f.as_float() == 4.2f);
}

BOOST_AUTO_TEST_CASE(from_field_view_double)
{
    field_view fv (4.2);
    field f (fv);
    BOOST_TEST(f.as_double() == 4.2);
}

BOOST_AUTO_TEST_CASE(from_field_view_date)
{
    auto d = makedate(2020, 1, 2);
    field_view fv (d);
    field f (fv);
    BOOST_TEST(f.as_date() == d);
}

BOOST_AUTO_TEST_CASE(from_field_view_datetime)
{
    auto d = makedt(2020, 1, 2);
    field_view fv (d);
    field f (fv);
    BOOST_TEST(f.as_datetime() == d);
}

BOOST_AUTO_TEST_CASE(from_field_view_time)
{
    auto t = maket(9, 1, 2);
    field_view fv (t);
    field f (fv);
    BOOST_TEST(f.as_time() == t);
}

BOOST_AUTO_TEST_SUITE_END()



BOOST_AUTO_TEST_SUITE(assignment)

BOOST_AUTO_TEST_CASE(copy_scalar)
{
    field v (42);
    field v2 (5.6);
    v = v2;
    BOOST_TEST(v.as_double() == 5.6);
}

BOOST_AUTO_TEST_CASE(copy_string)
{
    field v (42);
    field v2 ("test");
    v = v2;
    BOOST_TEST(v.as_string() == "test");

    // Changing the value of v2 doesn't affect v
    v2.as_string() = "other";
    BOOST_TEST(v.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(self_copy)
{
    field v ("test");
    const auto& ref = v;
    v = ref;
    BOOST_TEST(v.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(move)
{
    field v (42);
    field v2 ("test");
    v = std::move(v2);
    BOOST_TEST(v.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(self_move)
{
    field v ("test");
    auto&& ref = v;
    v = std::move(ref);
    BOOST_TEST(v.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(from_nullptr)
{
    field v(42);
    v = nullptr;
    BOOST_TEST(v.is_null());
}

BOOST_AUTO_TEST_CASE(from_u8)
{
    field v(9.2f);
    v = std::uint8_t(0xfe);
    BOOST_TEST(v.as_uint64() == 0xfe);
}

BOOST_AUTO_TEST_CASE(from_u16)
{
    field v(9.2f);
    v = std::uint16_t(0xfefe);
    BOOST_TEST(v.as_uint64() == 0xfefe);
}

BOOST_AUTO_TEST_CASE(from_u32)
{
    field v(9.2f);
    v = std::uint32_t(0xfefefefe);
    BOOST_TEST(v.as_uint64() == 0xfefefefe);
}

BOOST_AUTO_TEST_CASE(from_u64)
{
    field v(9.2f);
    v = std::uint64_t(0xfefefefefefefefe);
    BOOST_TEST(v.as_uint64() == 0xfefefefefefefefe);
}

BOOST_AUTO_TEST_CASE(from_s8)
{
    field v (9.2f);
    v = std::int8_t(-1);
    BOOST_TEST(v.as_int64() == -1);
}

BOOST_AUTO_TEST_CASE(from_s16)
{
    field v (9.2f);
    v = std::int16_t(-1);
    BOOST_TEST(v.as_int64() == -1);
}

BOOST_AUTO_TEST_CASE(from_s32)
{
    field v (9.2f);
    v = std::int32_t(-1);
    BOOST_TEST(v.as_int64() == -1);
}

BOOST_AUTO_TEST_CASE(from_s64)
{
    field v (9.2f);
    v = std::int64_t(-1);
    BOOST_TEST(v.as_int64() == -1);
}

BOOST_AUTO_TEST_CASE(from_char_array)
{
    field v (9.2f);
    v = "test";
    BOOST_TEST(v.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(from_c_str)
{
    const char* str = "test";
    field v (9.2f);
    v = str;
    BOOST_TEST(v.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(from_string_view)
{
    boost::string_view sv ("test123", 4);
    field v (9.2f);
    v = sv;
    BOOST_TEST(v.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(from_string_rvalue)
{
    field v (9.2f);
    v = std::string("test");
    BOOST_TEST(v.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(from_string_lvalue)
{
    std::string s ("test");
    field v (9.2f);
    v = s;
    BOOST_TEST(v.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(from_float)
{
    field v ("test");
    v = 4.2f;
    BOOST_TEST(v.as_float() == 4.2f);
}

BOOST_AUTO_TEST_CASE(from_double)
{
    field v ("test");
    v = 4.2;
    BOOST_TEST(v.as_double() == 4.2);
}

BOOST_AUTO_TEST_CASE(from_date)
{
    auto d = makedate(2022, 4, 1);
    field v ("test");
    v = d;
    BOOST_TEST(v.as_date() == d);
}

BOOST_AUTO_TEST_CASE(from_datetime)
{
    auto d = makedt(2022, 4, 1, 21);
    field v ("test");
    v = d;
    BOOST_TEST(v.as_datetime() == d);
}

BOOST_AUTO_TEST_CASE(from_time)
{
    auto t = maket(20, 10, 1);
    field v ("test");
    v = t;
    BOOST_TEST(v.as_time() == t);
}

BOOST_AUTO_TEST_CASE(from_field_view_null)
{
    field_view fv;
    field f ("test");
    f = fv;
    BOOST_TEST(f.is_null());
}

BOOST_AUTO_TEST_CASE(from_field_view_int64)
{
    field_view fv (-1);
    field f ("test");
    f = fv;
    BOOST_TEST(f.as_int64() == -1);
}

BOOST_AUTO_TEST_CASE(from_field_view_uint64)
{
    field_view fv (42u);
    field f ("test");
    f = fv;
    BOOST_TEST(f.as_uint64() == 42);
}

BOOST_AUTO_TEST_CASE(from_field_view_string)
{
    field_view fv ("test");
    field f (1);
    f = fv;
    BOOST_TEST(f.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(from_field_view_float)
{
    field_view fv (4.2f);
    field f ("test");
    f = fv;
    BOOST_TEST(f.as_float() == 4.2f);
}

BOOST_AUTO_TEST_CASE(from_field_view_double)
{
    field_view fv (4.2);
    field f ("test");
    f = fv;
    BOOST_TEST(f.as_double() == 4.2);
}

BOOST_AUTO_TEST_CASE(from_field_view_date)
{
    auto d = makedate(2020, 1, 2);
    field_view fv (d);
    field f ("test");
    f = fv;
    BOOST_TEST(f.as_date() == d);
}

BOOST_AUTO_TEST_CASE(from_field_view_datetime)
{
    auto d = makedt(2020, 1, 2);
    field_view fv (d);
    field f ("test");
    f = fv;
    BOOST_TEST(f.as_datetime() == d);
}

BOOST_AUTO_TEST_CASE(from_field_view_time)
{
    auto t = maket(9, 1, 2);
    field_view fv (t);
    field f ("test");
    f = fv;
    BOOST_TEST(f.as_time() == t);
}


BOOST_AUTO_TEST_SUITE_END()


BOOST_AUTO_TEST_SUITE(accesors)

struct
{
    const char* name;
    const field f;
    field_kind expected_kind;
    bool is_null, is_int64, is_uint64, is_string, is_float, is_double, is_date, is_datetime, is_time;
} test_cases [] = {
    // name       field                             kind                  null,  i64    u64    str    float  double date   dt    time 
    { "null",     field(),                     field_kind::null,     true,  false, false, false, false, false, false, false, false },
    { "int64",    field(42),                   field_kind::int64,    false, true,  false, false, false, false, false, false, false },
    { "uint64",   field(42u),                  field_kind::uint64,   false, false, true,  false, false, false, false, false, false },
    { "string",   field("test"),               field_kind::string,   false, false, false, true,  false, false, false, false, false },
    { "float",    field(4.2f),                 field_kind::float_,   false, false, false, false, true,  false, false, false, false },
    { "double",   field(4.2),                  field_kind::double_,  false, false, false, false, false, true,  false, false, false },
    { "date",     field(makedate(2020, 1, 1)), field_kind::date,     false, false, false, false, false, false, true,  false, false },
    { "datetime", field(makedt(2020, 1, 1)),   field_kind::datetime, false, false, false, false, false, false, false, true,  false },
    { "time",     field(maket(20, 1, 1)),      field_kind::time,     false, false, false, false, false, false, false, false, true },
};

BOOST_AUTO_TEST_CASE(kind)
{
    for (const auto& tc : test_cases)
    {
        BOOST_TEST(tc.f.kind() == tc.expected_kind, tc.name);
    }
}

BOOST_AUTO_TEST_CASE(is)
{
    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            BOOST_TEST(tc.f.is_null() == tc.is_null);
            BOOST_TEST(tc.f.is_int64() == tc.is_int64);
            BOOST_TEST(tc.f.is_uint64() == tc.is_uint64);
            BOOST_TEST(tc.f.is_string() == tc.is_string);
            BOOST_TEST(tc.f.is_float() == tc.is_float);
            BOOST_TEST(tc.f.is_double() == tc.is_double);
            BOOST_TEST(tc.f.is_date() == tc.is_date);
            BOOST_TEST(tc.f.is_datetime() == tc.is_datetime);
            BOOST_TEST(tc.f.is_time() == tc.is_time);
        }
    }
}

// We check both const and non-const versions
BOOST_AUTO_TEST_CASE(as_exceptions)
{
    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            if (tc.is_int64)
            {
                BOOST_CHECK_NO_THROW(tc.f.as_int64());
                BOOST_CHECK_NO_THROW(field(tc.f).as_int64());
            }
            else
            {
                BOOST_CHECK_THROW(tc.f.as_int64(), boost::mysql::bad_field_access);
                BOOST_CHECK_THROW(field(tc.f).as_int64(), boost::mysql::bad_field_access);
            }

            if (tc.is_uint64)
            {
                BOOST_CHECK_NO_THROW(tc.f.as_uint64());
                BOOST_CHECK_NO_THROW(field(tc.f).as_uint64());
            }
            else
            {
                BOOST_CHECK_THROW(tc.f.as_uint64(), boost::mysql::bad_field_access);
                BOOST_CHECK_THROW(field(tc.f).as_uint64(), boost::mysql::bad_field_access);
            }

            if (tc.is_string)
            {
                BOOST_CHECK_NO_THROW(tc.f.as_string());
                BOOST_CHECK_NO_THROW(field(tc.f).as_string());
            }
            else
            {
                BOOST_CHECK_THROW(tc.f.as_string(), boost::mysql::bad_field_access);
                BOOST_CHECK_THROW(field(tc.f).as_string(), boost::mysql::bad_field_access);
            }

            if (tc.is_float)
            {
                BOOST_CHECK_NO_THROW(tc.f.as_float());
                BOOST_CHECK_NO_THROW(field(tc.f).as_float());
            }
            else
            {
                BOOST_CHECK_THROW(tc.f.as_float(), boost::mysql::bad_field_access);
                BOOST_CHECK_THROW(field(tc.f).as_float(), boost::mysql::bad_field_access);
            }

            if (tc.is_double)
            {
                BOOST_CHECK_NO_THROW(tc.f.as_double());
                BOOST_CHECK_NO_THROW(field(tc.f).as_double());
            }
            else
            {
                BOOST_CHECK_THROW(tc.f.as_double(), boost::mysql::bad_field_access);
                BOOST_CHECK_THROW(field(tc.f).as_double(), boost::mysql::bad_field_access);
            }

            if (tc.is_date)
            {
                BOOST_CHECK_NO_THROW(tc.f.as_date());
                BOOST_CHECK_NO_THROW(field(tc.f).as_date());
            }
            else
            {
                BOOST_CHECK_THROW(tc.f.as_date(), boost::mysql::bad_field_access);
                BOOST_CHECK_THROW(field(tc.f).as_date(), boost::mysql::bad_field_access);
            }

            if (tc.is_datetime)
            {
                BOOST_CHECK_NO_THROW(tc.f.as_datetime());
                BOOST_CHECK_NO_THROW(field(tc.f).as_datetime());
            }
            else
            {
                BOOST_CHECK_THROW(tc.f.as_datetime(), boost::mysql::bad_field_access);
                BOOST_CHECK_THROW(field(tc.f).as_datetime(), boost::mysql::bad_field_access);
            }

            if (tc.is_time)
            {
                BOOST_CHECK_NO_THROW(tc.f.as_time());
                BOOST_CHECK_NO_THROW(field(tc.f).as_time());
            }
            else
            {
                BOOST_CHECK_THROW(tc.f.as_time(), boost::mysql::bad_field_access);
                BOOST_CHECK_THROW(field(tc.f).as_time(), boost::mysql::bad_field_access);
            }
        }
    }
}

// Success cases (the type matches the called function)
BOOST_AUTO_TEST_CASE(int64)
{
    field f (-1);
    BOOST_TEST(f.as_int64() == -1);
    BOOST_TEST(f.get_int64() == -1);

    f.as_int64() = -3;
    BOOST_TEST(f.as_int64() == -3);

    f.get_int64() = -4;
    BOOST_TEST(f.as_int64() == -4);

    const field f2 (-1);
    BOOST_TEST(f2.as_int64() == -1);
    BOOST_TEST(f2.get_int64() == -1);
}

BOOST_AUTO_TEST_CASE(uint64)
{
    field f (42u);
    BOOST_TEST(f.as_uint64() == 42u);
    BOOST_TEST(f.get_uint64() == 42u);

    f.as_uint64() = 44u;
    BOOST_TEST(f.as_uint64() == 44u);

    f.get_uint64() = 45u;
    BOOST_TEST(f.as_uint64() == 45u);

    const field f2 (42u);
    BOOST_TEST(f2.as_uint64() == 42u);
    BOOST_TEST(f2.get_uint64() == 42u);
}

BOOST_AUTO_TEST_CASE(string)
{
    field f ("test");
    BOOST_TEST(f.as_string() == "test");
    BOOST_TEST(f.get_string() == "test");

    f.as_string() = "test3";
    BOOST_TEST(f.as_string() == "test3");

    f.get_string() = "test4";
    BOOST_TEST(f.as_string() == "test4");

    const field f2 ("test");
    BOOST_TEST(f2.as_string() == "test");
    BOOST_TEST(f2.get_string() == "test");
}

BOOST_AUTO_TEST_CASE(float_)
{
    field f (4.2f);
    BOOST_TEST(f.as_float() == 4.2f);
    BOOST_TEST(f.get_float() == 4.2f);

    f.as_float() = 4.4f;
    BOOST_TEST(f.as_float() == 4.4f);

    f.get_float() = 4.5f;
    BOOST_TEST(f.as_float() == 4.5f);

    const field f2 (4.2f);
    BOOST_TEST(f2.as_float() == 4.2f);
    BOOST_TEST(f2.get_float() == 4.2f);
}

BOOST_AUTO_TEST_CASE(double_)
{
    field f (4.2);
    BOOST_TEST(f.as_double() == 4.2);
    BOOST_TEST(f.get_double() == 4.2);

    f.as_double() = 4.4;
    BOOST_TEST(f.as_double() == 4.4);

    f.get_double() = 4.5;
    BOOST_TEST(f.as_double() == 4.5);

    const field f2 (4.2);
    BOOST_TEST(f2.as_double() == 4.2);
    BOOST_TEST(f2.get_double() == 4.2);
}

BOOST_AUTO_TEST_CASE(date)
{
    auto d1 = makedate(2020, 1, 1);
    auto d2 = makedate(2020, 3, 3);
    auto d3 = makedate(2020, 4, 4);

    field f (d1);
    BOOST_TEST(f.as_date() == d1);
    BOOST_TEST(f.get_date() == d1);

    f.as_date() = d2;
    BOOST_TEST(f.as_date() == d2);

    f.get_date() = d3;
    BOOST_TEST(f.as_date() == d3);

    const field f2 (d1);
    BOOST_TEST(f2.as_date() == d1);
    BOOST_TEST(f2.get_date() == d1);
}

BOOST_AUTO_TEST_CASE(datetime)
{
    auto d1 = makedt(2020, 1, 1);
    auto d2 = makedt(2020, 3, 3);
    auto d3 = makedt(2020, 4, 4);

    field f (d1);
    BOOST_TEST(f.as_datetime() == d1);
    BOOST_TEST(f.get_datetime() == d1);

    f.as_datetime() = d2;
    BOOST_TEST(f.as_datetime() == d2);

    f.get_datetime() = d3;
    BOOST_TEST(f.as_datetime() == d3);

    const field f2 (d1);
    BOOST_TEST(f2.as_datetime() == d1);
    BOOST_TEST(f2.get_datetime() == d1);
}

BOOST_AUTO_TEST_CASE(time)
{
    auto t1 = maket(8, 1, 1);
    auto t2 = maket(10, 3, 3);
    auto t3 = maket(11, 4, 4);

    field f (t1);
    BOOST_TEST(f.as_time() == t1);
    BOOST_TEST(f.get_time() == t1);

    f.as_time() = t2;
    BOOST_TEST(f.as_time() == t2);

    f.get_time() = t3;
    BOOST_TEST(f.as_time() == t3);

    const field f2 (t1);
    BOOST_TEST(f2.as_time() == t1);
    BOOST_TEST(f2.get_time() == t1);
}

BOOST_AUTO_TEST_SUITE_END()


BOOST_AUTO_TEST_SUITE(emplace)

BOOST_AUTO_TEST_CASE(null)
{
    field f ("test");
    f.emplace_null();
    BOOST_TEST(f.is_null());
}

BOOST_AUTO_TEST_CASE(int64)
{
    field f ("test");
    f.emplace_int64(-1);
    BOOST_TEST(f.as_int64() == -1);
}

BOOST_AUTO_TEST_CASE(uint64)
{
    field f ("test");
    f.emplace_uint64(42u);
    BOOST_TEST(f.as_uint64() == 42u);
}

BOOST_AUTO_TEST_CASE(string)
{
    field f;
    f.emplace_string("test");
    BOOST_TEST(f.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(float_)
{
    field f ("test");
    f.emplace_float(4.2f);
    BOOST_TEST(f.as_float() == 4.2f);
}

BOOST_AUTO_TEST_CASE(double_)
{
    field f ("test");
    f.emplace_double(4.2);
    BOOST_TEST(f.as_double() == 4.2);
}

BOOST_AUTO_TEST_CASE(date)
{
    auto d = makedate(2020, 1, 1);
    field f ("test");
    f.emplace_date(d);
    BOOST_TEST(f.as_date() == d);
}

BOOST_AUTO_TEST_CASE(datetime)
{
    auto d = makedt(2020, 1, 1);
    field f ("test");
    f.emplace_datetime(d);
    BOOST_TEST(f.as_datetime() == d);
}

BOOST_AUTO_TEST_CASE(time)
{
    auto t = maket(8, 1, 1);
    field f ("test");
    f.emplace_time(t);
    BOOST_TEST(f.as_time() == t);
}

BOOST_AUTO_TEST_SUITE_END()


// operator== relies on field_view's operator==, so only
// a small subset of tests here
BOOST_AUTO_TEST_SUITE(operator_equals)

BOOST_AUTO_TEST_CASE(field_field)
{
    BOOST_TEST(field(42) == field(42));
    BOOST_TEST(!(field(42) != field(42)));
    
    BOOST_TEST(!(field(42) == field("test")));
    BOOST_TEST(field(42) != field("test"));
}

BOOST_AUTO_TEST_CASE(fieldview_field)
{
    BOOST_TEST(field_view(42) == field(42));
    BOOST_TEST(!(field_view(42) != field(42)));
    
    BOOST_TEST(!(field_view(42) == field("test")));
    BOOST_TEST(field_view(42) != field("test"));
}

BOOST_AUTO_TEST_CASE(field_fieldview)
{
    BOOST_TEST(field(42) == field_view(42));
    BOOST_TEST(!(field(42) != field_view(42)));
    
    BOOST_TEST(!(field(42) == field_view("test")));
    BOOST_TEST(field(42) != field_view("test"));
}

BOOST_AUTO_TEST_SUITE_END()

// operator<< relies on field_view's operator<<, so only
// a small subset of tests here
BOOST_AUTO_TEST_CASE(operator_stream)
{
    BOOST_TEST(stringize(field()) == "<NULL>");
    BOOST_TEST(stringize(field(-1)) == "-1");
    BOOST_TEST(stringize(field(42)) == "42");
}

BOOST_AUTO_TEST_SUITE_END()

}
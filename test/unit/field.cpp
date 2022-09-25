//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/field.hpp>
#include <boost/test/tools/context.hpp>
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
using boost::mysql::field_kind;

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

BOOST_AUTO_TEST_CASE(move)
{
    field v (42);
    field v2 ("test");
    v = std::move(v2);
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
BOOST_AUTO_TEST_CASE(if_pointer_validity)
{
    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            if (tc.is_int64)
            {
                BOOST_TEST(tc.f.if_int64() != nullptr);
                BOOST_TEST(field(tc.f).if_int64() != nullptr);
            }
            else
            {
                BOOST_TEST(tc.f.if_int64() == nullptr);
                BOOST_TEST(field(tc.f).if_int64() == nullptr);
            }

            if (tc.is_uint64)
            {
                BOOST_TEST(tc.f.if_uint64() != nullptr);
                BOOST_TEST(field(tc.f).if_uint64() != nullptr);
            }
            else
            {
                BOOST_TEST(tc.f.if_uint64() == nullptr);
                BOOST_TEST(field(tc.f).if_uint64() == nullptr);
            }

            if (tc.is_string)
            {
                BOOST_TEST(tc.f.if_string() != nullptr);
                BOOST_TEST(field(tc.f).if_string() != nullptr);
            }
            else
            {
                BOOST_TEST(tc.f.if_string() == nullptr);
                BOOST_TEST(field(tc.f).if_string() == nullptr);
            }

            if (tc.is_float)
            {
                BOOST_TEST(tc.f.if_float() != nullptr);
                BOOST_TEST(field(tc.f).if_float() != nullptr);
            }
            else
            {
                BOOST_TEST(tc.f.if_float() == nullptr);
                BOOST_TEST(field(tc.f).if_float() == nullptr);
            }

            if (tc.is_double)
            {
                BOOST_TEST(tc.f.if_double() != nullptr);
                BOOST_TEST(field(tc.f).if_double() != nullptr);
            }
            else
            {
                BOOST_TEST(tc.f.if_double() == nullptr);
                BOOST_TEST(field(tc.f).if_double() == nullptr);
            }

            if (tc.is_date)
            {
                BOOST_TEST(tc.f.if_date() != nullptr);
                BOOST_TEST(field(tc.f).if_date() != nullptr);
            }
            else
            {
                BOOST_TEST(tc.f.if_date() == nullptr);
                BOOST_TEST(field(tc.f).if_date() == nullptr);
            }

            if (tc.is_datetime)
            {
                BOOST_TEST(tc.f.if_datetime() != nullptr);
                BOOST_TEST(field(tc.f).if_datetime() != nullptr);
            }
            else
            {
                BOOST_TEST(tc.f.if_datetime() == nullptr);
                BOOST_TEST(field(tc.f).if_datetime() == nullptr);
            }

            if (tc.is_time)
            {
                BOOST_TEST(tc.f.if_time() != nullptr);
                BOOST_TEST(field(tc.f).if_time() != nullptr);
            }
            else
            {
                BOOST_TEST(tc.f.if_time() == nullptr);
                BOOST_TEST(field(tc.f).if_time() == nullptr);
            }
        }
    }
}

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

// // Success cases (the type matches the called function)
// BOOST_AUTO_TEST_CASE(int64)
// {
//     field_view f (-1);
//     BOOST_TEST_REQUIRE(f.if_int64() != nullptr);
//     BOOST_TEST(*f.if_int64() == -1);
//     BOOST_TEST(f.as_int64() == -1);
//     BOOST_TEST(f.get_int64() == -1);
// }

// BOOST_AUTO_TEST_CASE(uint64)
// {
//     field_view f (42u);
//     BOOST_TEST_REQUIRE(f.if_uint64() != nullptr);
//     BOOST_TEST(*f.if_uint64() == 42u);
//     BOOST_TEST(f.as_uint64() == 42u);
//     BOOST_TEST(f.get_uint64() == 42u);
// }

// BOOST_AUTO_TEST_CASE(string)
// {
//     field_view f ("test");
//     BOOST_TEST_REQUIRE(f.if_string() != nullptr);
//     BOOST_TEST(*f.if_string() == "test");
//     BOOST_TEST(f.as_string() == "test");
//     BOOST_TEST(f.get_string() == "test");
// }

// BOOST_AUTO_TEST_CASE(float_)
// {
//     field_view f (4.2f);
//     BOOST_TEST_REQUIRE(f.if_float() != nullptr);
//     BOOST_TEST(*f.if_float() == 4.2f);
//     BOOST_TEST(f.as_float() == 4.2f);
//     BOOST_TEST(f.get_float() == 4.2f);
// }

// BOOST_AUTO_TEST_CASE(double_)
// {
//     field_view f (4.2);
//     BOOST_TEST_REQUIRE(f.if_double() != nullptr);
//     BOOST_TEST(*f.if_double() == 4.2);
//     BOOST_TEST(f.as_double() == 4.2);
//     BOOST_TEST(f.get_double() == 4.2);
// }

// BOOST_AUTO_TEST_CASE(date)
// {
//     auto d = makedate(2020, 1, 2);
//     field_view f (d);
//     BOOST_TEST_REQUIRE(f.if_date() != nullptr);
//     BOOST_TEST(*f.if_date() == d);
//     BOOST_TEST(f.as_date() == d);
//     BOOST_TEST(f.get_date() == d);
// }

// BOOST_AUTO_TEST_CASE(datetime)
// {
//     auto dt = makedt(2020, 1, 2);
//     field_view f (dt);
//     BOOST_TEST_REQUIRE(f.if_datetime() != nullptr);
//     BOOST_TEST(*f.if_datetime() == dt);
//     BOOST_TEST(f.as_datetime() == dt);
//     BOOST_TEST(f.get_datetime() == dt);
// }

// BOOST_AUTO_TEST_CASE(time)
// {
//     auto t = maket(2020, 1, 2);
//     field_view f (t);
//     BOOST_TEST_REQUIRE(f.if_time() != nullptr);
//     BOOST_TEST(*f.if_time() == t);
//     BOOST_TEST(f.as_time() == t);
//     BOOST_TEST(f.get_time() == t);
// }

BOOST_AUTO_TEST_SUITE_END()


// BOOST_AUTO_TEST_CASE(operator_equals)
// {
//     struct
//     {
//         const char* name;
//         field_view f1;
//         field_view f2;
//         bool is_equal;
//     } test_cases [] = {
//         { "null_null", field_view(), field_view(), true, },
//         { "null_int64", field_view(), field_view(-1), false },
//         { "null_uint64", field_view(), field_view(42), false },
//         { "null_string", field_view(), field_view("<NULL>"), false },
//         { "null_float", field_view(), field_view(4.2f), false },
//         { "null_double", field_view(), field_view(4.3), false },
//         { "null_date", field_view(), field_view(makedate(2020, 1, 2)), false },
//         { "null_datetime", field_view(), field_view(makedt(2020, 1, 1)), false },
//         { "null_time", field_view(), field_view(maket(23, 1, 1)), false },

//         { "int64_int64_same", field_view(42), field_view(42), true },
//         { "int64_int64_different", field_view(42), field_view(-1), false },
//         { "int64_uint64_same", field_view(42), field_view(42u), true },
//         { "int64_uint64_different", field_view(42), field_view(43u), false },
//         { "int64_uint64_zero", field_view(0), field_view(0u), true },
//         { "int64_uint64_lt0", field_view(-1), field_view(42u), false },
//         { "int64_uint64_gtmax", field_view(42), field_view(0xffffffffffffffffu), false },
//         { "int64_uint64_lt0gtmax", field_view(-1), field_view(0xffffffffffffffffu), false },
//         { "int64_string", field_view(42), field_view("42"), false },
//         { "int64_float", field_view(42), field_view(42.0f), false },
//         { "int64_double", field_view(42), field_view(42.0), false },
//         { "int64_date", field_view(42), field_view(makedate(2020, 1, 1)), false },
//         { "int64_datetime", field_view(42), field_view(makedt(2020, 1, 1)), false },
//         { "int64_time", field_view(42), field_view(maket(20, 1, 1)), false },

//         { "uint64_uint64_same", field_view(0xffffffffffffffffu), field_view(0xffffffffffffffffu), true },
//         { "uint64_uint64_different", field_view(42u), field_view(31u), false },
//         { "uint64_string", field_view(42u), field_view("42"), false },
//         { "uint64_float", field_view(42u), field_view(42.0f), false },
//         { "uint64_double", field_view(42u), field_view(42.0), false },
//         { "uint64_date", field_view(42u), field_view(makedate(2020, 1, 1)), false },
//         { "uint64_datetime", field_view(42u), field_view(makedt(2020, 1, 1)), false },
//         { "uint64_time", field_view(42u), field_view(maket(20, 1, 1)), false },

//         { "string_string_same", field_view("test"), field_view("test"), true },
//         { "string_string_different", field_view("test"), field_view("test2"), false },
//         { "string_float", field_view("4.2"), field_view(4.2f), false },
//         { "string_double", field_view("4.2"), field_view(4.2), false },
//         { "string_date", field_view("2020-01-01"), field_view(makedate(2020, 1, 1)), false },
//         { "string_datetime", field_view("test"), field_view(makedt(2020, 1, 1)), false },
//         { "string_time", field_view("test"), field_view(maket(8, 1, 1)), false },

//         { "float_float_same", field_view(4.2f), field_view(4.2f), true },
//         { "float_float_different", field_view(4.2f), field_view(0.0f), false },
//         { "float_double", field_view(4.2f), field_view(4.2), false },
//         { "float_date", field_view(4.2f), field_view(makedate(2020, 1, 2)), false },
//         { "float_datetime", field_view(4.2f), field_view(makedt(2020, 1, 2)), false },
//         { "float_time", field_view(4.2f), field_view(maket(20, 1, 2)), false },

//         { "double_double_same", field_view(4.2), field_view(4.2), true },
//         { "double_double_different", field_view(4.2), field_view(-1.0), false },
//         { "double_date", field_view(4.2), field_view(makedate(2020, 1, 1)), false },
//         { "double_datetime", field_view(4.2), field_view(makedt(2020, 1, 1)), false },
//         { "double_time", field_view(4.2), field_view(maket(9, 1, 1)), false },

//         { "date_date_same", field_view(makedate(2020, 1, 1)), field_view(makedate(2020, 1, 1)), true },
//         { "date_date_different", field_view(makedate(2020, 1, 1)), field_view(makedate(2019, 1, 1)), false },
//         { "date_datetime", field_view(makedate(2020, 1, 1)), field_view(makedt(2020, 1, 1)), false },
//         { "date_time", field_view(makedate(2020, 1, 1)), field_view(maket(9, 1, 1)), false },

//         { "datetime_datetime_same", field_view(makedt(2020, 1, 1, 10)), field_view(makedt(2020, 1, 1, 10)), true },
//         { "datetime_datetime_different", field_view(makedt(2020, 1, 1, 10)), field_view(makedt(2020, 1, 1, 9)), false },
//         { "datetime_time", field_view(makedt(2020, 1, 1)), field_view(maket(20, 1, 1)), false },

//         { "time_time_same", field_view(maket(20, 1, 1)), field_view(maket(20, 1, 1)), true },
//         { "time_time_different", field_view(maket(20, 1, 1)), field_view(maket(20, 1, 1, 10)), false },
//     };

//     for (const auto& tc : test_cases)
//     {
//         BOOST_TEST_CONTEXT(tc.name)
//         {
//             if (tc.is_equal)
//             {
//                 BOOST_TEST(tc.f1 == tc.f2);
//                 BOOST_TEST(tc.f2 == tc.f1);

//                 BOOST_TEST(!(tc.f1 != tc.f2));
//                 BOOST_TEST(!(tc.f2 != tc.f1));
//             }
//             else
//             {
//                 BOOST_TEST(!(tc.f1 == tc.f2));
//                 BOOST_TEST(!(tc.f2 == tc.f1));

//                 BOOST_TEST(tc.f1 != tc.f2);
//                 BOOST_TEST(tc.f2 != tc.f1);
//             }
//         }
//     }
// }


// // operator<<
// struct stream_sample
// {
//     std::string name;
//     field_view input;
//     std::string expected;

//     template <class T>
//     stream_sample(std::string&& name, T input, std::string&& expected) :
//         name(std::move(name)),
//         input(input),
//         expected(std::move(expected))
//     {
//     }
// };

// // Helper struct to define stream operations for date, datetime and time
// // We will list the possibilities for each component (hours, minutes, days...) and will
// // take the Cartessian product of all them
// struct component_value
// {
//     const char* name;
//     int v;
//     const char* repr;
// };

// void add_date_samples(std::vector<stream_sample>& output)
// {
//     constexpr component_value year_values [] = {
//         { "min", 0, "0000" },
//         { "onedig",  1, "0001" },
//         { "twodig", 98, "0098" },
//         { "threedig", 789, "0789" },
//         { "regular", 1999, "1999" },
//         { "max", 9999, "9999" }
//     };

//     constexpr component_value month_values [] = {
//         { "min", 1, "01" },
//         { "max", 12, "12" }
//     };

//     constexpr component_value day_values [] = {
//         { "min", 1, "01" },
//         { "max", 31, "31" }
//     };

//     for (const auto& year : year_values)
//     {
//         for (const auto& month : month_values)
//         {
//             for (const auto& day : day_values)
//             {
//                 std::string name = stringize(
//                     "date_year", year.name,
//                     "_month", month.name,
//                     "_day", day.name
//                 );
//                 std::string str_val = stringize(year.repr, '-', month.repr, '-', day.repr);
//                 field_view val (makedate(year.v, static_cast<unsigned>(month.v), static_cast<unsigned>(day.v)));
//                 output.emplace_back(std::move(name), val, std::move(str_val));
//             }
//         }
//     }
// }

// void add_datetime_samples(std::vector<stream_sample>& output)
// {
//     constexpr component_value year_values [] = {
//         { "min", 0, "0000" },
//         { "onedig",  1, "0001" },
//         { "twodig", 98, "0098" },
//         { "threedig", 789, "0789" },
//         { "regular", 1999, "1999" },
//         { "max", 9999, "9999" }
//     };

//     constexpr component_value month_values [] = {
//         { "min", 1, "01" },
//         { "max", 12, "12" }
//     };

//     constexpr component_value day_values [] = {
//         { "min", 1, "01" },
//         { "max", 31, "31" }
//     };

//     constexpr component_value hours_values [] = {
//         { "zero", 0, "00" },
//         { "onedigit", 5, "05" },
//         { "max", 23, "23" }
//     };

//     constexpr component_value mins_secs_values [] = {
//         { "zero", 0, "00" },
//         { "onedigit", 5, "05" },
//         { "twodigits", 59, "59" }
//     };

//     constexpr component_value micros_values [] = {
//         { "zero", 0, "000000" },
//         { "onedigit", 5, "000005" },
//         { "twodigits", 50, "000050" },
//         { "max", 999999, "999999" },
//     };

//     for (const auto& year : year_values)
//     {
//         for (const auto& month : month_values)
//         {
//             for (const auto& day : day_values)
//             {
//                 for (const auto& hours: hours_values)
//                 {
//                     for (const auto& mins: mins_secs_values)
//                     {
//                         for (const auto& secs: mins_secs_values)
//                         {
//                             for (const auto& micros: micros_values)
//                             {
//                                 std::string name = stringize(
//                                     "datetime_year", year.name,
//                                     "_month", month.name,
//                                     "_day", day.name,
//                                     "_h", hours.name,
//                                     "_m", mins.name,
//                                     "_s", secs.name,
//                                     "_u", micros.name
//                                 );
//                                 std::string str_val = stringize(
//                                     year.repr, '-',
//                                     month.repr, '-',
//                                     day.repr, ' ',
//                                     hours.repr, ':',
//                                     mins.repr, ':',
//                                     secs.repr, '.',
//                                     micros.repr
//                                 );
//                                 auto val = makedt(
//                                     year.v,
//                                     static_cast<unsigned>(month.v),
//                                     static_cast<unsigned>(day.v),
//                                     hours.v,
//                                     mins.v,
//                                     secs.v,
//                                     micros.v
//                                 );
//                                 output.emplace_back(std::move(name), field_view(val), std::move(str_val));
//                             }
//                         }
//                     }
//                 }
//             }
//         }
//     }
// }

// void add_time_samples(std::vector<stream_sample>& output)
// {
//     constexpr component_value sign_values [] = {
//         { "positive", 1, "" },
//         { "negative", -1, "-" }
//     };

//     constexpr component_value hours_values [] = {
//         { "zero", 0, "00" },
//         { "onedigit", 5, "05" },
//         { "twodigits", 23, "23" },
//         { "max", 838, "838" }
//     };

//     constexpr component_value mins_secs_values [] = {
//         { "zero", 0, "00" },
//         { "onedigit", 5, "05" },
//         { "twodigits", 59, "59" }
//     };

//     constexpr component_value micros_values [] = {
//         { "zero", 0, "000000" },
//         { "onedigit", 5, "000005" },
//         { "twodigits", 50, "000050" },
//         { "max", 999999, "999999" },
//     };

//     for (const auto& sign: sign_values)
//     {
//         for (const auto& hours: hours_values)
//         {
//             for (const auto& mins: mins_secs_values)
//             {
//                 for (const auto& secs: mins_secs_values)
//                 {
//                     for (const auto& micros: micros_values)
//                     {
//                         std::string name = stringize(
//                             "time_", sign.name,
//                             "_h", hours.name,
//                             "_m", mins.name,
//                             "_s", secs.name,
//                             "_u", micros.name
//                         );
//                         std::string str_val = stringize(
//                             sign.repr,
//                             hours.repr, ':',
//                             mins.repr, ':',
//                             secs.repr, '.',
//                             micros.repr
//                         );
//                         auto val = sign.v * maket(hours.v,
//                                 mins.v, secs.v, micros.v);
//                         if (sign.v == -1 && val == maket(0, 0, 0))
//                             continue; // This case makes no sense, as negative zero is represented as zero
//                         output.emplace_back(std::move(name), field_view(val), std::move(str_val));
//                     }
//                 }
//             }
//         }
//     }
// }

// std::vector<stream_sample> make_stream_samples()
// {
//     std::vector<stream_sample> res {
//         { "null", nullptr, "<NULL>" },
//         { "i64_positive", std::int64_t(42), "42" },
//         { "i64_negative", std::int64_t(-90), "-90" },
//         { "i64_zero", std::int64_t(0), "0" },
//         { "u64_positive", std::uint64_t(42), "42" },
//         { "u64_zero", std::uint64_t(0), "0" },
//         { "string_view", "a_string", "a_string" },
//         { "float", 2.43f, "2.43" },
//         { "double", 8.12, "8.12" },
//     };
//     add_date_samples(res);
//     add_datetime_samples(res);
//     add_time_samples(res);
//     return res;
// }

// BOOST_AUTO_TEST_CASE(operator_stream)
// {
//     auto test_cases = make_stream_samples();
//     for (const auto& tc : test_cases)
//     {
//         BOOST_TEST_CONTEXT(tc.name)
//         {
//             std::ostringstream ss;
//             ss << tc.input;
//             BOOST_TEST(ss.str() == tc.expected);
//         }
//     }
// }

BOOST_AUTO_TEST_SUITE_END()

}
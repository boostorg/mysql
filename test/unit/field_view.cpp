//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/field_view.hpp>
#include <boost/test/unit_test_suite.hpp>
#include <boost/type_index.hpp>
#include <boost/test/data/monomorphic/collection.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/utility/string_view_fwd.hpp>
#include <cstdint>
#include <set>
#include <sstream>
#include <map>
#include <tuple>
#include "test_common.hpp"

BOOST_TEST_DONT_PRINT_LOG_VALUE(boost::mysql::date)
BOOST_TEST_DONT_PRINT_LOG_VALUE(boost::mysql::datetime)
BOOST_TEST_DONT_PRINT_LOG_VALUE(boost::mysql::time)

using namespace boost::mysql::test;
using boost::mysql::field_view;
using boost::mysql::field_kind;
using boost::typeindex::type_index;

BOOST_AUTO_TEST_SUITE(test_field_value)

BOOST_AUTO_TEST_SUITE(constructors)

BOOST_AUTO_TEST_CASE(default_constructor)
{
    field_view v;
    BOOST_TEST(v.is_null());
}

BOOST_AUTO_TEST_CASE(copy)
{
    field_view v (32);
    field_view v2 (v);
    BOOST_TEST(v2.as_int64() == 32);
}

BOOST_AUTO_TEST_CASE(move)
{
    field_view v (field_view(32));
    BOOST_TEST(v.as_int64() == 32);
}

BOOST_AUTO_TEST_CASE(from_nullptr)
{
    field_view v(nullptr);
    BOOST_TEST(v.is_null());
}

BOOST_AUTO_TEST_CASE(from_u8)
{
    field_view v(std::uint8_t(0xfe));
    BOOST_TEST(v.as_uint64() == 0xfe);
}

BOOST_AUTO_TEST_CASE(from_u16)
{
    field_view v(std::uint16_t(0xfefe));
    BOOST_TEST(v.as_uint64() == 0xfefe);
}

BOOST_AUTO_TEST_CASE(from_u32)
{
    field_view v(std::uint32_t(0xfefefefe));
    BOOST_TEST(v.as_uint64() == 0xfefefefe);
}

BOOST_AUTO_TEST_CASE(from_u64)
{
    field_view v(std::uint64_t(0xfefefefefefefefe));
    BOOST_TEST(v.as_uint64() == 0xfefefefefefefefe);
}

BOOST_AUTO_TEST_CASE(from_s8)
{
    field_view v(std::int8_t(-1));
    BOOST_TEST(v.as_int64() == -1);
}

BOOST_AUTO_TEST_CASE(from_s16)
{
    field_view v(std::int16_t(-1));
    BOOST_TEST(v.as_int64() == -1);
}

BOOST_AUTO_TEST_CASE(from_s32)
{
    field_view v(std::int32_t(-1));
    BOOST_TEST(v.as_int64() == -1);
}

BOOST_AUTO_TEST_CASE(from_s64)
{
    field_view v(std::int64_t(-1));
    BOOST_TEST(v.as_int64() == -1);
}

BOOST_AUTO_TEST_CASE(from_char_array)
{
    field_view v("test");
    BOOST_TEST(v.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(from_c_str)
{
    const char* str = "test";
    field_view v(str);
    BOOST_TEST(v.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(from_string_view)
{
    boost::string_view sv ("test123", 4);
    field_view v(sv);
    BOOST_TEST(v.as_string() == "test");
}

BOOST_AUTO_TEST_CASE(from_float)
{
    field_view v(4.2f);
    BOOST_TEST(v.as_float() == 4.2f);
}

BOOST_AUTO_TEST_CASE(from_double)
{
    field_view v(4.2);
    BOOST_TEST(v.as_double() == 4.2);
}

BOOST_AUTO_TEST_CASE(from_date)
{
    auto d = makedate(2022, 4, 1);
    field_view v (d);
    BOOST_TEST(v.as_date() == d);
}

BOOST_AUTO_TEST_CASE(from_datetime)
{
    auto d = makedt(2022, 4, 1, 21);
    field_view v (d);
    BOOST_TEST(v.as_datetime() == d);
}

BOOST_AUTO_TEST_CASE(from_time)
{
    auto t = maket(20, 10, 1);
    field_view v (t);
    BOOST_TEST(v.as_time() == t);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(kind)
{
    struct
    {
        const char* name;
        field_view field;
        field_kind expected;
    } test_cases [] = {
        { "null",     field_view(nullptr), field_kind::null },
        { "int64",    field_view(32), field_kind::int64 },
        { "uint64",   field_view(42u), field_kind::uint64 },
        { "string",   field_view("test"), field_kind::string },
        { "float",    field_view(3.1f), field_kind::float_ },
        { "double",   field_view(3.1), field_kind::double_ },
        { "date",     field_view(makedate(2020, 2, 1)), field_kind::date },
        { "datetime", field_view(makedt(2020, 2, 1)), field_kind::datetime },
        { "time",     field_view(maket(20, 1, 2)), field_kind::time },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST(tc.field.kind() == tc.expected, tc.name);
    }
}


// BOOST_AUTO_TEST_CASE(copy_assignment_from_non_const_lvalue)
// {
//     field_view v (10);
//     field_view v2;
//     v2 = v;
//     BOOST_TEST(v2.to_variant() == vt(std::int64_t(10)));
// }

// BOOST_AUTO_TEST_CASE(copy_assignament_from_const_lvalue)
// {
//     const field_view v (10);
//     field_view v2;
//     v2 = v;
//     BOOST_TEST(v2.to_variant() == vt(std::int64_t(10)));
// }

// BOOST_AUTO_TEST_CASE(move_assignment)
// {
//     field_view v (10);
//     field_view v2;
//     v2 = std::move(v);
//     BOOST_TEST(v2.to_variant() == vt(std::int64_t(10)));
// }

// // accessors: is, is_convertible_to, is_null, get, get_optional
// using all_types = std::tuple<
//     boost::mysql::null_t,
//     std::uint64_t,
//     std::int64_t,
//     boost::string_view,
//     float,
//     double,
//     boost::mysql::date,
//     boost::mysql::datetime,
//     boost::mysql::time
// >;

// struct accessors_sample
// {
//     const char* name;
//     field_view v;
//     type_index is_type; // the type for which is() should return true
//     std::map<type_index, vt> conversions;
//     bool is_null;

//     accessors_sample(const char* name, field_view v, type_index is, std::map<type_index, vt>&& convs, bool is_null = false) :
//         name(name),
//         v(v),
//         is_type(is),
//         conversions(std::move(convs)),
//         is_null(is_null)
//     {
//     }
// };

// std::ostream& operator<<(std::ostream& os, const accessors_sample& input)
// {
//     return os << input.name;
// }

// template <class... Types>
// std::map<type_index, vt> make_conversions(const Types&... types)
// {
//     return { { type_id<Types>(), types }... };
// }

// template <class T>
// accessors_sample make_default_accessors_sample(
//     const char* name,
//     const T& v,
//     bool is_null = false
// )
// {
//     return accessors_sample(name, field_view(v), type_id<T>(), make_conversions(v), is_null);
// }

// const accessors_sample all_accessors_samples [] {
//     make_default_accessors_sample("null", boost::mysql::null_t(), true),
//     accessors_sample("i64_positive", field_view(std::int64_t(42)), type_id<std::int64_t>(),
//             make_conversions(std::int64_t(42), std::uint64_t(42))),
//     accessors_sample("i64_negative", field_view(std::int64_t(-42)), type_id<std::int64_t>(),
//             make_conversions(std::int64_t(-42))),
//     accessors_sample("i64_zero", field_view(std::int64_t(0)), type_id<std::int64_t>(),
//             make_conversions(std::int64_t(0), std::uint64_t(0))),
//     accessors_sample("u64_small", field_view(std::uint64_t(42)), type_id<std::uint64_t>(),
//             make_conversions(std::int64_t(42), std::uint64_t(42))),
//     accessors_sample("u64_big", field_view(std::uint64_t(0xfffffffffffffffe)), type_id<std::uint64_t>(),
//             make_conversions(std::uint64_t(0xfffffffffffffffe))),
//     accessors_sample("u64_zero", field_view(std::uint64_t(0)), type_id<std::uint64_t>(),
//             make_conversions(std::int64_t(0), std::uint64_t(0))),
//             make_default_accessors_sample("string_view", makesv("test")),
//     accessors_sample("float", field_view(4.2f), type_id<float>(),
//             make_conversions(4.2f, double(4.2f))),
//     make_default_accessors_sample("double", 4.2),
//     make_default_accessors_sample("date", makedate(2020, 10, 5)),
//     make_default_accessors_sample("datetime", makedt(2020, 10, 5, 10, 20, 30)),
//     make_default_accessors_sample("time", maket(10, 20, 30))
// };

// // Template + data tests not supported. We use template
// // test cases and use this function to emulate data test cases
// template <class Callable>
// void for_each_accessors_sample(Callable&& cb)
// {
//     for (const auto& sample: all_accessors_samples)
//     {
//         BOOST_TEST_CHECKPOINT(sample);
//         cb(sample);
//     }
// }

// BOOST_DATA_TEST_CASE(is_null, data::make(all_accessors_samples))
// {
//     BOOST_TEST(sample.v.is_null() == sample.is_null);
// }

// BOOST_AUTO_TEST_CASE_TEMPLATE(is, T, all_types)
// {
//     for_each_accessors_sample([](const accessors_sample& sample) {
//         bool expected = sample.is_type == type_id<T>();
//         BOOST_TEST(sample.v.is<T>() == expected, sample);
//     });
// }

// BOOST_AUTO_TEST_CASE_TEMPLATE(is_convertible_to, T, all_types)
// {
//     for_each_accessors_sample([](const accessors_sample& sample) {
//         auto it = sample.conversions.find(type_id<T>());
//         bool expected = it != sample.conversions.end();
//         BOOST_TEST(sample.v.is_convertible_to<T>() == expected, sample);
//     });
// }

// BOOST_AUTO_TEST_CASE_TEMPLATE(get, T, all_types)
// {
//     for_each_accessors_sample([](const accessors_sample& sample) {
//         auto it = sample.conversions.find(type_id<T>());
//         if (it != sample.conversions.end())
//         {
//             T expected = boost::variant2::get<T>(it->second);
//             BOOST_TEST(sample.v.get<T>() == expected, sample);
//         }
//         else
//         {
//             BOOST_CHECK_THROW(sample.v.get<T>(), boost::mysql::bad_field_access);
//         }
//     });
// }

// BOOST_AUTO_TEST_CASE_TEMPLATE(get_optional, T, all_types)
// {
//     for_each_accessors_sample([](const accessors_sample& sample) {
//         auto it = sample.conversions.find(type_id<T>());
//         if (it != sample.conversions.end())
//         {
//             auto expected = boost::variant2::get<T>(it->second);
//             auto opt = sample.v.get_optional<T>();
//             BOOST_TEST_REQUIRE(opt.has_value(), sample);
//             BOOST_TEST(*opt == expected, sample);
//         }
//         else
//         {
//             BOOST_TEST(!sample.v.get_optional<T>().has_value(), sample);
//         }
//     });
// }

// #ifndef BOOST_NO_CXX17_HDR_OPTIONAL

// BOOST_AUTO_TEST_CASE_TEMPLATE(get_std_optional, T, all_types)
// {
//     for_each_accessors_sample([](const accessors_sample& sample) {
//         auto it = sample.conversions.find(type_id<T>());
//         if (it != sample.conversions.end())
//         {
//             auto expected = boost::variant2::get<T>(it->second);
//             auto opt = sample.v.get_std_optional<T>();
//             BOOST_TEST_REQUIRE(opt.has_value(), sample);
//             BOOST_TEST(*opt == expected, sample);
//         }
//         else
//         {
//             BOOST_TEST(!sample.v.get_std_optional<T>().has_value(), sample);
//         }
//     });
// }

// #endif // BOOST_NO_CXX17_HDR_OPTIONAL


// // operator== and operator!=
// struct value_equality_fixture
// {
//     std::vector<field_view> values = make_value_vector(
//         std::int64_t(-1),
//         std::uint64_t(0x100000000),
//         "string",
//         3.14f,
//         8.89,
//         makedate(2019, 10, 1),
//         makedt(2019, 10, 1, 10),
//         maket(0, 0, -10),
//         boost::mysql::null_t()
//     );
// };

// BOOST_FIXTURE_TEST_CASE(operators_eq_ne_different_type, value_equality_fixture)
// {
//     for (std::size_t i = 0; i < values.size(); ++i)
//     {
//         for (std::size_t j = 0; j < i; ++j)
//         {
//             BOOST_TEST(!(values.at(i) == values.at(j)));
//             BOOST_TEST(values.at(i) != values.at(j));
//         }
//     }
// }

// BOOST_FIXTURE_TEST_CASE(operators_eq_ne_same_type_different_value, value_equality_fixture)
// {
//     auto other_values = make_value_vector(
//         std::int64_t(-22),
//         std::uint64_t(222),
//         "other_string",
//         -3.0f,
//         8e24,
//         makedate(2019, 9, 1),
//         makedt(2019, 9, 1, 10),
//         maket(0, 0, 10),
//         boost::mysql::null_t()
//     );

//     // Note: null_t (the last value) can't have other value than null_t()
//     // so it is excluded from this test
//     for (std::size_t i = 0; i < values.size() - 1; ++i)
//     {
//         BOOST_TEST(!(values.at(i) == other_values.at(i)));
//         BOOST_TEST(values.at(i) != other_values.at(i));
//     }
// }

// BOOST_FIXTURE_TEST_CASE(operators_eq_ne_same_type_same_value, value_equality_fixture)
// {
//     std::vector<field_view> values_copy = values;
//     for (std::size_t i = 0; i < values.size(); ++i)
//     {
//         BOOST_TEST(values.at(i) == values_copy.at(i));
//         BOOST_TEST(!(values.at(i) != values_copy.at(i)));
//     }
// }

// BOOST_FIXTURE_TEST_CASE(operators_eq_ne_self_comparison, value_equality_fixture)
// {
//     for (std::size_t i = 0; i < values.size(); ++i)
//     {
//         BOOST_TEST(values.at(i) == values.at(i));
//         BOOST_TEST(!(values.at(i) != values.at(i)));
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

// std::ostream& operator<<(std::ostream& os, const stream_sample& input)
// {
//     return os << input.name;
// }

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
//         { "null", boost::mysql::null_t(), "<NULL>" },
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

// BOOST_DATA_TEST_CASE(operator_stream, data::make(make_stream_samples()))
// {
//     std::ostringstream ss;
//     ss << sample.input;
//     BOOST_TEST(ss.str() == sample.expected);
// }

// // Operators <, <=, >, >= (variant behavior)
// BOOST_AUTO_TEST_CASE(operator_lt)
// {
//     BOOST_TEST(field_view(100) < field_view(200)); // same type
//     BOOST_TEST(field_view(-2) < field_view("hola")); // different type
//     BOOST_TEST(!(field_view(200) < field_view(200))); // same type
//     BOOST_TEST(!(field_view("hola") < field_view(2))); // different type
// }

// BOOST_AUTO_TEST_CASE(operator_lte)
// {
//     BOOST_TEST(field_view(200) <= field_view(200)); // same type
//     BOOST_TEST(field_view(-2) <= field_view("hola")); // different type
//     BOOST_TEST(!(field_view(300) <= field_view(200))); // same type
//     BOOST_TEST(!(field_view("hola") <= field_view(2))); // different type
// }

// BOOST_AUTO_TEST_CASE(operator_gt)
// {
//     BOOST_TEST(field_view(200) > field_view(100)); // same type
//     BOOST_TEST(field_view("hola") > field_view(2)); // different type
//     BOOST_TEST(!(field_view(200) > field_view(200))); // same type
//     BOOST_TEST(!(field_view(-2) > field_view("hola"))); // different type
//     BOOST_TEST(field_view(std::uint64_t(10)) > field_view(std::int64_t(20))); // example
// }

// BOOST_AUTO_TEST_CASE(operator_gte)
// {
//     BOOST_TEST(field_view(200) >= field_view(200)); // same type
//     BOOST_TEST(field_view("hola") >= field_view(-2)); // different type
//     BOOST_TEST(!(field_view(200) >= field_view(300))); // same type
//     BOOST_TEST(!(field_view(2) >= field_view("hola"))); // different type
// }

// // Can be placed in containers
// BOOST_AUTO_TEST_CASE(can_be_placed_in_set)
// {
//     std::set<field_view> s { field_view(200), field_view("hola"), field_view(200) };
//     BOOST_TEST(s.size() == 2u);
//     s.emplace("test");
//     BOOST_TEST(s.size() == 3u);
// }

// BOOST_AUTO_TEST_CASE(can_be_placed_in_map)
// {
//     std::map<field_view, const char*> m;
//     m[field_view(200)] = "msg0";
//     m[field_view("key")] = "msg1";
//     BOOST_TEST(m.size() == 2u);
//     BOOST_TEST(m.at(field_view("key")) == "msg1");
//     m[field_view("key")] = "msg2";
//     BOOST_TEST(m.size() == 2u);
//     BOOST_TEST(m.at(field_view("key")) == "msg2");
// }

BOOST_AUTO_TEST_SUITE_END()

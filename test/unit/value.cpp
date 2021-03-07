//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/value.hpp"
#include "test_common.hpp"
#include <boost/test/unit_test_suite.hpp>
#include <boost/type_index.hpp>
#include <boost/test/data/monomorphic/collection.hpp>
#include <boost/test/data/test_case.hpp>
#include <set>
#include <sstream>
#include <map>
#include <tuple>

BOOST_TEST_DONT_PRINT_LOG_VALUE(boost::mysql::value::variant_type)
BOOST_TEST_DONT_PRINT_LOG_VALUE(boost::mysql::date)
BOOST_TEST_DONT_PRINT_LOG_VALUE(boost::mysql::datetime)
BOOST_TEST_DONT_PRINT_LOG_VALUE(boost::mysql::time)
BOOST_TEST_DONT_PRINT_LOG_VALUE(boost::mysql::null_t)

using namespace boost::mysql::test;
using namespace boost::unit_test;
using boost::mysql::value;
using boost::typeindex::type_index;
using boost::typeindex::type_id;

BOOST_AUTO_TEST_SUITE(test_value)

using vt = value::variant_type;

template <class T>
std::string type_name() { return type_id<T>().pretty_name(); }

// Constructors
struct value_constructor_sample
{
    const char* name;
    value v;
    vt expected;

    value_constructor_sample(const char* name, value v, vt expected) :
        name(name), v(v), expected(expected) {}
};

std::ostream& operator<<(std::ostream& os, const value_constructor_sample& input)
{
    return os << input.name;
}

std::string test_string ("test");

template <class T>
T& non_const_int()
{
    static T res = 42;
    return res;
}

template <class T>
const T& const_int()
{
    static T res = 42;
    return res;
}

const value_constructor_sample all_value_constructor_samples [] {
    value_constructor_sample("default_constructor", value(), vt(boost::variant2::monostate())),
    value_constructor_sample("from_null_t", value(boost::mysql::null_t()), vt(boost::variant2::monostate())),
    value_constructor_sample("from_nullptr", value(nullptr), vt(boost::variant2::monostate())),
    value_constructor_sample("from_u8", value(std::uint8_t(0xff)), vt(std::uint64_t(0xff))),
    value_constructor_sample("from_u8_const_lvalue", value(const_int<std::uint8_t>()), vt(std::uint64_t(42))),
    value_constructor_sample("from_u8_lvalue", value(non_const_int<std::uint8_t>()), vt(std::uint64_t(42))),
    value_constructor_sample("from_u16", value(std::uint16_t(0xffff)), vt(std::uint64_t(0xffff))),
    value_constructor_sample("from_u16_const_lvalue", value(const_int<std::uint16_t>()), vt(std::uint64_t(42))),
    value_constructor_sample("from_u16_lvalue", value(non_const_int<std::uint16_t>()), vt(std::uint64_t(42))),
    value_constructor_sample("from_ushort", value((unsigned short)(0xffff)), vt(std::uint64_t(0xffff))),
    value_constructor_sample("from_u32", value(std::uint32_t(42)), vt(std::uint64_t(42))),
    value_constructor_sample("from_u32_const_lvalue", value(const_int<std::uint32_t>()), vt(std::uint64_t(42))),
    value_constructor_sample("from_u32_lvalue", value(non_const_int<std::uint32_t>()), vt(std::uint64_t(42))),
    value_constructor_sample("from_uint", value(42u), vt(std::uint64_t(42))),
    value_constructor_sample("from_ulong", value(42uL), vt(std::uint64_t(42))),
    value_constructor_sample("from_ulonglong", value(42uLL), vt(std::uint64_t(42))),
    value_constructor_sample("from_u64", value(std::uint64_t(42)), vt(std::uint64_t(42))),
    value_constructor_sample("from_u64_const_lvalue", value(const_int<std::uint64_t>()), vt(std::uint64_t(42))),
    value_constructor_sample("from_u64_lvalue", value(non_const_int<std::uint64_t>()), vt(std::uint64_t(42))),
    value_constructor_sample("from_s8", value(std::int8_t(-42)), vt(std::int64_t(-42))),
    value_constructor_sample("from_s8_const_lvalue", value(const_int<std::int8_t>()), vt(std::int64_t(42))),
    value_constructor_sample("from_s8_lvalue", value(non_const_int<std::int8_t>()), vt(std::int64_t(42))),
    value_constructor_sample("from_s16", value(std::int16_t(-42)), vt(std::int64_t(-42))),
    value_constructor_sample("from_s16_const_lvalue", value(const_int<std::int16_t>()), vt(std::int64_t(42))),
    value_constructor_sample("from_s16_lvalue", value(non_const_int<std::int16_t>()), vt(std::int64_t(42))),
    value_constructor_sample("from_sshort", value(short(-42)), vt(std::int64_t(-42))),
    value_constructor_sample("from_s32", value(std::int32_t(-42)), vt(std::int64_t(-42))),
    value_constructor_sample("from_s32_const_lvalue", value(const_int<std::int32_t>()), vt(std::int64_t(42))),
    value_constructor_sample("from_s32_lvalue", value(non_const_int<std::int32_t>()), vt(std::int64_t(42))),
    value_constructor_sample("from_sint", value(-42), vt(std::int64_t(-42))),
    value_constructor_sample("from_slong", value(-42L), vt(std::int64_t(-42))),
    value_constructor_sample("from_slonglong", value(-42LL), vt(std::int64_t(-42))),
    value_constructor_sample("from_s64", value(std::int64_t(-42)), vt(std::int64_t(-42))),
    value_constructor_sample("from_s64_const_lvalue", value(const_int<std::int64_t>()), vt(std::int64_t(42))),
    value_constructor_sample("from_s64_lvalue", value(non_const_int<std::int64_t>()), vt(std::int64_t(42))),
    value_constructor_sample("from_string_view", value(boost::string_view("test")), vt("test")),
    value_constructor_sample("from_string", value(test_string), vt("test")),
    value_constructor_sample("from_const_char", value("test"), vt("test")),
    value_constructor_sample("from_float", value(4.2f), vt(4.2f)),
    value_constructor_sample("from_double", value(4.2), vt(4.2)),
    value_constructor_sample("from_date", value(makedate(2020, 1, 10)), vt(makedate(2020, 1, 10))),
    value_constructor_sample("from_datetime", value(makedt(2020, 1, 10, 5)), vt(makedt(2020, 1, 10, 5))),
    value_constructor_sample("from_time", value(maket(1, 2, 3)), vt(maket(1, 2, 3)))
};

BOOST_DATA_TEST_CASE(constructor, data::make(all_value_constructor_samples))
{
    BOOST_TEST(sample.v.to_variant() == sample.expected);
}

// Copy and move
BOOST_AUTO_TEST_CASE(copy_constructor_from_non_const_lvalue)
{
    value v (10);
    value v2 (v);
    BOOST_TEST(v2.to_variant() == vt(std::int64_t(10)));
}

BOOST_AUTO_TEST_CASE(copy_constructor_from_const_lvalue)
{
    const value v (10);
    value v2 (v);
    BOOST_TEST(v2.to_variant() == vt(std::int64_t(10)));
}

BOOST_AUTO_TEST_CASE(move_constructor)
{
    value v (10);
    value v2 (std::move(v));
    BOOST_TEST(v2.to_variant() == vt(std::int64_t(10)));
}

BOOST_AUTO_TEST_CASE(copy_assignment_from_non_const_lvalue)
{
    value v (10);
    value v2;
    v2 = v;
    BOOST_TEST(v2.to_variant() == vt(std::int64_t(10)));
}

BOOST_AUTO_TEST_CASE(copy_assignament_from_const_lvalue)
{
    const value v (10);
    value v2;
    v2 = v;
    BOOST_TEST(v2.to_variant() == vt(std::int64_t(10)));
}

BOOST_AUTO_TEST_CASE(move_assignment)
{
    value v (10);
    value v2;
    v2 = std::move(v);
    BOOST_TEST(v2.to_variant() == vt(std::int64_t(10)));
}

// accessors: is, is_convertible_to, is_null, get, get_optional
using all_types = std::tuple<
    boost::mysql::null_t,
    std::uint64_t,
    std::int64_t,
    boost::string_view,
    float,
    double,
    boost::mysql::date,
    boost::mysql::datetime,
    boost::mysql::time
>;

struct accessors_sample
{
    const char* name;
    value v;
    type_index is_type; // the type for which is() should return true
    std::map<type_index, vt> conversions;
    bool is_null;

    accessors_sample(const char* name, value v, type_index is, std::map<type_index, vt>&& convs, bool is_null = false) :
        name(name),
        v(v),
        is_type(is),
        conversions(std::move(convs)),
        is_null(is_null)
    {
    }
};

std::ostream& operator<<(std::ostream& os, const accessors_sample& input)
{
    return os << input.name;
}

template <class... Types>
std::map<type_index, vt> make_conversions(const Types&... types)
{
    return { { type_id<Types>(), types }... };
}

template <class T>
accessors_sample make_default_accessors_sample(
    const char* name,
    const T& v,
    bool is_null = false
)
{
    return accessors_sample(name, value(v), type_id<T>(), make_conversions(v), is_null);
}

const accessors_sample all_accessors_samples [] {
    make_default_accessors_sample("null", boost::mysql::null_t(), true),
    accessors_sample("i64_positive", value(std::int64_t(42)), type_id<std::int64_t>(),
            make_conversions(std::int64_t(42), std::uint64_t(42))),
    accessors_sample("i64_negative", value(std::int64_t(-42)), type_id<std::int64_t>(),
            make_conversions(std::int64_t(-42))),
    accessors_sample("i64_zero", value(std::int64_t(0)), type_id<std::int64_t>(),
            make_conversions(std::int64_t(0), std::uint64_t(0))),
    accessors_sample("u64_small", value(std::uint64_t(42)), type_id<std::uint64_t>(),
            make_conversions(std::int64_t(42), std::uint64_t(42))),
    accessors_sample("u64_big", value(std::uint64_t(0xfffffffffffffffe)), type_id<std::uint64_t>(),
            make_conversions(std::uint64_t(0xfffffffffffffffe))),
    accessors_sample("u64_zero", value(std::uint64_t(0)), type_id<std::uint64_t>(),
            make_conversions(std::int64_t(0), std::uint64_t(0))),
            make_default_accessors_sample("string_view", makesv("test")),
    accessors_sample("float", value(4.2f), type_id<float>(),
            make_conversions(4.2f, double(4.2f))),
    make_default_accessors_sample("double", 4.2),
    make_default_accessors_sample("date", makedate(2020, 10, 5)),
    make_default_accessors_sample("datetime", makedt(2020, 10, 5, 10, 20, 30)),
    make_default_accessors_sample("time", maket(10, 20, 30))
};

// Template + data tests not supported. We use template
// test cases and use this function to emulate data test cases
template <class Callable>
void for_each_accessors_sample(Callable&& cb)
{
    for (const auto& sample: all_accessors_samples)
    {
        BOOST_TEST_CHECKPOINT(sample);
        cb(sample);
    }
}

BOOST_DATA_TEST_CASE(is_null, data::make(all_accessors_samples))
{
    BOOST_TEST(sample.v.is_null() == sample.is_null);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(is, T, all_types)
{
    for_each_accessors_sample([](const accessors_sample& sample) {
        bool expected = sample.is_type == type_id<T>();
        BOOST_TEST(sample.v.is<T>() == expected, sample);
    });
}

BOOST_AUTO_TEST_CASE_TEMPLATE(is_convertible_to, T, all_types)
{
    for_each_accessors_sample([](const accessors_sample& sample) {
        auto it = sample.conversions.find(type_id<T>());
        bool expected = it != sample.conversions.end();
        BOOST_TEST(sample.v.is_convertible_to<T>() == expected, sample);
    });
}

BOOST_AUTO_TEST_CASE_TEMPLATE(get, T, all_types)
{
    for_each_accessors_sample([](const accessors_sample& sample) {
        auto it = sample.conversions.find(type_id<T>());
        if (it != sample.conversions.end())
        {
            T expected = boost::variant2::get<T>(it->second);
            BOOST_TEST(sample.v.get<T>() == expected, sample);
        }
        else
        {
            BOOST_CHECK_THROW(sample.v.get<T>(), boost::mysql::bad_value_access);
        }
    });
}

BOOST_AUTO_TEST_CASE_TEMPLATE(get_optional, T, all_types)
{
    for_each_accessors_sample([](const accessors_sample& sample) {
        auto it = sample.conversions.find(type_id<T>());
        if (it != sample.conversions.end())
        {
            auto expected = boost::variant2::get<T>(it->second);
            auto opt = sample.v.get_optional<T>();
            BOOST_TEST_REQUIRE(opt.has_value(), sample);
            BOOST_TEST(*opt == expected, sample);
        }
        else
        {
            BOOST_TEST(!sample.v.get_optional<T>().has_value(), sample);
        }
    });
}

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL

BOOST_AUTO_TEST_CASE_TEMPLATE(get_std_optional, T, all_types)
{
    for_each_accessors_sample([](const accessors_sample& sample) {
        auto it = sample.conversions.find(type_id<T>());
        if (it != sample.conversions.end())
        {
            auto expected = boost::variant2::get<T>(it->second);
            auto opt = sample.v.get_std_optional<T>();
            BOOST_TEST_REQUIRE(opt.has_value(), sample);
            BOOST_TEST(*opt == expected, sample);
        }
        else
        {
            BOOST_TEST(!sample.v.get_std_optional<T>().has_value(), sample);
        }
    });
}

#endif // BOOST_NO_CXX17_HDR_OPTIONAL


// operator== and operator!=
struct value_equality_fixture
{
    std::vector<value> values = make_value_vector(
        std::int64_t(-1),
        std::uint64_t(0x100000000),
        "string",
        3.14f,
        8.89,
        makedate(2019, 10, 1),
        makedt(2019, 10, 1, 10),
        maket(0, 0, -10),
        boost::mysql::null_t()
    );
};

BOOST_FIXTURE_TEST_CASE(operators_eq_ne_different_type, value_equality_fixture)
{
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        for (std::size_t j = 0; j < i; ++j)
        {
            BOOST_TEST(!(values.at(i) == values.at(j)));
            BOOST_TEST(values.at(i) != values.at(j));
        }
    }
}

BOOST_FIXTURE_TEST_CASE(operators_eq_ne_same_type_different_value, value_equality_fixture)
{
    auto other_values = make_value_vector(
        std::int64_t(-22),
        std::uint64_t(222),
        "other_string",
        -3.0f,
        8e24,
        makedate(2019, 9, 1),
        makedt(2019, 9, 1, 10),
        maket(0, 0, 10),
        boost::mysql::null_t()
    );

    // Note: null_t (the last value) can't have other value than null_t()
    // so it is excluded from this test
    for (std::size_t i = 0; i < values.size() - 1; ++i)
    {
        BOOST_TEST(!(values.at(i) == other_values.at(i)));
        BOOST_TEST(values.at(i) != other_values.at(i));
    }
}

BOOST_FIXTURE_TEST_CASE(operators_eq_ne_same_type_same_value, value_equality_fixture)
{
    std::vector<value> values_copy = values;
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        BOOST_TEST(values.at(i) == values_copy.at(i));
        BOOST_TEST(!(values.at(i) != values_copy.at(i)));
    }
}

BOOST_FIXTURE_TEST_CASE(operators_eq_ne_self_comparison, value_equality_fixture)
{
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        BOOST_TEST(values.at(i) == values.at(i));
        BOOST_TEST(!(values.at(i) != values.at(i)));
    }
}

// operator<<
struct stream_sample
{
    std::string name;
    value input;
    std::string expected;

    template <class T>
    stream_sample(std::string&& name, T input, std::string&& expected) :
        name(std::move(name)),
        input(input),
        expected(std::move(expected))
    {
    }
};

std::ostream& operator<<(std::ostream& os, const stream_sample& input)
{
    return os << input.name;
}

// Helper struct to define stream operations for date, datetime and time
// We will list the possibilities for each component (hours, minutes, days...) and will
// take the Cartessian product of all them
struct component_value
{
    const char* name;
    int v;
    const char* repr;
};

void add_date_samples(std::vector<stream_sample>& output)
{
    constexpr component_value year_values [] = {
        { "min", 0, "0000" },
        { "onedig",  1, "0001" },
        { "twodig", 98, "0098" },
        { "threedig", 789, "0789" },
        { "regular", 1999, "1999" },
        { "max", 9999, "9999" }
    };

    constexpr component_value month_values [] = {
        { "min", 1, "01" },
        { "max", 12, "12" }
    };

    constexpr component_value day_values [] = {
        { "min", 1, "01" },
        { "max", 31, "31" }
    };

    for (const auto& year : year_values)
    {
        for (const auto& month : month_values)
        {
            for (const auto& day : day_values)
            {
                std::string name = stringize(
                    "date_year", year.name,
                    "_month", month.name,
                    "_day", day.name
                );
                std::string str_val = stringize(year.repr, '-', month.repr, '-', day.repr);
                value val (makedate(year.v, static_cast<unsigned>(month.v), static_cast<unsigned>(day.v)));
                output.emplace_back(std::move(name), val, std::move(str_val));
            }
        }
    }
}

void add_datetime_samples(std::vector<stream_sample>& output)
{
    constexpr component_value year_values [] = {
        { "min", 0, "0000" },
        { "onedig",  1, "0001" },
        { "twodig", 98, "0098" },
        { "threedig", 789, "0789" },
        { "regular", 1999, "1999" },
        { "max", 9999, "9999" }
    };

    constexpr component_value month_values [] = {
        { "min", 1, "01" },
        { "max", 12, "12" }
    };

    constexpr component_value day_values [] = {
        { "min", 1, "01" },
        { "max", 31, "31" }
    };

    constexpr component_value hours_values [] = {
        { "zero", 0, "00" },
        { "onedigit", 5, "05" },
        { "max", 23, "23" }
    };

    constexpr component_value mins_secs_values [] = {
        { "zero", 0, "00" },
        { "onedigit", 5, "05" },
        { "twodigits", 59, "59" }
    };

    constexpr component_value micros_values [] = {
        { "zero", 0, "000000" },
        { "onedigit", 5, "000005" },
        { "twodigits", 50, "000050" },
        { "max", 999999, "999999" },
    };

    for (const auto& year : year_values)
    {
        for (const auto& month : month_values)
        {
            for (const auto& day : day_values)
            {
                for (const auto& hours: hours_values)
                {
                    for (const auto& mins: mins_secs_values)
                    {
                        for (const auto& secs: mins_secs_values)
                        {
                            for (const auto& micros: micros_values)
                            {
                                std::string name = stringize(
                                    "datetime_year", year.name,
                                    "_month", month.name,
                                    "_day", day.name,
                                    "_h", hours.name,
                                    "_m", mins.name,
                                    "_s", secs.name,
                                    "_u", micros.name
                                );
                                std::string str_val = stringize(
                                    year.repr, '-',
                                    month.repr, '-',
                                    day.repr, ' ',
                                    hours.repr, ':',
                                    mins.repr, ':',
                                    secs.repr, '.',
                                    micros.repr
                                );
                                auto val = makedt(
                                    year.v,
                                    static_cast<unsigned>(month.v),
                                    static_cast<unsigned>(day.v),
                                    hours.v,
                                    mins.v,
                                    secs.v,
                                    micros.v
                                );
                                output.emplace_back(std::move(name), value(val), std::move(str_val));
                            }
                        }
                    }
                }
            }
        }
    }
}

void add_time_samples(std::vector<stream_sample>& output)
{
    constexpr component_value sign_values [] = {
        { "positive", 1, "" },
        { "negative", -1, "-" }
    };

    constexpr component_value hours_values [] = {
        { "zero", 0, "00" },
        { "onedigit", 5, "05" },
        { "twodigits", 23, "23" },
        { "max", 838, "838" }
    };

    constexpr component_value mins_secs_values [] = {
        { "zero", 0, "00" },
        { "onedigit", 5, "05" },
        { "twodigits", 59, "59" }
    };

    constexpr component_value micros_values [] = {
        { "zero", 0, "000000" },
        { "onedigit", 5, "000005" },
        { "twodigits", 50, "000050" },
        { "max", 999999, "999999" },
    };

    for (const auto& sign: sign_values)
    {
        for (const auto& hours: hours_values)
        {
            for (const auto& mins: mins_secs_values)
            {
                for (const auto& secs: mins_secs_values)
                {
                    for (const auto& micros: micros_values)
                    {
                        std::string name = stringize(
                            "time_", sign.name,
                            "_h", hours.name,
                            "_m", mins.name,
                            "_s", secs.name,
                            "_u", micros.name
                        );
                        std::string str_val = stringize(
                            sign.repr,
                            hours.repr, ':',
                            mins.repr, ':',
                            secs.repr, '.',
                            micros.repr
                        );
                        auto val = sign.v * maket(hours.v,
                                mins.v, secs.v, micros.v);
                        if (sign.v == -1 && val == maket(0, 0, 0))
                            continue; // This case makes no sense, as negative zero is represented as zero
                        output.emplace_back(std::move(name), value(val), std::move(str_val));
                    }
                }
            }
        }
    }
}

std::vector<stream_sample> make_stream_samples()
{
    std::vector<stream_sample> res {
        { "null", boost::mysql::null_t(), "<NULL>" },
        { "i64_positive", std::int64_t(42), "42" },
        { "i64_negative", std::int64_t(-90), "-90" },
        { "i64_zero", std::int64_t(0), "0" },
        { "u64_positive", std::uint64_t(42), "42" },
        { "u64_zero", std::uint64_t(0), "0" },
        { "string_view", "a_string", "a_string" },
        { "float", 2.43f, "2.43" },
        { "double", 8.12, "8.12" },
    };
    add_date_samples(res);
    add_datetime_samples(res);
    add_time_samples(res);
    return res;
}

BOOST_DATA_TEST_CASE(operator_stream, data::make(make_stream_samples()))
{
    std::ostringstream ss;
    ss << sample.input;
    BOOST_TEST(ss.str() == sample.expected);
}

// Operators <, <=, >, >= (variant behavior)
BOOST_AUTO_TEST_CASE(operator_lt)
{
    BOOST_TEST(value(100) < value(200)); // same type
    BOOST_TEST(value(-2) < value("hola")); // different type
    BOOST_TEST(!(value(200) < value(200))); // same type
    BOOST_TEST(!(value("hola") < value(2))); // different type
}

BOOST_AUTO_TEST_CASE(operator_lte)
{
    BOOST_TEST(value(200) <= value(200)); // same type
    BOOST_TEST(value(-2) <= value("hola")); // different type
    BOOST_TEST(!(value(300) <= value(200))); // same type
    BOOST_TEST(!(value("hola") <= value(2))); // different type
}

BOOST_AUTO_TEST_CASE(operator_gt)
{
    BOOST_TEST(value(200) > value(100)); // same type
    BOOST_TEST(value("hola") > value(2)); // different type
    BOOST_TEST(!(value(200) > value(200))); // same type
    BOOST_TEST(!(value(-2) > value("hola"))); // different type
    BOOST_TEST(value(std::uint64_t(10)) > value(std::int64_t(20))); // example
}

BOOST_AUTO_TEST_CASE(operator_gte)
{
    BOOST_TEST(value(200) >= value(200)); // same type
    BOOST_TEST(value("hola") >= value(-2)); // different type
    BOOST_TEST(!(value(200) >= value(300))); // same type
    BOOST_TEST(!(value(2) >= value("hola"))); // different type
}

// Can be placed in containers
BOOST_AUTO_TEST_CASE(can_be_placed_in_set)
{
    std::set<value> s { value(200), value("hola"), value(200) };
    BOOST_TEST(s.size() == 2);
    s.emplace("test");
    BOOST_TEST(s.size() == 3);
}

BOOST_AUTO_TEST_CASE(can_be_placed_in_map)
{
    std::map<value, const char*> m;
    m[value(200)] = "msg0";
    m[value("key")] = "msg1";
    BOOST_TEST(m.size() == 2);
    BOOST_TEST(m.at(value("key")) == "msg1");
    m[value("key")] = "msg2";
    BOOST_TEST(m.size() == 2);
    BOOST_TEST(m.at(value("key")) == "msg2");
}

BOOST_AUTO_TEST_SUITE_END() // test_value

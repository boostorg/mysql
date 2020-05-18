//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <gtest/gtest.h>
#include <boost/type_index.hpp>
#include <sstream>
#include <map>
#include "boost/mysql/value.hpp"
#include "test_common.hpp"

using namespace boost::mysql::test;
using namespace testing;
using namespace date::literals;
using namespace std::chrono;
using boost::mysql::value;
using boost::typeindex::type_index;
using boost::typeindex::type_id;

namespace
{

using vt = value::variant_type;

template <typename T>
std::string type_name() { return type_id<T>().pretty_name(); }

// Constructors
struct value_constructor_testcase : named_param
{
    std::string name;
    value v;
    vt expected;

    value_constructor_testcase(std::string name, value v, vt expected) :
        name(std::move(name)), v(v), expected(expected) {}
};

struct ValueConstructorTest :  public TestWithParam<value_constructor_testcase>
{
};

TEST_P(ValueConstructorTest, Constructor_Trivial_ContainedVariantMatches)
{
    EXPECT_EQ(GetParam().v.to_variant(), GetParam().expected);
}

std::string test_string ("test");

template <typename T>
T non_const_int = 42;

template <typename T>
const T const_int = 42;

INSTANTIATE_TEST_SUITE_P(Default, ValueConstructorTest, Values(
    value_constructor_testcase("default_constructor", value(), vt(nullptr)),
    value_constructor_testcase("from_nullptr", value(nullptr), vt(nullptr)),
    value_constructor_testcase("from_u8", value(std::uint8_t(0xff)), vt(std::uint64_t(0xff))),
    value_constructor_testcase("from_u8_const_lvalue", value(const_int<std::uint8_t>), vt(std::uint64_t(42))),
    value_constructor_testcase("from_u8_lvalue", value(non_const_int<std::uint8_t>), vt(std::uint64_t(42))),
    value_constructor_testcase("from_u16", value(std::uint16_t(0xffff)), vt(std::uint64_t(0xffff))),
    value_constructor_testcase("from_u16_const_lvalue", value(const_int<std::uint16_t>), vt(std::uint64_t(42))),
    value_constructor_testcase("from_u16_lvalue", value(non_const_int<std::uint16_t>), vt(std::uint64_t(42))),
    value_constructor_testcase("from_ushort", value((unsigned short)(0xffff)), vt(std::uint64_t(0xffff))),
    value_constructor_testcase("from_u32", value(std::uint32_t(42)), vt(std::uint64_t(42))),
    value_constructor_testcase("from_u32_const_lvalue", value(const_int<std::uint32_t>), vt(std::uint64_t(42))),
    value_constructor_testcase("from_u32_lvalue", value(non_const_int<std::uint32_t>), vt(std::uint64_t(42))),
    value_constructor_testcase("from_uint", value(42u), vt(std::uint64_t(42))),
    value_constructor_testcase("from_ulong", value(42uL), vt(std::uint64_t(42))),
    value_constructor_testcase("from_ulonglong", value(42uLL), vt(std::uint64_t(42))),
    value_constructor_testcase("from_u64", value(std::uint64_t(42)), vt(std::uint64_t(42))),
    value_constructor_testcase("from_u64_const_lvalue", value(const_int<std::uint64_t>), vt(std::uint64_t(42))),
    value_constructor_testcase("from_u64_lvalue", value(non_const_int<std::uint64_t>), vt(std::uint64_t(42))),
    value_constructor_testcase("from_s8", value(std::int8_t(-42)), vt(std::int64_t(-42))),
    value_constructor_testcase("from_s8_const_lvalue", value(const_int<std::int8_t>), vt(std::int64_t(42))),
    value_constructor_testcase("from_s8_lvalue", value(non_const_int<std::int8_t>), vt(std::int64_t(42))),
    value_constructor_testcase("from_s16", value(std::int16_t(-42)), vt(std::int64_t(-42))),
    value_constructor_testcase("from_s16_const_lvalue", value(const_int<std::int16_t>), vt(std::int64_t(42))),
    value_constructor_testcase("from_s16_lvalue", value(non_const_int<std::int16_t>), vt(std::int64_t(42))),
    value_constructor_testcase("from_sshort", value(short(-42)), vt(std::int64_t(-42))),
    value_constructor_testcase("from_s32", value(std::int32_t(-42)), vt(std::int64_t(-42))),
    value_constructor_testcase("from_s32_const_lvalue", value(const_int<std::int32_t>), vt(std::int64_t(42))),
    value_constructor_testcase("from_s32_lvalue", value(non_const_int<std::int32_t>), vt(std::int64_t(42))),
    value_constructor_testcase("from_sint", value(-42), vt(std::int64_t(-42))),
    value_constructor_testcase("from_slong", value(-42L), vt(std::int64_t(-42))),
    value_constructor_testcase("from_slonglong", value(-42LL), vt(std::int64_t(-42))),
    value_constructor_testcase("from_s64", value(std::int64_t(-42)), vt(std::int64_t(-42))),
    value_constructor_testcase("from_s64_const_lvalue", value(const_int<std::int64_t>), vt(std::int64_t(42))),
    value_constructor_testcase("from_s64_lvalue", value(non_const_int<std::int64_t>), vt(std::int64_t(42))),
    value_constructor_testcase("from_string_view", value(std::string_view("test")), vt("test")),
    value_constructor_testcase("from_string", value(test_string), vt("test")),
    value_constructor_testcase("from_const_char", value("test"), vt("test")),
    value_constructor_testcase("from_float", value(4.2f), vt(4.2f)),
    value_constructor_testcase("from_double", value(4.2), vt(4.2)),
    value_constructor_testcase("from_date", value(makedate(2020, 1, 10)), vt(makedate(2020, 1, 10))),
    value_constructor_testcase("from_datetime", value(makedt(2020, 1, 10, 5)), vt(makedt(2020, 1, 10, 5))),
    value_constructor_testcase("from_time", value(maket(1, 2, 3)), vt(maket(1, 2, 3)))
), test_name_generator);

// Copy and move
TEST(ValueTest, CopyConstructor_FromNonConstLValue_Copies)
{
    value v (10);
    value v2 (v);
    EXPECT_EQ(v2.to_variant(), vt(std::int64_t(10)));
}

TEST(ValueTest, CopyConstructor_FromConstLValue_Copies)
{
    const value v (10);
    value v2 (v);
    EXPECT_EQ(v2.to_variant(), vt(std::int64_t(10)));
}

TEST(ValueTest, MoveConstructor_Trivial_Copies)
{
    value v (10);
    value v2 (std::move(v));
    EXPECT_EQ(v2.to_variant(), vt(std::int64_t(10)));
}

TEST(ValueTest, CopyAssignment_FromNonConstLValue_Copies)
{
    value v (10);
    value v2;
    v2 = v;
    EXPECT_EQ(v2.to_variant(), vt(std::int64_t(10)));
}

TEST(ValueTest, CopyAssignament_FromConstLValue_Copies)
{
    const value v (10);
    value v2;
    v2 = v;
    EXPECT_EQ(v2.to_variant(), vt(std::int64_t(10)));
}

TEST(ValueTest, MoveAssignment_Trivial_Copies)
{
    value v (10);
    value v2;
    v2 = std::move(v);
    EXPECT_EQ(v2.to_variant(), vt(std::int64_t(10)));
}

// accessors: is, is_convertible_to, is_null, get, get_optional
constexpr vt all_types [] = {
    vt(nullptr),
    vt(std::uint64_t{}),
    vt(std::int64_t{}),
    vt(std::string_view{}),
    vt(float{}),
    vt(double{}),
    vt(boost::mysql::date{}),
    vt(boost::mysql::datetime{}),
    vt(boost::mysql::time{})
};

template <typename Callable>
void for_each_type(Callable&& cb)
{
    for (auto type_variant : all_types)
    {
        std::visit(std::forward<Callable>(cb), type_variant);
    }
}

struct accessors_testcase : named_param
{
    std::string name;
    value v;
    type_index is_type; // the type for which is() should return true
    std::map<type_index, vt> conversions;

    accessors_testcase(std::string&& name, value v, type_index is, std::map<type_index, vt>&& convs) :
        name(std::move(name)),
        v(v),
        is_type(is),
        conversions(std::move(convs))
    {
    }
};

template <typename... Types>
std::map<type_index, vt> make_conversions(const Types&... types)
{
    return { { type_id<Types>(), types }... };
}

template <typename T>
accessors_testcase make_default_accessors_testcase(
    std::string&& name,
    const T& v
)
{
    return accessors_testcase(std::move(name), value(v), type_id<T>(), make_conversions(v));
}

struct ValueAccessorsTest : TestWithParam<accessors_testcase>
{
};

TEST_P(ValueAccessorsTest, IsNull_Trivial_ReturnsTrueOnlyForNullptrAlternative)
{
    bool expected = GetParam().is_type == type_id<std::nullptr_t>();
    EXPECT_EQ(GetParam().v.is_null(), expected);
}

TEST_P(ValueAccessorsTest, Is_Trivial_ReturnsTrueOnlyIfTypeMatches)
{
    for_each_type([](auto type_value) {
        using T = decltype(type_value);
        bool expected = GetParam().is_type == type_id<T>();
        EXPECT_EQ(GetParam().v.is<T>(), expected) << type_name<T>();
    });
}

TEST_P(ValueAccessorsTest, IsConvertibleTo_TypeAllowsConversions_ReturnsTrue)
{
    for_each_type([](auto type_value) {
        using T = decltype(type_value);
        auto it = GetParam().conversions.find(type_id<T>());
        bool expected = it != GetParam().conversions.end();
        EXPECT_EQ(GetParam().v.is_convertible_to<T>(), expected) << type_name<T>();
    });
}

TEST_P(ValueAccessorsTest, Get_TypeConvertibleToTarget_ReturnsConvertedValue)
{
    for_each_type([](auto type_value) {
        using T = decltype(type_value);
        auto it = GetParam().conversions.find(type_id<T>());
        if (it != GetParam().conversions.end())
        {
            T expected = std::get<T>(it->second);
            EXPECT_EQ(GetParam().v.get<T>(), expected) << type_name<T>();
        }
    });
}

TEST_P(ValueAccessorsTest, Get_TypeNotConvertibleToTarget_Throws)
{
    for_each_type([](auto type_value) {
        using T = decltype(type_value);
        auto it = GetParam().conversions.find(type_id<T>());
        if (it == GetParam().conversions.end())
        {
            EXPECT_THROW(GetParam().v.get<T>(), std::bad_variant_access) << type_name<T>();
        }
    });
}

TEST_P(ValueAccessorsTest, GetOptional_TypeConvertibleToTarget_ReturnsConvertedValue)
{
    for_each_type([](auto type_value) {
        using T = decltype(type_value);
        auto it = GetParam().conversions.find(type_id<T>());
        if (it != GetParam().conversions.end())
        {
            auto expected = std::get<T>(it->second);
            auto opt = GetParam().v.get_optional<T>();
            ASSERT_TRUE(opt) << type_name<T>();
            EXPECT_EQ(*opt, expected) << type_name<T>();
        }
    });
}

TEST_P(ValueAccessorsTest, GetOptional_TypeNotConvertibleToTarget_ReturnsEmptyOptional)
{
    for_each_type([](auto type_value) {
        using T = decltype(type_value);
        auto it = GetParam().conversions.find(type_id<T>());
        if (it == GetParam().conversions.end())
        {
            EXPECT_FALSE(GetParam().v.get_optional<T>()) << type_name<T>();
        }
    });
}

INSTANTIATE_TEST_SUITE_P(Default, ValueAccessorsTest, Values(
    make_default_accessors_testcase("null", nullptr),
    accessors_testcase("i64_positive", value(std::int64_t(42)), type_id<std::int64_t>(),
            make_conversions(std::int64_t(42), std::uint64_t(42))),
    accessors_testcase("i64_negative", value(std::int64_t(-42)), type_id<std::int64_t>(),
            make_conversions(std::int64_t(-42))),
    accessors_testcase("i64_zero", value(std::int64_t(0)), type_id<std::int64_t>(),
            make_conversions(std::int64_t(0), std::uint64_t(0))),
    accessors_testcase("u64_small", value(std::uint64_t(42)), type_id<std::uint64_t>(),
            make_conversions(std::int64_t(42), std::uint64_t(42))),
    accessors_testcase("u64_big", value(std::uint64_t(0xfffffffffffffffe)), type_id<std::uint64_t>(),
            make_conversions(std::uint64_t(0xfffffffffffffffe))),
    accessors_testcase("u64_zero", value(std::uint64_t(0)), type_id<std::uint64_t>(),
            make_conversions(std::int64_t(0), std::uint64_t(0))),
    make_default_accessors_testcase("string_view", makesv("test")),
    accessors_testcase("float", value(4.2f), type_id<float>(),
            make_conversions(4.2f, double(4.2f))),
    make_default_accessors_testcase("double", 4.2),
    make_default_accessors_testcase("date", makedate(2020, 10, 5)),
    make_default_accessors_testcase("datetime", makedt(2020, 10, 5, 10, 20, 30)),
    make_default_accessors_testcase("time", maket(10, 20, 30))
), test_name_generator);

// operator== and operator!=
struct ValueEqualityTest : public Test
{
    std::vector<value> values = makevalues(
        std::int32_t(20),
        std::int64_t(-1),
        std::uint32_t(0xffffffff),
        std::uint64_t(0x100000000),
        3.14f,
        8.89,
        boost::mysql::date(1_d/10/2019_y),
        boost::mysql::date(1_d/10/2019_y) + std::chrono::hours(10),
        boost::mysql::time(std::chrono::seconds(-10)),
        std::uint32_t(2010),
        nullptr
    );
};

TEST_F(ValueEqualityTest, OperatorsEqNe_DifferentType_ReturnNotEquals)
{
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        for (std::size_t j = 0; j < i; ++j)
        {
            EXPECT_FALSE(values.at(i) == values.at(j)) << "i=" << i << ", j=" << j;
            EXPECT_TRUE(values.at(i) != values.at(j))  << "i=" << i << ", j=" << j;
        }
    }
}

TEST_F(ValueEqualityTest, OperatorsEqNe_SameTypeDifferentValue_ReturnNotEquals)
{
    auto other_values = makevalues(
        std::int32_t(10),
        std::int64_t(-22),
        std::uint32_t(0xff6723),
        std::uint64_t(222),
        -3.0f,
        8e24,
        boost::mysql::date(1_d/9/2019_y),
        boost::mysql::date(1_d/9/2019_y) + std::chrono::hours(10),
        boost::mysql::time(std::chrono::seconds(10)),
        std::uint32_t(1900),
        nullptr
    );

    // Note: nullptr_t (the last value) can't have other value than nullptr
    // so it is excluded from this test
    for (std::size_t i = 0; i < values.size() - 1; ++i)
    {
        EXPECT_FALSE(values.at(i) == other_values.at(i)) << "i=" << i;
        EXPECT_TRUE(values.at(i) != other_values.at(i))  << "i=" << i;
    }
}

TEST_F(ValueEqualityTest, OperatorsEqNe_SameTypeSameValue_ReturnEquals)
{
    std::vector<value> values_copy = values;
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        EXPECT_TRUE(values.at(i) == values_copy.at(i))  << "i=" << i;
        EXPECT_FALSE(values.at(i) != values_copy.at(i)) << "i=" << i;
    }
}

// operator<<
struct stream_testcase : named_param
{
    std::string name;
    value input;
    std::string expected;

    template <typename T>
    stream_testcase(std::string&& name, T input, std::string&& expected) :
        name(std::move(name)),
        input(input),
        expected(std::move(expected))
    {
    }
};

struct ValueStreamTest : TestWithParam<stream_testcase>
{
};

TEST_P(ValueStreamTest, OutputStream_Trivial_ProducesExpectedString)
{
    std::ostringstream ss;
    ss << GetParam().input;
    EXPECT_EQ(ss.str(), GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(Default, ValueStreamTest, Values(
    stream_testcase("null", nullptr, "<NULL>"),
    stream_testcase("i64_positive", std::int64_t(42), "42"),
    stream_testcase("i64_negative", std::int64_t(-90), "-90"),
    stream_testcase("i64_zero", std::int64_t(0), "0"),
    stream_testcase("u64_positive", std::uint64_t(42), "42"),
    stream_testcase("u64_zero", std::uint64_t(0), "0"),
    stream_testcase("string_view", "a_string", "a_string"),
    stream_testcase("float", 2.43f, "2.43"),
    stream_testcase("double", 8.12, "8.12"),
    stream_testcase("date_regular", makedate(2019, 9, 1), "2019-09-01"),
    stream_testcase("date_zero", makedate(0, 9, 1), "0000-09-01"),
    stream_testcase("datetime_dhmsu", makedt(2019, 1, 8, 9, 20, 11, 123), "2019-01-08 09:20:11.000123"),
    stream_testcase("datetime_d", makedt(2019, 1, 8), "2019-01-08 00:00:00.000000"),
    stream_testcase("time_zero", maket(0, 0, 0), "00:00:00:000000"),
    stream_testcase("time_positive_h", maket(24, 0, 0), "24:00:00:000000"),
    stream_testcase("time_positive_hmsu", maket(210, 59, 59, 100), "210:59:59:000100"),
    stream_testcase("time_negative_hmsu", -maket(839, 20, 35, 999999), "-839:20:35:999999"),
    stream_testcase("time_positive_ms", maket(0, 2, 5), "00:02:05:000000"),
    stream_testcase("time_negative_ms", -maket(0, 21, 45), "-00:21:45:000000"),
    stream_testcase("time_positive_su", maket(0, 0, 1, 234000), "00:00:01:234000"),
    stream_testcase("time_negative_su", -maket(0, 0, 1, 234000), "-00:00:01:234000")
));

}

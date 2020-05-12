//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_DETAIL_PROTOCOL_SERIALIZATION_TEST_COMMON_HPP
#define BOOST_MYSQL_TEST_UNIT_DETAIL_PROTOCOL_SERIALIZATION_TEST_COMMON_HPP

#include "boost/mysql/detail/protocol/serialization.hpp"
#include "boost/mysql/detail/protocol/constants.hpp"
#include <gtest/gtest.h>
#include <string>
#include <any>
#include <functional>
#include <boost/type_index.hpp>
#include "test_common.hpp"

namespace boost {
namespace mysql {
namespace test {

template <std::size_t N>
std::ostream& operator<<(std::ostream& os, const std::array<char, N>& v)
{
    return os << std::string_view(v.data(), N);
}

inline std::ostream& operator<<(std::ostream& os, std::uint8_t value)
{
    return os << +value;
}
using ::date::operator<<;

// Operator == for structs
template <std::size_t index, typename T>
bool equals_struct(const T& lhs, const T& rhs)
{
    constexpr auto fields = detail::get_struct_fields<T>::value;
    if constexpr (index == std::tuple_size<decltype(fields)>::value)
    {
        return true;
    }
    else
    {
        constexpr auto pmem = std::get<index>(fields);
        return (rhs.*pmem == lhs.*pmem) && equals_struct<index+1>(lhs, rhs);
    }
}

template <typename T>
std::enable_if_t<detail::is_struct_with_fields<T>(), bool>
operator==(const T& lhs, const T& rhs)
{
    return equals_struct<0>(lhs, rhs);
}

// Operator << for value_holder
template <typename T>
std::ostream& operator<<(std::ostream& os, const detail::value_holder<T>& value)
{
    return os << value.value;
}

template <typename T>
std::enable_if_t<std::is_enum_v<T>, std::ostream&>
operator<<(std::ostream& os, T value)
{
    return os << boost::typeindex::type_id<T>().pretty_name() << "(" <<
            static_cast<std::underlying_type_t<T>>(value) << ")";
}

// Operator << for structs
template <std::size_t index, typename T>
void print_struct(std::ostream& os, const T& value)
{
    constexpr auto fields = detail::get_struct_fields<T>::value;
    if constexpr (index < std::tuple_size<decltype(fields)>::value)
    {
        constexpr auto pmem = std::get<index>(fields);
        os << "    " << (value.*pmem) << ",\n";
        print_struct<index+1>(os, value);
    }
}

template <typename T>
std::enable_if_t<detail::is_struct_with_fields<T>(), std::ostream&>
operator<<(std::ostream& os, const T& value)
{
    os << boost::typeindex::type_id<T>().pretty_name() << "(\n";
    print_struct<0>(os, value);
    os << ")\n";
    return os;
}

class any_value
{
public:
    virtual ~any_value() {}
    virtual void serialize(detail::serialization_context& ctx) const = 0;
    virtual std::size_t get_size(const detail::serialization_context& ctx) const = 0;
    virtual errc deserialize(detail::deserialization_context& ctx) = 0;
    virtual std::shared_ptr<any_value> default_construct() const = 0;
    virtual bool equals(const any_value& rhs) const = 0;
    virtual void print(std::ostream& os) const = 0;

    bool operator==(const any_value& rhs) const { return equals(rhs); }
};
inline std::ostream& operator<<(std::ostream& os, const any_value& value)
{
    value.print(os);
    return os;
}

template <typename T>
class any_value_impl : public any_value
{
    T value_;
public:
    any_value_impl(const T& v): value_(v) {};
    void serialize(detail::serialization_context& ctx) const override
    {
        ::boost::mysql::detail::serialize(ctx, value_);
    }
    std::size_t get_size(const detail::serialization_context& ctx) const override
    {
        return ::boost::mysql::detail::get_size(ctx, value_);
    }
    errc deserialize(detail::deserialization_context& ctx) override
    {
        return ::boost::mysql::detail::deserialize(ctx, value_);
    }
    std::shared_ptr<any_value> default_construct() const override
    {
        return std::make_shared<any_value_impl<T>>(T{});
    }
    bool equals(const any_value& rhs) const override
    {
        auto typed_value = dynamic_cast<const any_value_impl<T>*>(&rhs);
        return typed_value && (typed_value->value_ == value_);
    }
    void print(std::ostream& os) const override
    {
        os << value_;
    }
};

struct serialization_testcase : test::named_param
{
    std::shared_ptr<any_value> value;
    std::vector<uint8_t> expected_buffer;
    std::string name;
    detail::capabilities caps;
    std::any additional_storage;

    template <typename T>
    serialization_testcase(const T& v, std::vector<uint8_t>&& buff,
                    std::string&& name, std::uint32_t caps=0, std::any storage = {}):
        value(std::make_shared<any_value_impl<T>>(v)),
        expected_buffer(move(buff)),
        name(move(name)),
        caps(caps),
        additional_storage(std::move(storage))
    {
    }
};

// We expose this function so binary serialization, which does not employ
// the regular serialize() overloads, can use it
void do_serialize_test(
    const std::vector<std::uint8_t>& expected_buffer,
    const std::function<void(detail::serialization_context&)>& serializator,
    detail::capabilities caps = detail::capabilities()
);

// Test fixtures
struct SerializationFixture : public testing::TestWithParam<serialization_testcase> {}; // base
struct SerializeTest : SerializationFixture {}; // Only serialization
struct DeserializeTest : SerializationFixture {}; // Only deserialization
struct DeserializeSpaceTest : SerializationFixture {}; // Deserialization + extra/infra space
struct SerializeDeserializeTest : SerializationFixture {}; // Serialization + deserialization
struct FullSerializationTest : SerializationFixture {}; // All


} // test
} // mysql
} // boost


#endif /* TEST_SERIALIZATION_TEST_COMMON_HPP_ */

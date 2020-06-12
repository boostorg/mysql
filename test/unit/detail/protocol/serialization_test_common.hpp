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
#include <functional>
#include <boost/type_index.hpp>
#include <boost/any.hpp>
#include "test_common.hpp"

namespace boost {
namespace mysql {
namespace test {

template <std::size_t N>
std::ostream& operator<<(std::ostream& os, const std::array<char, N>& v)
{
    return os << boost::string_view(v.data(), N);
}

inline std::ostream& operator<<(std::ostream& os, std::uint8_t value)
{
    return os << +value;
}

// Operator == for structs. Very poor efficiency but does the
// job for the purpose of testing
template <typename T>
struct struct_member_comparer_op
{
    const T& lhs;
    const T& rhs;
    bool result {};

    struct_member_comparer_op(const T& lhs, const T& rhs) : lhs(lhs), rhs(rhs) {}

    template <typename... Types>
    void operator()(const Types&...)
    {
        std::tuple<Types...> lhs_as_tuple;
        std::tuple<Types...> rhs_as_tuple;
        T::apply(lhs, [&lhs_as_tuple](const Types&... args) {
            lhs_as_tuple = std::tuple<Types...>(args...);
        });
        T::apply(rhs, [&rhs_as_tuple](const Types&... args) {
            rhs_as_tuple = std::tuple<Types...>(args...);
        });
        result = lhs_as_tuple == rhs_as_tuple;
    }
};

template <typename T>
typename std::enable_if<detail::is_struct_with_fields<T>(), bool>::type
operator==(const T& lhs, const T& rhs)
{
    struct_member_comparer_op<T> op (lhs, rhs);
    T::apply(lhs, op);
    return op.result;
}

// Operator << for value_holder
template <typename T>
std::ostream& operator<<(std::ostream& os, const detail::value_holder<T>& value)
{
    return os << value.value;
}

template <typename T>
typename std::enable_if<std::is_enum<T>::value, std::ostream&>::type
operator<<(std::ostream& os, T value)
{
    return os << boost::typeindex::type_id<T>().pretty_name() << "(" <<
            static_cast<typename std::underlying_type<T>::type>(value) << ")";
}

// Operator << for structs
struct struct_print_op
{
    std::ostream& os;

    void impl() {}

    template <typename T, typename... Tail>
    void impl(const T& head, const Tail&... tail)
    {
        os << "    " << head << ",\n";
        impl(tail...);
    }

    template <typename... Types>
    void operator()(const Types&... values)
    {
        impl(values...);
    }
};

template <typename T>
typename std::enable_if<detail::is_struct_with_fields<T>(), std::ostream&>::type
operator<<(std::ostream& os, const T& value)
{
    os << boost::typeindex::type_id<T>().pretty_name() << "(\n";
    T::apply(value, struct_print_op{os});
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

struct serialization_testcase : public test::named
{
    std::shared_ptr<any_value> value;
    std::vector<uint8_t> expected_buffer;
    detail::capabilities caps;
    boost::any additional_storage;

    template <typename T>
    serialization_testcase(const T& v, std::vector<uint8_t>&& buff,
                    std::string&& name, std::uint32_t caps=0, boost::any&& storage = {}):
        named(std::move(name)),
        value(std::make_shared<any_value_impl<T>>(v)),
        expected_buffer(std::move(buff)),
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

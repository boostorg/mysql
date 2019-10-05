#ifndef TEST_SERIALIZATION_TEST_COMMON_HPP_
#define TEST_SERIALIZATION_TEST_COMMON_HPP_

#include "mysql/impl/basic_serialization.hpp"
#include "mysql/impl/messages.hpp"
#include "mysql/impl/constants.hpp"
#include <gtest/gtest.h>
#include <string>
#include <boost/type_index.hpp>

namespace mysql
{
namespace detail
{

// Operator == for structs
template <std::size_t index, typename T>
bool equals_struct(const T& lhs, const T& rhs)
{
	if constexpr (index == std::tuple_size<std::decay_t<decltype(T::fields)>>::value)
	{
		return true;
	}
	else
	{
		constexpr auto pmem = std::get<index>(T::fields);
		return (rhs.*pmem == lhs.*pmem) && equals_struct<index+1>(lhs, rhs);
	}
}

template <typename T>
std::enable_if_t<is_struct_with_fields<T>::value, bool>
operator==(const T& lhs, const T& rhs)
{
	return equals_struct<0>(lhs, rhs);
}


class TypeErasedValue
{
public:
	virtual ~TypeErasedValue() {}
	virtual void serialize(SerializationContext& ctx) const = 0;
	virtual std::size_t get_size(const SerializationContext& ctx) const = 0;
	virtual Error deserialize(DeserializationContext& ctx) = 0;
	virtual std::string get_type_name() const = 0;
	virtual std::shared_ptr<TypeErasedValue> default_construct() const = 0;
	virtual bool equals(const TypeErasedValue& rhs) const = 0;

	bool operator==(const TypeErasedValue& rhs) const { return equals(rhs); }
};

template <typename T>
class TypeErasedValueImpl : public TypeErasedValue
{
	T value_;
public:
	TypeErasedValueImpl(const T& v): value_(v) {};
	void serialize(SerializationContext& ctx) const override { ::mysql::detail::serialize(value_, ctx); }
	std::size_t get_size(const SerializationContext& ctx) const override { return ::mysql::detail::get_size(value_, ctx); }
	Error deserialize(DeserializationContext& ctx) override { return ::mysql::detail::deserialize(value_, ctx); }
	std::string get_type_name() const override
	{
		return boost::typeindex::type_id<T>().pretty_name();
	}
	std::shared_ptr<TypeErasedValue> default_construct() const override
	{
		T res; // intentionally not value-initializing it
		return std::make_shared<TypeErasedValueImpl<T>>(res);
	}
	bool equals(const TypeErasedValue& rhs) const
	{
		auto typed_value = dynamic_cast<const TypeErasedValueImpl<T>*>(&rhs);
		return typed_value && (typed_value->value_ == value_);
	}
};

struct SerializeParams
{
	std::shared_ptr<TypeErasedValue> value;
	std::vector<uint8_t> expected_buffer;
	std::string test_name;

	template <typename T>
	SerializeParams(const T& v, std::vector<uint8_t>&& buff,
			        std::string&& name="default"):
		value(std::make_shared<TypeErasedValueImpl<T>>(v)),
		expected_buffer(move(buff)),
		test_name(move(name))
	{
	}
};

std::ostream& operator<<(std::ostream& os, const SerializeParams& params)
{
	return os << params.value->get_type_name() << " - " << params.test_name;
}

std::vector<uint8_t> concat(std::vector<uint8_t>&& lhs, const std::vector<uint8_t>& rhs)
{
	size_t lhs_size = lhs.size();
	std::vector<uint8_t> res (move(lhs));
	res.resize(lhs_size + rhs.size());
	std::memcpy(res.data() + lhs_size, rhs.data(), rhs.size());
	return res;
}

// Test fixtures
struct SerializationFixture : public testing::TestWithParam<SerializeParams>
{
	static std::string_view make_buffer_view(const uint8_t* buff, size_t sz)
	{
		return std::string_view(reinterpret_cast<const char*>(buff), sz);
	}

	// get_size
	void get_size_test()
	{
		SerializationContext ctx (0, nullptr);
		auto size = GetParam().value->get_size(ctx);
		EXPECT_EQ(size, GetParam().expected_buffer.size());
	}

	// serialize
	void serialize_test()
	{
		auto expected_size = GetParam().expected_buffer.size();
		std::vector<uint8_t> buffer (expected_size + 8, 0xaa); // buffer overrun detector
		SerializationContext ctx (0, buffer.data());
		GetParam().value->serialize(ctx);

		// Iterator
		EXPECT_EQ(ctx.first(), buffer.data() + expected_size) << "Iterator not updated correctly";

		// Buffer
		std::string_view expected_populated = make_buffer_view(GetParam().expected_buffer.data(), expected_size);
		std::string_view actual_populated = make_buffer_view(buffer.data(), expected_size);
		EXPECT_EQ(expected_populated, actual_populated) << "Buffer contents incorrect";

		// Check for buffer overruns
		std::string expected_clean (8, 0xaa);
		std::string_view actual_clean = make_buffer_view(buffer.data() + expected_size, 8);
		EXPECT_EQ(expected_clean, actual_clean) << "Buffer overrun";
	}

	// deserialize
	void deserialize_test()
	{
		auto first = GetParam().expected_buffer.data();
		auto size = GetParam().expected_buffer.size();
		DeserializationContext ctx (first, first + size, 0);
		auto actual_value = GetParam().value->default_construct();
		auto err = actual_value->deserialize(ctx);

		// No error
		EXPECT_EQ(err, Error::ok);

		// Iterator advanced
		EXPECT_EQ(ctx.first(), first + size);

		// Actual value
		EXPECT_EQ(*actual_value, *GetParam().value);
	}

	void deserialize_extra_space_test()
	{
		std::vector<uint8_t> buffer (GetParam().expected_buffer);
		buffer.push_back(0xff);
		auto first = buffer.data();
		DeserializationContext ctx (first, first + buffer.size(), 0);
		auto actual_value = GetParam().value->default_construct();
		auto err = actual_value->deserialize(ctx);

		// No error
		EXPECT_EQ(err, Error::ok);

		// Iterator advanced
		EXPECT_EQ(ctx.first(), first + GetParam().expected_buffer.size());

		// Actual value
		EXPECT_EQ(*actual_value, *GetParam().value);
	}

	void deserialize_not_enough_space_test()
	{
		std::vector<uint8_t> buffer (GetParam().expected_buffer);
		buffer.back() = 0xaa; // try to detect any overruns
		DeserializationContext ctx (buffer.data(), buffer.data() + buffer.size() - 1, 0);
		auto actual_value = GetParam().value->default_construct();
		auto err = actual_value->deserialize(ctx);
		EXPECT_EQ(err, Error::incomplete_message);
	}
};

// Only serialization
struct SerializeTest : SerializationFixture {};
TEST_P(SerializeTest, get_size) { get_size_test(); }
TEST_P(SerializeTest, serialize) { serialize_test(); }

// Only deserialization
struct DeserializeTest : SerializationFixture {};
TEST_P(DeserializeTest, deserialize) { deserialize_test(); }

// Deserialization + extra/infra space
struct DeserializeSpaceTest : SerializationFixture {};
TEST_P(DeserializeSpaceTest, deserialize) { deserialize_test(); }
TEST_P(DeserializeSpaceTest, deserialize_extra_space) { deserialize_extra_space_test(); }
TEST_P(DeserializeSpaceTest, deserialize_not_enough_space) { deserialize_not_enough_space_test(); }

// Serialization + deserialization
struct SerializeDeserializeTest : SerializationFixture {};
TEST_P(SerializeDeserializeTest, get_size) { get_size_test(); }
TEST_P(SerializeDeserializeTest, serialize) { serialize_test(); }
TEST_P(SerializeDeserializeTest, deserialize) { deserialize_test(); }

// All
struct FullSerializationTest : SerializationFixture {};
TEST_P(FullSerializationTest, get_size) { get_size_test(); }
TEST_P(FullSerializationTest, serialize) { serialize_test(); }
TEST_P(FullSerializationTest, deserialize) { deserialize_test(); }
TEST_P(FullSerializationTest, deserialize_extra_space) { deserialize_extra_space_test(); }
TEST_P(FullSerializationTest, deserialize_not_enough_space) { deserialize_not_enough_space_test(); }


}
}


#endif /* TEST_SERIALIZATION_TEST_COMMON_HPP_ */

#ifndef TEST_SERIALIZATION_TEST_COMMON_HPP_
#define TEST_SERIALIZATION_TEST_COMMON_HPP_

#include "boost/mysql/detail/protocol/messages.hpp"
#include "boost/mysql/detail/protocol/constants.hpp"
#include <gtest/gtest.h>
#include <string>
#include <any>
#include <boost/type_index.hpp>
#include "boost/mysql/detail/protocol/serialization.hpp"
#include "boost/mysql/value.hpp"
#include "test_common.hpp"

namespace boost {
namespace mysql {
namespace detail {

// Operator << for some basic types
template <std::size_t N>
std::ostream& operator<<(std::ostream& os, const std::array<char, N>& v)
{
	return os << v.data();
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
	constexpr auto fields = get_struct_fields<T>::value;
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
std::enable_if_t<is_struct_with_fields<T>::value, bool>
operator==(const T& lhs, const T& rhs)
{
	return equals_struct<0>(lhs, rhs);
}

// Operator << for ValueHolder's
template <typename T>
std::ostream& operator<<(std::ostream& os, const value_holder<T>& value)
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
	constexpr auto fields = get_struct_fields<T>::value;
	if constexpr (index < std::tuple_size<decltype(fields)>::value)
	{
		constexpr auto pmem = std::get<index>(fields);
		os << "    " << (value.*pmem) << ",\n";
		print_struct<index+1>(os, value);
	}
}

template <typename T>
std::enable_if_t<is_struct_with_fields<T>::value, std::ostream&>
operator<<(std::ostream& os, const T& value)
{
	os << boost::typeindex::type_id<T>().pretty_name() << "(\n";
	print_struct<0>(os, value);
	os << ")\n";
	return os;
}

class TypeErasedValue
{
public:
	virtual ~TypeErasedValue() {}
	virtual void serialize(serialization_context& ctx) const = 0;
	virtual std::size_t get_size(const serialization_context& ctx) const = 0;
	virtual errc deserialize(deserialization_context& ctx) = 0;
	virtual std::shared_ptr<TypeErasedValue> default_construct() const = 0;
	virtual bool equals(const TypeErasedValue& rhs) const = 0;
	virtual void print(std::ostream& os) const = 0;

	bool operator==(const TypeErasedValue& rhs) const { return equals(rhs); }
};
inline std::ostream& operator<<(std::ostream& os, const TypeErasedValue& value)
{
	value.print(os);
	return os;
}

template <typename T>
class TypeErasedValueImpl : public TypeErasedValue
{
	T value_;
public:
	TypeErasedValueImpl(const T& v): value_(v) {};
	void serialize(serialization_context& ctx) const override { ::boost::mysql::detail::serialize(value_, ctx); }
	std::size_t get_size(const serialization_context& ctx) const override { return ::boost::mysql::detail::get_size(value_, ctx); }
	errc deserialize(deserialization_context& ctx) override { return ::boost::mysql::detail::deserialize(value_, ctx); }
	std::shared_ptr<TypeErasedValue> default_construct() const override
	{
		return std::make_shared<TypeErasedValueImpl<T>>(T{});
	}
	bool equals(const TypeErasedValue& rhs) const override
	{
		auto typed_value = dynamic_cast<const TypeErasedValueImpl<T>*>(&rhs);
		return typed_value && (typed_value->value_ == value_);
	}
	void print(std::ostream& os) const override
	{
		os << value_;
	}
};

struct SerializeParams : test::named_param
{
	std::shared_ptr<TypeErasedValue> value;
	std::vector<uint8_t> expected_buffer;
	std::string name;
	capabilities caps;
	std::any additional_storage;

	template <typename T>
	SerializeParams(const T& v, std::vector<uint8_t>&& buff,
			        std::string&& name, std::uint32_t caps=0, std::any storage = {}):
		value(std::make_shared<TypeErasedValueImpl<T>>(v)),
		expected_buffer(move(buff)),
		name(move(name)),
		caps(caps),
		additional_storage(std::move(storage))
	{
	}
};

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
	// get_size
	void get_size_test()
	{
		serialization_context ctx (GetParam().caps, nullptr);
		auto size = GetParam().value->get_size(ctx);
		EXPECT_EQ(size, GetParam().expected_buffer.size());
	}

	// serialize
	void serialize_test()
	{
		auto expected_size = GetParam().expected_buffer.size();
		std::vector<uint8_t> buffer (expected_size + 8, 0x7a); // buffer overrun detector
		serialization_context ctx (GetParam().caps, buffer.data());
		GetParam().value->serialize(ctx);

		// Iterator
		EXPECT_EQ(ctx.first(), buffer.data() + expected_size) << "Iterator not updated correctly";

		// Buffer
		std::string_view expected_populated = test::makesv(GetParam().expected_buffer.data(), expected_size);
		std::string_view actual_populated = test::makesv(buffer.data(), expected_size);
		test::compare_buffers(expected_populated, actual_populated, "Buffer contents incorrect");

		// Check for buffer overruns
		std::string expected_clean (8, 0x7a);
		std::string_view actual_clean = test::makesv(buffer.data() + expected_size, 8);
		test::compare_buffers(expected_clean, actual_clean, "Buffer overrun");
	}

	// deserialize
	void deserialize_test()
	{
		auto first = GetParam().expected_buffer.data();
		auto size = GetParam().expected_buffer.size();
		deserialization_context ctx (first, first + size, GetParam().caps);
		auto actual_value = GetParam().value->default_construct();
		auto err = actual_value->deserialize(ctx);

		// No error
		EXPECT_EQ(err, errc::ok);

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
		deserialization_context ctx (first, first + buffer.size(), GetParam().caps);
		auto actual_value = GetParam().value->default_construct();
		auto err = actual_value->deserialize(ctx);

		// No error
		EXPECT_EQ(err, errc::ok);

		// Iterator advanced
		EXPECT_EQ(ctx.first(), first + GetParam().expected_buffer.size());

		// Actual value
		EXPECT_EQ(*actual_value, *GetParam().value);
	}

	void deserialize_not_enough_space_test()
	{
		std::vector<uint8_t> buffer (GetParam().expected_buffer);
		buffer.back() = 0x7a; // try to detect any overruns
		deserialization_context ctx (buffer.data(), buffer.data() + buffer.size() - 1, GetParam().caps);
		auto actual_value = GetParam().value->default_construct();
		auto err = actual_value->deserialize(ctx);
		EXPECT_EQ(err, errc::incomplete_message);
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


// errc tests
struct DeserializeErrorParams : test::named_param
{
	std::shared_ptr<TypeErasedValue> value;
	std::vector<uint8_t> buffer;
	std::string name;
	errc expected_error;

	template <typename T>
	DeserializeErrorParams(
		std::vector<uint8_t>&& buffer,
		std::string&& test_name,
		errc err = errc::incomplete_message
	) :
		value(std::make_shared<TypeErasedValueImpl<T>>(T{})),
		buffer(std::move(buffer)),
		name(std::move(test_name)),
		expected_error(err)
	{
	}
};

struct DeserializeErrorTest : testing::TestWithParam<DeserializeErrorParams> {};

TEST_P(DeserializeErrorTest, Deserialize_ErrorCondition_ReturnsErrorCode)
{
	auto first = GetParam().buffer.data();
	auto last = GetParam().buffer.data() + GetParam().buffer.size();
	deserialization_context ctx (first, last, capabilities(0));
	auto value = GetParam().value->default_construct();
	auto err = value->deserialize(ctx);
	EXPECT_EQ(err, GetParam().expected_error);
}


} // detail
} // mysql
} // boost


#endif /* TEST_SERIALIZATION_TEST_COMMON_HPP_ */

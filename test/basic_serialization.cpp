/*
 * deserialization.cpp
 *
 *  Created on: Jun 29, 2019
 *      Author: ruben
 */

#include "mysql/impl/basic_serialization.hpp"
#include <gtest/gtest.h>
#include <string>
#include <boost/type_index.hpp>

using namespace testing;
using namespace std;
using namespace mysql;
using namespace mysql::detail;

namespace
{

class TypeErasedValue
{
public:
	virtual ~TypeErasedValue() {}
	virtual void serialize(SerializationContext& ctx) const = 0;
	virtual std::size_t get_size(const SerializationContext& ctx) const = 0;
	virtual Error deserialize(DeserializationContext& ctx) = 0;
	virtual string get_type_name() const = 0;
	virtual shared_ptr<TypeErasedValue> default_construct() const = 0;
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
	string get_type_name() const override
	{
		return boost::typeindex::type_id<T>().pretty_name();
	}
	shared_ptr<TypeErasedValue> default_construct() const override
	{
		T res; // intentionally not value-initializing it
		return make_shared<TypeErasedValueImpl<T>>(res);
	}
	bool equals(const TypeErasedValue& rhs) const
	{
		auto typed_value = dynamic_cast<const TypeErasedValueImpl<T>*>(&rhs);
		return typed_value && (typed_value->value_ == value_);
	}
};

struct SerializeParams
{
	shared_ptr<TypeErasedValue> value;
	vector<uint8_t> expected_buffer;
	string test_name;
	bool is_space_sensitive;

	template <typename T>
	SerializeParams(const T& v, vector<uint8_t>&& buff, string&& name="default", bool is_space_sensitive=true):
		value(std::make_shared<TypeErasedValueImpl<T>>(v)),
		expected_buffer(move(buff)),
		test_name(move(name)),
		is_space_sensitive(is_space_sensitive)
	{
	}
};

ostream& operator<<(ostream& os, const SerializeParams& params)
{
	return os << params.value->get_type_name() << " - " << params.test_name;
}

vector<uint8_t> concat(vector<uint8_t>&& lhs, const vector<uint8_t>& rhs)
{
	size_t lhs_size = lhs.size();
	vector<uint8_t> res (move(lhs));
	res.resize(lhs_size + rhs.size());
	memcpy(res.data() + lhs_size, rhs.data(), rhs.size());
	return res;
}

struct SerializeTest : public testing::TestWithParam<SerializeParams>
{
	static string_view make_buffer_view(const uint8_t* buff, size_t sz)
	{
		return string_view(reinterpret_cast<const char*>(buff), sz);
	}
};

TEST_P(SerializeTest, GetSize_Trivial_ReturnsExpectedBufferSize)
{
	SerializationContext ctx (0, nullptr);
	auto size = GetParam().value->get_size(ctx);
	EXPECT_EQ(size, GetParam().expected_buffer.size());
}

TEST_P(SerializeTest, Serialize_Trivial_AdvancesIteratorPopulatesBuffer)
{
	auto expected_size = GetParam().expected_buffer.size();
	vector<uint8_t> buffer (expected_size + 8, 0xaa); // buffer overrun detector
	SerializationContext ctx (0, buffer.data());
	GetParam().value->serialize(ctx);

	// Iterator
	EXPECT_EQ(ctx.first(), buffer.data() + expected_size) << "Iterator not updated correctly";

	// Buffer
	string_view expected_populated = make_buffer_view(GetParam().expected_buffer.data(), expected_size);
	string_view actual_populated = make_buffer_view(buffer.data(), expected_size);
	EXPECT_EQ(expected_populated, actual_populated) << "Buffer contents incorrect";

	// Check for buffer overruns
	string expected_clean (8, 0xaa);
	string_view actual_clean = make_buffer_view(buffer.data() + expected_size, 8);
	EXPECT_EQ(expected_clean, actual_clean) << "Buffer overrun";
}

TEST_P(SerializeTest, Deserialize_ExactSpace_AdvancesIteratorPopulatesValue)
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

TEST_P(SerializeTest, Deserialize_ExtraSpace_AdvancesIteratorPopulatesValue)
{
	if (GetParam().is_space_sensitive)
	{
		vector<uint8_t> buffer (GetParam().expected_buffer);
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
}

TEST_P(SerializeTest, Deserialize_NotEnoughSpace_ReturnsError)
{
	if (GetParam().is_space_sensitive)
	{
		vector<uint8_t> buffer (GetParam().expected_buffer);
		buffer.back() = 0xaa; // try to detect any overruns
		DeserializationContext ctx (buffer.data(), buffer.data() + buffer.size() - 1, 0);
		auto actual_value = GetParam().value->default_construct();
		auto err = actual_value->deserialize(ctx);
		EXPECT_EQ(err, Error::incomplete_message);
	}
}

struct DeserializeErrorParams
{
	shared_ptr<TypeErasedValue> value;
	vector<uint8_t> buffer;
	string test_name;
	Error expected_error;

	template <typename T>
	DeserializeErrorParams create(vector<uint8_t>&& buffer, string&& test_name, Error err = Error::incomplete_message)
	{
		return DeserializeErrorParams {
			make_shared<TypeErasedValueImpl<T>>(T{}),
			move(buffer),
			move(test_name),
			err
		};
	}
};

ostream& operator<<(ostream& os, const DeserializeErrorParams& params)
{
	return os << params.value->get_type_name() << " - " << params.test_name;
}

// Special error conditions
struct DeserializeErrorTest : testing::TestWithParam<DeserializeErrorParams>
{
};

TEST_P(DeserializeErrorTest, Deserialize_ErrorCondition_ReturnsErrorCode)
{
	auto first = GetParam().buffer.data();
	auto last = GetParam().buffer.data() + GetParam().buffer.size();
	DeserializationContext ctx (first, last, 0);
	auto value = GetParam().value->default_construct();
	auto err = value->deserialize(ctx);
	EXPECT_EQ(err, GetParam().expected_error);
}

// Definitions for the parameterized tests
string string_250 (250, 'a');
string string_251 (251, 'a');
string string_ffff (0xffff, 'a');
string string_10000 (0x10000, 'a');

enum class EnumInt1 : uint8_t
{
	value0 = 0,
	value1 = 3,
	value2 = 0xff
};

enum class EnumInt2 : uint16_t
{
	value0 = 0,
	value1 = 3,
	value2 = 0xfeff
};

enum class EnumInt4 : uint32_t
{
	value0 = 0,
	value1 = 3,
	value2 = 0xfcfdfeff
};

INSTANTIATE_TEST_SUITE_P(Default, SerializeTest, ::testing::Values(
	// Unsigned fixed size ints
	SerializeParams(int1{0xff}, {0xff}),
	SerializeParams(int2{0xfeff}, {0xff, 0xfe}),
	SerializeParams(int3{0xfdfeff}, {0xff, 0xfe, 0xfd}),
	SerializeParams(int4{0xfcfdfeff}, {0xff, 0xfe, 0xfd, 0xfc}),
	SerializeParams(int6{0xfafbfcfdfeff}, {0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa}),
	SerializeParams(int8{0xf8f9fafbfcfdfeff}, {0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8}),

	// Signed, fixed size ints
	SerializeParams(int1_signed{-1}, {0xff}, "Negative"),
	SerializeParams(int2_signed{-0x101}, {0xff, 0xfe}, "Negative"),
	SerializeParams(int4_signed{-0x3020101}, {0xff, 0xfe, 0xfd, 0xfc}, "Negative"),
	SerializeParams(int8_signed{-0x0706050403020101}, {0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8}, "Negative"),
	SerializeParams(int1_signed{0x01}, {0x01}, "Positive"),
	SerializeParams(int2_signed{0x0201}, {0x01, 0x02}, "Positive"),
	SerializeParams(int4_signed{0x04030201}, {0x01, 0x02, 0x03, 0x04}, "Positive"),
	SerializeParams(int8_signed{0x0807060504030201}, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}, "Positive"),

	// int_lenenc
	SerializeParams(int_lenenc{1}, {0x01}, "1 byte (regular value)"),
	SerializeParams(int_lenenc{250}, {0xfa}, "1 byte (max value)"),
	SerializeParams(int_lenenc{0xfeb7}, {0xfc, 0xb7, 0xfe}, "2 bytes (regular value)"),
	SerializeParams(int_lenenc{0xffff}, {0xfc, 0xff, 0xff}, "2 bytes (max value)"),
	SerializeParams(int_lenenc{0xa0feff}, {0xfd, 0xff, 0xfe, 0xa0}, "3 bytes (regular value)"),
	SerializeParams(int_lenenc{0xffffff}, {0xfd, 0xff, 0xff, 0xff}, "3 bytes (max value)"),
	SerializeParams(int_lenenc{0xf8f9fafbfcfdfeff},
			{0xfe, 0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8}, "8 bytes (regular value)"),
	SerializeParams(int_lenenc{0xffffffffffffffff},
			{0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, "8 bytes (max value)"),

	// fixed-size string
	SerializeParams(string_fixed<4>{'a', 'b', 'd', 'e'}, {0x61, 0x62, 0x64, 0x65}, "Regular characters"),
	SerializeParams(string_fixed<3>{'\0', '\1', 'a'}, {0x00, 0x01, 0x61}, "Null characters"),
	SerializeParams(string_fixed<3>{char(0xc3), char(0xb1), 'a'}, {0xc3, 0xb1, 0x61}, "UTF-8 characters"),
	SerializeParams(string_fixed<1>{'a'}, {0x61}, "Size 1 string"),

	// null-terminated string
	SerializeParams(string_null{"abc"}, {0x61, 0x62, 0x63, 0x00}, "Regular characters"),
	SerializeParams(string_null{"\xc3\xb1"}, {0xc3, 0xb1, 0x00}, "UTF-8 characters"),
	SerializeParams(string_null{""}, {0x00}, "Empty string"),

	// length-encoded string
	SerializeParams(string_lenenc{""}, {0x00}, "Empty string"),
	SerializeParams(string_lenenc{"abc"}, {0x03, 0x61, 0x62, 0x63}, "1 byte size, regular characters"),
	SerializeParams(string_lenenc{string_view("a\0b", 3)}, {0x03, 0x61, 0x00, 0x62}, "1 byte size, null characters"),
	SerializeParams(string_lenenc{string_250}, concat({250}, vector<uint8_t>(250, 0x61)), "1 byte size, max"),
	SerializeParams(string_lenenc{string_251}, concat({0xfc, 251, 0}, vector<uint8_t>(251, 0x61)), "2 byte size, min"),
	SerializeParams(string_lenenc{string_ffff}, concat({0xfc, 0xff, 0xff}, vector<uint8_t>(0xffff, 0x61)), "2 byte size, max"),
	SerializeParams(string_lenenc{string_10000}, concat({0xfd, 0x00, 0x00, 0x01}, vector<uint8_t>(0x10000, 0x61)), "3 byte size, max"),

	// string eof
	SerializeParams(string_eof{"abc"}, {0x61, 0x62, 0x63}, "Regular characters", false),
	SerializeParams(string_eof{string_view("a\0b", 3)}, {0x61, 0x00, 0x62}, "Null characters", false),
	SerializeParams(string_eof{""}, {}, "Empty string", false),

	// enums
	SerializeParams(EnumInt1::value1, {0x03}, "low value"),
	SerializeParams(EnumInt1::value2, {0xff}, "high value"),
	SerializeParams(EnumInt2::value1, {0x03, 0x00}, "low value"),
	SerializeParams(EnumInt2::value2, {0xff, 0xfe}, "high value"),
	SerializeParams(EnumInt4::value1, {0x03, 0x00, 0x00, 0x00}, "low value"),
	SerializeParams(EnumInt4::value2, {0xff, 0xfe, 0xfd, 0xfc}, "high value")
));



} // anon namespace

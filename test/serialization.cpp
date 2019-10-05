/*
 * deserialization.cpp
 *
 *  Created on: Jun 29, 2019
 *      Author: ruben
 */

#include "serialization_test_common.hpp"

using namespace testing;
using namespace std;
using namespace mysql;
using namespace mysql::detail;

namespace
{

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

INSTANTIATE_TEST_SUITE_P(BasicTypes, SerializeTest, ::testing::Values(
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

INSTANTIATE_TEST_SUITE_P(PacketHeader, SerializeTest, ::testing::Values(
	// packet header
	SerializeParams(msgs::packet_header{{3}, {0}}, {0x03, 0x00, 0x00, 0x00}, "small packet, seqnum==0"),
	SerializeParams(msgs::packet_header{{9}, {2}}, {0x09, 0x00, 0x00, 0x02}, "small packet, seqnum!=0"),
	SerializeParams(msgs::packet_header{{0xcacbcc}, {0xfa}}, {0xcc, 0xcb, 0xca, 0xfa}, "big packet, seqnum!=0"),
	SerializeParams(msgs::packet_header{{0xffffff}, {0xff}}, {0xff, 0xff, 0xff, 0xff}, "max packet, max seqnum")
));

INSTANTIATE_TEST_SUITE_P(OkPacket, SerializeTest, ::testing::Values(
	SerializeParams(msgs::ok_packet{
		{4}, // affected rows
		{0}, // last insert ID
		{SERVER_STATUS_AUTOCOMMIT | SERVER_QUERY_NO_INDEX_USED}, // server status
		{0}, // warnings
		string_lenenc{"Rows matched: 5  Changed: 4  Warnings: 0"}
	}, {
		0x04, 0x00, 0x22, 0x00, 0x00, 0x00, 0x28, 0x52, 0x6f, 0x77, 0x73,
		0x20, 0x6d, 0x61, 0x74, 0x63, 0x68, 0x65, 0x64, 0x3a, 0x20, 0x35, 0x20, 0x20, 0x43, 0x68, 0x61,
		0x6e, 0x67, 0x65, 0x64, 0x3a, 0x20, 0x34, 0x20, 0x20, 0x57, 0x61, 0x72, 0x6e, 0x69, 0x6e, 0x67,
		0x73, 0x3a, 0x20, 0x30
	}, "successful UPDATE", false),

	SerializeParams(msgs::ok_packet{
		{1}, // affected rows
		{6}, // last insert ID
		{SERVER_STATUS_AUTOCOMMIT}, // server status
		{0}, // warnings
		{}  // no message
	},{
		0x01, 0x06, 0x02, 0x00, 0x00, 0x00
	}, "successful INSERT", false)
));

INSTANTIATE_TEST_SUITE_P(ErrPacket, SerializeTest, ::testing::Values(
	SerializeParams(msgs::err_packet{
		{1049}, // eror code
		{0x23}, // sql state marker
		{'4', '2', '0', '0', '0'}, // sql state
		{"Unknown database 'a'"} // err msg
	}, {
		0x19, 0x04, 0x23, 0x34, 0x32, 0x30, 0x30, 0x30, 0x55, 0x6e, 0x6b,
		0x6e, 0x6f, 0x77, 0x6e, 0x20, 0x64, 0x61, 0x74,
		0x61, 0x62, 0x61, 0x73, 0x65, 0x20, 0x27, 0x61, 0x27
	}, "Wrong USE database", false),

	SerializeParams(msgs::err_packet{
		{1146}, // eror code
		{0x23}, // sql state marker
		{'4', '2', 'S', '0', '2'}, // sql state
		{"Table 'awesome.unknown' doesn't exist"} // err msg
	}, {
		0x7a, 0x04, 0x23, 0x34, 0x32, 0x53, 0x30, 0x32,
		0x54, 0x61, 0x62, 0x6c, 0x65, 0x20, 0x27, 0x61,
		0x77, 0x65, 0x73, 0x6f, 0x6d, 0x65, 0x2e, 0x75,
		0x6e, 0x6b, 0x6e, 0x6f, 0x77, 0x6e, 0x27, 0x20,
		0x64, 0x6f, 0x65, 0x73, 0x6e, 0x27, 0x74, 0x20,
		0x65, 0x78, 0x69, 0x73, 0x74
	}, "Unknown table", false)
));


} // anon namespace

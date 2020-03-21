
#include "serialization_test_common.hpp"

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using boost::mysql::errc;

namespace
{

// Test bodies
void get_size_test(const serialization_testcase& p)
{
	serialization_context ctx (p.caps, nullptr);
	auto size = p.value->get_size(ctx);
	EXPECT_EQ(size, p.expected_buffer.size());
}

// serialize
void serialize_test(const serialization_testcase& p)
{
	auto expected_size = p.expected_buffer.size();
	std::vector<uint8_t> buffer (expected_size + 8, 0x7a); // buffer overrun detector
	serialization_context ctx (p.caps, buffer.data());
	p.value->serialize(ctx);

	// Iterator
	EXPECT_EQ(ctx.first(), buffer.data() + expected_size) << "Iterator not updated correctly";

	// Buffer
	std::string_view expected_populated = makesv(p.expected_buffer.data(), expected_size);
	std::string_view actual_populated = makesv(buffer.data(), expected_size);
	compare_buffers(expected_populated, actual_populated, "Buffer contents incorrect");

	// Check for buffer overruns
	std::string expected_clean (8, 0x7a);
	std::string_view actual_clean = makesv(buffer.data() + expected_size, 8);
	compare_buffers(expected_clean, actual_clean, "Buffer overrun");
}

// deserialize
void deserialize_test(const serialization_testcase& p)
{
	auto first = p.expected_buffer.data();
	auto size = p.expected_buffer.size();
	deserialization_context ctx (first, first + size, p.caps);
	auto actual_value = p.value->default_construct();
	auto err = actual_value->deserialize(ctx);

	// No error
	EXPECT_EQ(err, errc::ok);

	// Iterator advanced
	EXPECT_EQ(ctx.first(), first + size);

	// Actual value
	EXPECT_EQ(*actual_value, *p.value);
}

void deserialize_extra_space_test(const serialization_testcase& p)
{
	std::vector<uint8_t> buffer (p.expected_buffer);
	buffer.push_back(0xff);
	auto first = buffer.data();
	deserialization_context ctx (first, first + buffer.size(), p.caps);
	auto actual_value = p.value->default_construct();
	auto err = actual_value->deserialize(ctx);

	// No error
	EXPECT_EQ(err, errc::ok);

	// Iterator advanced
	EXPECT_EQ(ctx.first(), first + p.expected_buffer.size());

	// Actual value
	EXPECT_EQ(*actual_value, *p.value);
}

void deserialize_not_enough_space_test(const serialization_testcase& p)
{
	std::vector<uint8_t> buffer (p.expected_buffer);
	buffer.back() = 0x7a; // try to detect any overruns
	deserialization_context ctx (buffer.data(), buffer.data() + buffer.size() - 1, p.caps);
	auto actual_value = p.value->default_construct();
	auto err = actual_value->deserialize(ctx);
	EXPECT_EQ(err, errc::incomplete_message);
}

// Fixture test definition
TEST_P(SerializeTest, get_size) { get_size_test(GetParam()); }
TEST_P(SerializeTest, serialize) { serialize_test(GetParam()); }

TEST_P(DeserializeTest, deserialize) { deserialize_test(GetParam()); }

TEST_P(DeserializeSpaceTest, deserialize) { deserialize_test(GetParam()); }
TEST_P(DeserializeSpaceTest, deserialize_extra_space) { deserialize_extra_space_test(GetParam()); }
TEST_P(DeserializeSpaceTest, deserialize_not_enough_space) { deserialize_not_enough_space_test(GetParam()); }

TEST_P(SerializeDeserializeTest, get_size) { get_size_test(GetParam()); }
TEST_P(SerializeDeserializeTest, serialize) { serialize_test(GetParam()); }
TEST_P(SerializeDeserializeTest, deserialize) { deserialize_test(GetParam()); }

TEST_P(FullSerializationTest, get_size) { get_size_test(GetParam()); }
TEST_P(FullSerializationTest, serialize) { serialize_test(GetParam()); }
TEST_P(FullSerializationTest, deserialize) { deserialize_test(GetParam()); }
TEST_P(FullSerializationTest, deserialize_extra_space) { deserialize_extra_space_test(GetParam()); }
TEST_P(FullSerializationTest, deserialize_not_enough_space) { deserialize_not_enough_space_test(GetParam()); }

// Errc tests
TEST_P(DeserializeErrorTest, Deserialize_ErrorCondition_ReturnsErrorCode)
{
	auto first = GetParam().buffer.data();
	auto last = GetParam().buffer.data() + GetParam().buffer.size();
	deserialization_context ctx (first, last, capabilities());
	auto value = GetParam().value->default_construct();
	auto err = value->deserialize(ctx);
	EXPECT_EQ(err, GetParam().expected_error);
}

} // anon namespace





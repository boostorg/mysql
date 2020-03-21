/*
 * deserialization.cpp
 *
 *  Created on: Jun 29, 2019
 *      Author: ruben
 */

#include "serialization_test_common.hpp"
#include "test_common.hpp"

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using boost::mysql::errc;
using boost::mysql::collation;
using boost::mysql::value;

namespace
{

// Definitions for the parameterized tests
std::string string_250 (250, 'a');
std::string string_251 (251, 'a');
std::string string_ffff (0xffff, 'a');
std::string string_10000 (0x10000, 'a');

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

INSTANTIATE_TEST_SUITE_P(UnsignedFixedSizeInts, FullSerializationTest, ::testing::Values(
	serialization_testcase(int1(0xff), {0xff}, "int1"),
	serialization_testcase(int2(0xfeff), {0xff, 0xfe}, "int2"),
	serialization_testcase(int3(0xfdfeff), {0xff, 0xfe, 0xfd}, "int3"),
	serialization_testcase(int4(0xfcfdfeff), {0xff, 0xfe, 0xfd, 0xfc}, "int4"),
	serialization_testcase(int6(0xfafbfcfdfeff), {0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa}, "int6"),
	serialization_testcase(int8(0xf8f9fafbfcfdfeff), {0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8}, "int8")
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(SignedFixedSizeInts, FullSerializationTest, ::testing::Values(
	serialization_testcase(int1_signed(-1), {0xff}, "int1_negative"),
	serialization_testcase(int2_signed(-0x101), {0xff, 0xfe}, "int2_negative"),
	serialization_testcase(int4_signed(-0x3020101), {0xff, 0xfe, 0xfd, 0xfc}, "int4_negative"),
	serialization_testcase(int8_signed(-0x0706050403020101),
			{0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8}, "int8_negative"),
	serialization_testcase(int1_signed(0x01), {0x01}, "int1_positive"),
	serialization_testcase(int2_signed(0x0201), {0x01, 0x02}, "int2_positive"),
	serialization_testcase(int4_signed(0x04030201), {0x01, 0x02, 0x03, 0x04}, "int4_positive"),
	serialization_testcase(int8_signed(0x0807060504030201),
			{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}, "int8_positive")
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(LengthEncodedInt, FullSerializationTest, ::testing::Values(
	serialization_testcase(int_lenenc(1), {0x01}, "1_byte_regular"),
	serialization_testcase(int_lenenc(250), {0xfa}, "1_byte_max"),
	serialization_testcase(int_lenenc(0xfeb7), {0xfc, 0xb7, 0xfe}, "2_bytes_regular"),
	serialization_testcase(int_lenenc(0xffff), {0xfc, 0xff, 0xff}, "2_bytes_max"),
	serialization_testcase(int_lenenc(0xa0feff), {0xfd, 0xff, 0xfe, 0xa0}, "3_bytes_regular"),
	serialization_testcase(int_lenenc(0xffffff), {0xfd, 0xff, 0xff, 0xff}, "3_bytes_max"),
	serialization_testcase(int_lenenc(0xf8f9fafbfcfdfeff),
			{0xfe, 0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8}, "8_bytes_regular"),
	serialization_testcase(int_lenenc(0xffffffffffffffff),
			{0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, "8_bytes_max")
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(FixedSizeString, FullSerializationTest, ::testing::Values(
	serialization_testcase(string_fixed<4>{'a', 'b', 'd', 'e'},
			{0x61, 0x62, 0x64, 0x65}, "4c_regular_characters"),
	serialization_testcase(string_fixed<3>{'\0', '\1', 'a'},
			{0x00, 0x01, 0x61}, "3c_null_characters"),
	serialization_testcase(string_fixed<3>{char(0xc3), char(0xb1), 'a'},
			{0xc3, 0xb1, 0x61}, "3c_utf8_characters"),
	serialization_testcase(string_fixed<1>{'a'},
			{0x61}, "1c_regular_characters")
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(NullTerminatedString, FullSerializationTest, ::testing::Values(
	serialization_testcase(string_null("abc"), {0x61, 0x62, 0x63, 0x00}, "regular_characters"),
	serialization_testcase(string_null("\xc3\xb1"), {0xc3, 0xb1, 0x00}, "utf8_characters"),
	serialization_testcase(string_null(""), {0x00}, "empty")
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(LengthEncodedString, FullSerializationTest, ::testing::Values(
	serialization_testcase(string_lenenc(""), {0x00}, "empty"),
	serialization_testcase(string_lenenc("abc"),
			{0x03, 0x61, 0x62, 0x63}, "1_byte_size_regular_characters"),
	serialization_testcase(string_lenenc(std::string_view("a\0b", 3)),
			{0x03, 0x61, 0x00, 0x62}, "1_byte_size_null_characters"),
	serialization_testcase(string_lenenc(string_250),
			concat_copy({250}, std::vector<std::uint8_t>(250, 0x61)), "1_byte_size_max"),
	serialization_testcase(string_lenenc(string_251),
			concat_copy({0xfc, 251, 0}, std::vector<std::uint8_t>(251, 0x61)), "2_byte_size_min"),
	serialization_testcase(string_lenenc(string_ffff),
			concat_copy({0xfc, 0xff, 0xff}, std::vector<std::uint8_t>(0xffff, 0x61)), "2_byte_size_max"),
	serialization_testcase(string_lenenc(string_10000),
			concat_copy({0xfd, 0x00, 0x00, 0x01}, std::vector<std::uint8_t>(0x10000, 0x61)), "3_byte_size_min")
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(EofString, SerializeDeserializeTest, ::testing::Values(
	serialization_testcase(string_eof("abc"), {0x61, 0x62, 0x63}, "regular_characters"),
	serialization_testcase(string_eof(std::string_view("a\0b", 3)), {0x61, 0x00, 0x62}, "null_characters"),
	serialization_testcase(string_eof(""), {}, "empty")
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(Enums, FullSerializationTest, ::testing::Values(
	serialization_testcase(EnumInt1::value1, {0x03}, "int1_low_value"),
	serialization_testcase(EnumInt1::value2, {0xff}, "int1_high_value"),
	serialization_testcase(EnumInt2::value1, {0x03, 0x00}, "int2_low_value"),
	serialization_testcase(EnumInt2::value2, {0xff, 0xfe}, "int2_high_value"),
	serialization_testcase(EnumInt4::value1, {0x03, 0x00, 0x00, 0x00}, "int4_low_value"),
	serialization_testcase(EnumInt4::value2, {0xff, 0xfe, 0xfd, 0xfc}, "int4_high_value")
), test_name_generator);

// Other binary values
INSTANTIATE_TEST_SUITE_P(Float, FullSerializationTest, ::testing::Values(
	serialization_testcase(-4.2f,    {0x66, 0x66, 0x86, 0xc0}, "fractional_negative"),
	serialization_testcase(4.2f,     {0x66, 0x66, 0x86, 0x40}, "fractional_positive"),
	serialization_testcase(3.14e20f, {0x01, 0x2d, 0x88, 0x61}, "positive_exp_positive_fractional"),
	serialization_testcase(0.0f,     {0x00, 0x00, 0x00, 0x00}, "zero")
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(Double, FullSerializationTest, ::testing::Values(
	serialization_testcase(-4.2,     {0xcd, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x10, 0xc0}, "fractional_negative"),
	serialization_testcase(4.2,      {0xcd, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x10, 0x40}, "fractional_positive"),
	serialization_testcase(3.14e200, {0xce, 0x46, 0x3c, 0x76, 0x9c, 0x68, 0x90, 0x69}, "positive_exp_positive_fractional"),
	serialization_testcase(0.0,      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, "zero")
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(Date, FullSerializationTest, ::testing::Values(
	serialization_testcase(makedate(2010, 3, 28), {0x04, 0xda, 0x07, 0x03, 0x1c}, "regular"),
	serialization_testcase(makedate(1000, 1, 1), {0x04, 0xe8, 0x03, 0x01, 0x01}, "min"),
	serialization_testcase(makedate(9999, 12, 31), {0x04, 0x0f, 0x27, 0x0c, 0x1f}, "max")
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(Datetime, FullSerializationTest, ::testing::Values(
	serialization_testcase(makedt(2010, 1, 1),
			{0x04, 0xda, 0x07, 0x01, 0x01}, "only_date"),
	serialization_testcase(makedt(2010, 1, 1, 20,  0,  0,  0),
			{0x07, 0xda, 0x07, 0x01, 0x01, 0x14, 0x00, 0x00}, "date_h"),
	serialization_testcase(makedt(2010, 1, 1, 0,   1,  0,  0),
			{0x07, 0xda, 0x07, 0x01, 0x01, 0x00, 0x01, 0x00}, "date_m"),
	serialization_testcase(makedt(2010, 1, 1, 3,   2,  0,  0),
			{0x07, 0xda, 0x07, 0x01, 0x01, 0x03, 0x02, 0x00}, "date_hm"),
	serialization_testcase(makedt(2010, 1, 1, 0,   0,  1,  0),
			{0x07, 0xda, 0x07, 0x01, 0x01, 0x00, 0x00, 0x01}, "date_s"),
	serialization_testcase(makedt(2010, 1, 1, 0,   59, 1,  0),
			{0x07, 0xda, 0x07, 0x01, 0x01, 0x00, 0x3b, 0x01}, "date_ms"),
	serialization_testcase(makedt(2010, 1, 1, 5,   0,  1,  0),
			{0x07, 0xda, 0x07, 0x01, 0x01, 0x05, 0x00, 0x01}, "date_hs"),
	serialization_testcase(makedt(2010, 1, 1, 23,  1,  59, 0),
			{0x07, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01, 0x3b}, "date_hms"),
	serialization_testcase(makedt(2010, 1, 1, 0,   0,  0,  251000),
			{0x0b, 0xda, 0x07, 0x01, 0x01, 0x00, 0x00, 0x00, 0x78, 0xd4, 0x03, 0x00}, "date_u"),
	serialization_testcase(makedt(2010, 1, 1, 23,  0,  0,  967510),
			{0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x00, 0x00, 0x56, 0xc3, 0x0e, 0x00}, "date_hu"),
	serialization_testcase(makedt(2010, 1, 1, 0,   1,  0,  967510),
			{0x0b, 0xda, 0x07, 0x01, 0x01, 0x00, 0x01, 0x00, 0x56, 0xc3, 0x0e, 0x00}, "date_mu"),
	serialization_testcase(makedt(2010, 1, 1, 23,  1,  0,  967510),
			{0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01, 0x00, 0x56, 0xc3, 0x0e, 0x00}, "date_hmu"),
	serialization_testcase(makedt(2010, 1, 1, 0,   0,  59, 967510),
			{0x0b, 0xda, 0x07, 0x01, 0x01, 0x00, 0x00, 0x3b, 0x56, 0xc3, 0x0e, 0x00}, "date_su"),
	serialization_testcase(makedt(2010, 1, 1, 0,   1,  59, 967510),
			{0x0b, 0xda, 0x07, 0x01, 0x01, 0x00, 0x01, 0x3b, 0x56, 0xc3, 0x0e, 0x00}, "date_msu"),
	serialization_testcase(makedt(2010, 1, 1, 23,  0,  59, 967510),
			{0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x00, 0x3b, 0x56, 0xc3, 0x0e, 0x00}, "date_hsu"),
	serialization_testcase(makedt(2010, 1, 1, 23,  1,  59, 967510),
			{0x0b, 0xda, 0x07, 0x01, 0x01, 0x17, 0x01, 0x3b, 0x56, 0xc3, 0x0e, 0x00}, "date_hmsu")
), test_name_generator);

INSTANTIATE_TEST_SUITE_P(Time, FullSerializationTest, ::testing::Values(
	serialization_testcase(maket(0, 0, 0),
			{0x00}, "zero"),
	serialization_testcase(maket(48,  0,  0, 0),
			{0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, "positive_d"),
	serialization_testcase(maket(21, 0, 0, 0),
			{0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00}, "positive_h"),
    serialization_testcase(maket(0, 40, 0),
    		{0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00}, "positive_m"),
    serialization_testcase(maket(0, 0, 21),
    		{0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15}, "positive_s"),
	serialization_testcase(maket(0, 0, 0, 321000),
			{0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe8, 0xe5, 0x04, 0x00}, "positive_u"),
	serialization_testcase(maket(838, 59, 58, 999000),
			{0x0c, 0x00, 0x22, 0x00, 0x00, 0x00, 0x16, 0x3b, 0x3a, 0x58, 0x3e, 0x0f, 0x00}, "positive_hmsu"),
	serialization_testcase(-maket(48,  0,  0, 0),
			{0x08, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, "negative_d"),
	serialization_testcase(-maket(21, 0, 0, 0),
			{0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00}, "negative_h"),
	serialization_testcase(-maket(0, 40, 0),
			{0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00}, "negative_m"),
	serialization_testcase(-maket(0, 0, 21),
			{0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15}, "negative_s"),
	serialization_testcase(-maket(0, 0, 0, 321000),
			{0x0c, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe8, 0xe5, 0x04, 0x00}, "negative_u"),
	serialization_testcase(-maket(838, 59, 58, 999000),
			{0x0c, 0x01, 0x22, 0x00, 0x00, 0x00, 0x16, 0x3b, 0x3a, 0x58, 0x3e, 0x0f, 0x00}, "negative_hmsu")
), test_name_generator);


} // anon namespace

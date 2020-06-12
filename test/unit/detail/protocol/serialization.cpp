//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

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

enum class EnumInt1 : std::uint8_t
{
    value0 = 0,
    value1 = 3,
    value2 = 0xff
};

enum class EnumInt2 : std::uint16_t
{
    value0 = 0,
    value1 = 3,
    value2 = 0xfeff
};

enum class EnumInt4 : std::uint32_t
{
    value0 = 0,
    value1 = 3,
    value2 = 0xfcfdfeff
};

INSTANTIATE_TEST_SUITE_P(FixedSizeInts, FullSerializationTest, ::testing::Values(
    serialization_testcase(std::uint8_t(0xff), {0xff}, "int1"),
    serialization_testcase(std::uint16_t(0xfeff), {0xff, 0xfe}, "int2"),
    serialization_testcase(int3(0xfdfeff), {0xff, 0xfe, 0xfd}, "int3"),
    serialization_testcase(std::uint32_t(0xfcfdfeff), {0xff, 0xfe, 0xfd, 0xfc}, "int4"),
    serialization_testcase(std::uint64_t(0xf8f9fafbfcfdfeff),
        {0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8}, "int8"),
    serialization_testcase(std::int8_t(0x01), {0x01}, "int1_positive"),
    serialization_testcase(std::int8_t(-1), {0xff}, "int1_negative"),
    serialization_testcase(std::int16_t(0x0201), {0x01, 0x02}, "int2_positive"),
    serialization_testcase(std::int16_t(-0x101), {0xff, 0xfe}, "int2_negative"),
    serialization_testcase(std::int32_t(0x04030201), {0x01, 0x02, 0x03, 0x04}, "int4_positive"),
    serialization_testcase(std::int32_t(-0x3020101), {0xff, 0xfe, 0xfd, 0xfc}, "int4_negative"),
    serialization_testcase(std::int64_t(0x0807060504030201),
            {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}, "int8_positive"),
    serialization_testcase(std::int64_t(-0x0706050403020101),
            {0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8}, "int8_negative")
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
    serialization_testcase(string_lenenc(boost::string_view("a\0b", 3)),
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
    serialization_testcase(string_eof(boost::string_view("a\0b", 3)), {0x61, 0x00, 0x62}, "null_characters"),
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

} // anon namespace

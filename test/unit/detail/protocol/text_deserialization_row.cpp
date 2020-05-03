//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <gtest/gtest.h>
#include "boost/mysql/detail/protocol/text_deserialization.hpp"
#include "test_common.hpp"

// Test deserialize_text_row()

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using namespace testing;
using boost::mysql::value;
using boost::mysql::collation;
using boost::mysql::error_code;
using boost::mysql::errc;

namespace
{

struct DeserializeTextRowTest : public Test
{
    std::vector<boost::mysql::field_metadata> meta {
        column_definition_packet {
            string_lenenc("def"),
            string_lenenc("awesome"),
            string_lenenc("test_table"),
            string_lenenc("test_table"),
            string_lenenc("f0"),
            string_lenenc("f0"),
            collation::utf8_general_ci,
            int4(300),
            protocol_field_type::var_string,
            int2(0),
            int1(0)
        },
        column_definition_packet {
            string_lenenc("def"),
            string_lenenc("awesome"),
            string_lenenc("test_table"),
            string_lenenc("test_table"),
            string_lenenc("f1"),
            string_lenenc("f1"),
            collation::binary,
            int4(11),
            protocol_field_type::long_,
            int2(0),
            int1(0)
        },
        column_definition_packet {
            string_lenenc("def"),
            string_lenenc("awesome"),
            string_lenenc("test_table"),
            string_lenenc("test_table"),
            string_lenenc("f2"),
            string_lenenc("f2"),
            collation::binary,
            int4(22),
            protocol_field_type::datetime,
            int2(column_flags::binary),
            int1(2)
        }
    };
    std::vector<value> values;

    error_code deserialize(const std::vector<std::uint8_t>& buffer)
    {
        deserialization_context ctx (buffer.data(), buffer.data() + buffer.size(), capabilities());
        return deserialize_text_row(ctx, meta, values);
    }
};

TEST_F(DeserializeTextRowTest, SameNumberOfValuesAsFieldsNonNulls_DeserializesReturnsOk)
{
    std::vector<value> expected_values {value("val"), value(std::int32_t(21)), value(makedt(2010, 10, 1))};
    std::vector<std::uint8_t> buffer {
        0x03, 0x76, 0x61, 0x6c, 0x02, 0x32, 0x31, 0x16,
        0x32, 0x30, 0x31, 0x30, 0x2d, 0x31, 0x30, 0x2d,
        0x30, 0x31, 0x20, 0x30, 0x30, 0x3a, 0x30, 0x30,
        0x3a, 0x30, 0x30, 0x2e, 0x30, 0x30
    };
    auto err = deserialize(buffer);
    EXPECT_EQ(err, error_code());
    EXPECT_EQ(values, expected_values);
}

TEST_F(DeserializeTextRowTest, SameNumberOfValuesAsFieldsOneNull_DeserializesReturnsOk)
{
    std::vector<value> expected_values {value("val"), value(nullptr), value(makedt(2010, 10, 1))};
    std::vector<std::uint8_t> buffer {
        0x03, 0x76, 0x61, 0x6c, 0xfb, 0x16, 0x32, 0x30,
        0x31, 0x30, 0x2d, 0x31, 0x30, 0x2d, 0x30, 0x31,
        0x20, 0x30, 0x30, 0x3a, 0x30, 0x30, 0x3a, 0x30,
        0x30, 0x2e, 0x30, 0x30
    };
    auto err = deserialize(buffer);
    EXPECT_EQ(err, error_code());
    EXPECT_EQ(values, expected_values);
}

TEST_F(DeserializeTextRowTest, SameNumberOfValuesAsFieldsAllNull_DeserializesReturnsOk)
{
    std::vector<value> expected_values {value(nullptr), value(nullptr), value(nullptr)};
    auto err = deserialize({0xfb, 0xfb, 0xfb});
    EXPECT_EQ(err, error_code());
    EXPECT_EQ(values, expected_values);
}

TEST_F(DeserializeTextRowTest, TooFewValues_ReturnsError)
{
    auto err = deserialize({0xfb, 0xfb});
    EXPECT_EQ(err, make_error_code(errc::incomplete_message));
}

TEST_F(DeserializeTextRowTest, TooManyValues_ReturnsError)
{
    auto err = deserialize({0xfb, 0xfb, 0xfb, 0xfb});
    EXPECT_EQ(err, make_error_code(errc::extra_bytes));
}

TEST_F(DeserializeTextRowTest, ErrorDeserializingContainerStringValue_ReturnsError)
{
    auto err = deserialize({0x03, 0xaa, 0xab, 0xfb, 0xfb});
    EXPECT_EQ(err, make_error_code(errc::incomplete_message));
}

TEST_F(DeserializeTextRowTest, ErrorDeserializingContainerValue_ReturnsError)
{
    std::vector<std::uint8_t> buffer {
        0x03, 0x76, 0x61, 0x6c, 0xfb, 0x16, 0x32, 0x30,
        0x31, 0x30, 0x2d, 0x31, 0x30, 0x2d, 0x30, 0x31,
        0x20, 0x30, 0x30, 0x3a, 0x30, 0x30, 0x3a, 0x30,
        0x30, 0x2f, 0x30, 0x30
    };
    auto err = deserialize(buffer);
    EXPECT_EQ(err, make_error_code(errc::protocol_value_error));
}

} // anon namespace

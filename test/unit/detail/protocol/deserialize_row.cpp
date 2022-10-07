//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Tests for both deserialize_binary_row() and deserialize_text_row()

#include <boost/mysql/detail/protocol/deserialize_row.hpp>
#include <boost/test/tools/context.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/data/monomorphic/collection.hpp>
#include <boost/test/data/test_case.hpp>
#include "boost/mysql/detail/auxiliar/string_view_offset.hpp"
#include "boost/mysql/detail/protocol/capabilities.hpp"
#include "boost/mysql/detail/protocol/common_messages.hpp"
#include "boost/mysql/detail/protocol/constants.hpp"
#include "boost/mysql/detail/protocol/resultset_encoding.hpp"
#include "boost/mysql/field_view.hpp"
#include "boost/mysql/metadata.hpp"
#include "test_common.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql::detail;
using boost::mysql::field_view;
using boost::mysql::collation;
using boost::mysql::error_code;
using boost::mysql::errc;
using boost::mysql::metadata;

namespace
{

BOOST_AUTO_TEST_SUITE(test_deserialize_row)

std::vector<metadata> make_meta(
    const std::vector<protocol_field_type>& types
)
{
    std::vector<metadata> res;
    for (const auto type: types)
    {
        column_definition_packet coldef {};
        coldef.type = type;
        res.emplace_back(coldef, false);
    }
    return res;
}

BOOST_AUTO_TEST_CASE(no_resultset_success)
{
    struct
    {
        const char* name;
        resultset_encoding encoding;
        std::vector<std::uint8_t> from;
        std::vector<field_view> expected_with_offsets; // before offset conversion
        std::vector<field_view> expected; // after offset conversion
        std::vector<metadata> meta;
    } test_cases [] = {
        // Text
        {
            "text_one_value",
            resultset_encoding::text,
            {0x01, 0x35},
            make_fv_vector(std::int64_t(5)),
            make_fv_vector(std::int64_t(5)),
            make_meta({ protocol_field_type::tiny })
        },
        {
            "text_one_null",
            resultset_encoding::text,
            {0xfb},
            make_fv_vector(nullptr),
            make_fv_vector(nullptr),
            make_meta({ protocol_field_type::tiny })
        },
        {
            "text_several_values",
            resultset_encoding::text,
            {0x03, 0x76, 0x61, 0x6c, 0x02, 0x32, 0x31, 0x03, 0x30, 0x2e, 0x30},
            make_fv_vector(string_view_offset(1, 3), std::int64_t(21), 0.0f),
            make_fv_vector("val", std::int64_t(21), 0.0f),
            make_meta({ protocol_field_type::var_string, protocol_field_type::long_, protocol_field_type::float_ })
        },
        {
            "text_several_values_one_null",
            resultset_encoding::text,
            {0x03, 0x76, 0x61, 0x6c, 0xfb, 0x03, 0x76, 0x61, 0x6c},
            make_fv_vector(string_view_offset(1, 3), nullptr, string_view_offset(6, 3)),
            make_fv_vector("val", nullptr, "val"),
            make_meta({ protocol_field_type::var_string, protocol_field_type::long_, protocol_field_type::varchar })
        },
        {
            "text_several_nulls",
            resultset_encoding::text,
            {0xfb, 0xfb, 0xfb},
            make_fv_vector(nullptr, nullptr, nullptr),
            make_fv_vector(nullptr, nullptr, nullptr),
            make_meta({ protocol_field_type::var_string, protocol_field_type::long_, protocol_field_type::datetime })
        },

        // Binary
        {
            "binary_one_value",
            resultset_encoding::binary,
            {0x00, 0x00, 0x14},
            make_fv_vector(std::int64_t(20)),
            make_fv_vector(std::int64_t(20)),
            make_meta({ protocol_field_type::tiny })
        },
        {
            "binary_one_null",
            resultset_encoding::binary,
            {0x00, 0x04},
            make_fv_vector(nullptr),
            make_fv_vector(nullptr),
            make_meta({ protocol_field_type::tiny })
        },
        {
            "binary_two_values",
            resultset_encoding::binary,
            {0x00, 0x00, 0x03, 0x6d, 0x69, 0x6e, 0x6d, 0x07},
            make_fv_vector(string_view_offset(3, 3), std::int64_t(1901)),
            make_fv_vector("min", std::int64_t(1901)),
            make_meta({ protocol_field_type::var_string, protocol_field_type::short_ })
        },
        {
            "binary_one_value_one_null",
            resultset_encoding::binary,
            {0x00, 0x08, 0x03, 0x6d, 0x61, 0x78},
            make_fv_vector(string_view_offset(3, 3), nullptr),
            make_fv_vector("max", nullptr),
            make_meta({ protocol_field_type::var_string, protocol_field_type::tiny })
        },
        {
            "binary_two_nulls",
            resultset_encoding::binary,
            {0x00, 0x0c},
            make_fv_vector(nullptr, nullptr),
            make_fv_vector(nullptr, nullptr),
            make_meta({ protocol_field_type::tiny, protocol_field_type::tiny })
        },
        {
            "binary_six_nulls",
            resultset_encoding::binary,
            {0x00, 0xfc},
            std::vector<field_view>(6, field_view()),
            std::vector<field_view>(6, field_view()),
            make_meta(std::vector<protocol_field_type>(6, protocol_field_type::tiny))
        },
        {
            "binary_seven_nulls",
            resultset_encoding::binary,
            {0x00, 0xfc, 0x01},
            std::vector<field_view>(7, field_view()),
            std::vector<field_view>(7, field_view()),
            make_meta(std::vector<protocol_field_type>(7, protocol_field_type::tiny))
        },
        {
            "binary_several_values",
            resultset_encoding::binary,
            {
                0x00, 0x90, 0x00, 0xfd, 0x03, 0x61, 0x62, 0x63,
                0xc3, 0xf5, 0x48, 0x40, 0x02, 0x61, 0x62, 0x04,
                0xe2, 0x07, 0x0a, 0x05, 0x71, 0x99, 0x6d, 0xe2,
                0x93, 0x4d, 0xf5, 0x3d
            },
            make_fv_vector(
                std::int64_t(-3),
                string_view_offset(5, 3),
                nullptr,
                3.14f,
                string_view_offset(13, 2),
                nullptr,
                makedate(2018, 10, 5),
                3.10e-10
            ),
            make_fv_vector(
                std::int64_t(-3),
                "abc",
                nullptr,
                3.14f,
                "ab",
                nullptr,
                makedate(2018, 10, 5),
                3.10e-10
            ),
            make_meta({
                protocol_field_type::tiny,
                protocol_field_type::var_string,
                protocol_field_type::long_,
                protocol_field_type::float_,
                protocol_field_type::string,
                protocol_field_type::long_,
                protocol_field_type::date,
                protocol_field_type::double_
            })
        }
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            const auto& buffer = tc.from;
            deserialization_context ctx (buffer.data(), buffer.data() + buffer.size(), capabilities());
            std::vector<field_view> actual;
            error_code err;

            deserialize_row(tc.encoding, ctx, tc.meta, tc.from.data(), actual, err);
            BOOST_TEST(err == error_code());
            BOOST_TEST(actual == tc.expected_with_offsets);

            offsets_to_string_views(actual, tc.from.data());
            BOOST_TEST(actual == tc.expected);
        }
    }
}


BOOST_AUTO_TEST_CASE(no_resultset_error)
{
    struct
    {
        const char* name;
        resultset_encoding encoding;
        std::vector<std::uint8_t> from;
        errc expected;
        std::vector<metadata> meta;
    } test_cases [] = {
        // text
        {
            "text_no_space_string_single",
            resultset_encoding::text,
            {0x02, 0x00},
            errc::incomplete_message,
            make_meta({ protocol_field_type::short_} )
        },
        {
            "text_no_space_string_final",
            resultset_encoding::text,
            {0x01, 0x35, 0x02, 0x35},
            errc::incomplete_message,
            make_meta({ protocol_field_type::tiny, protocol_field_type::short_ }),
        },
        {
            "text_no_space_null_single",
            resultset_encoding::text,
            {},
            errc::incomplete_message,
            make_meta({ protocol_field_type::tiny })
        },
        {
            "text_no_space_null_final",
            resultset_encoding::text,
            {0xfb},
            errc::incomplete_message,
            make_meta({protocol_field_type::tiny, protocol_field_type::tiny}),
        },
        {
            "text_extra_bytes",
            resultset_encoding::text,
            {0x01, 0x35, 0xfb, 0x00},
            errc::extra_bytes,
            make_meta({ protocol_field_type::tiny, protocol_field_type::tiny })
        },
        {
            "text_contained_value_error_single",
            resultset_encoding::text,
            {0x01, 0x00},
            errc::protocol_value_error,
            make_meta({ protocol_field_type::date })
        },
        {
            "text_contained_value_error_middle",
            resultset_encoding::text,
            {0xfb, 0x01, 0x00, 0xfb},
            errc::protocol_value_error,
            make_meta({ protocol_field_type::date, protocol_field_type::date, protocol_field_type::date })
        },

        // binary
        {
            "binary_no_space_null_bitmap_1",
            resultset_encoding::binary,
            {0x00},
            errc::incomplete_message,
            make_meta({ protocol_field_type::tiny })
        },
        {
            "binary_no_space_null_bitmap_2",
            resultset_encoding::binary,
            {0x00, 0xfc},
            errc::incomplete_message,
            make_meta(std::vector<protocol_field_type>(7, protocol_field_type::tiny))
        },
        {
            "binary_no_space_value_single",
            resultset_encoding::binary,
            {0x00, 0x00},
            errc::incomplete_message,
            make_meta({ protocol_field_type::tiny })
        },
        {
            "binary_no_space_value_last",
            resultset_encoding::binary,
            {0x00, 0x00, 0x01},
            errc::incomplete_message,
            make_meta(std::vector<protocol_field_type>(2, protocol_field_type::tiny))
        },
        {
            "binary_no_space_value_middle",
            resultset_encoding::binary,
            {0x00, 0x00, 0x01},
            errc::incomplete_message,
            make_meta(std::vector<protocol_field_type>(3, protocol_field_type::tiny))
        },
        {
            "binary_extra_bytes",
            resultset_encoding::binary,
            {0x00, 0x00, 0x01, 0x02},
            errc::extra_bytes,
            make_meta({ protocol_field_type::tiny })
        }
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            const auto& buffer = tc.from;
            deserialization_context ctx (buffer.data(), buffer.data() + buffer.size(), capabilities());
            std::vector<field_view> actual;
            error_code err;

            deserialize_row(tc.encoding, ctx, tc.meta, buffer.data(), actual, err);
            BOOST_TEST(err == error_code(tc.expected));
        }
    }
}


BOOST_AUTO_TEST_SUITE_END()

}

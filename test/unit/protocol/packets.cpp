//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "protocol/packets.hpp"

#include <boost/test/unit_test.hpp>

#include "operators.hpp"
#include "protocol/constants.hpp"
#include "protocol/protocol.hpp"
#include "serialization_test.hpp"
#include "test_unit/create_ok.hpp"

using namespace boost::mysql::detail;
using namespace boost::mysql::test;

BOOST_AUTO_TEST_SUITE(test_packets)

// ok_view
BOOST_AUTO_TEST_CASE(ok_view_success)
{
    struct
    {
        const char* name;
        ok_view expected;
        std::vector<std::uint8_t> serialized;
    } test_cases[] = {
        {
         "successful_update",      ok_builder()
                .affected_rows(4)
                .last_insert_id(0)
                .flags(SERVER_STATUS_AUTOCOMMIT | SERVER_QUERY_NO_INDEX_USED)
                .warnings(0)
                .info("Rows matched: 5  Changed: 4  Warnings: 0")
                .build(),
         {0x04, 0x00, 0x22, 0x00, 0x00, 0x00, 0x28, 0x52, 0x6f, 0x77, 0x73, 0x20, 0x6d, 0x61, 0x74, 0x63,
             0x68, 0x65, 0x64, 0x3a, 0x20, 0x35, 0x20, 0x20, 0x43, 0x68, 0x61, 0x6e, 0x67, 0x65, 0x64, 0x3a,
             0x20, 0x34, 0x20, 0x20, 0x57, 0x61, 0x72, 0x6e, 0x69, 0x6e, 0x67, 0x73, 0x3a, 0x20, 0x30},
         },
        {
         "successful_insert",                        ok_builder()
                .affected_rows(1)
                .last_insert_id(6)
                .flags(SERVER_STATUS_AUTOCOMMIT)
                .warnings(0)
                .info("")
                .build(),
         {0x01, 0x06, 0x02, 0x00, 0x00, 0x00},
         },
        {
         "successful_login", ok_builder()
                .affected_rows(0)
                .last_insert_id(0)
                .flags(SERVER_STATUS_AUTOCOMMIT)
                .warnings(0)
                .info("")
                .build(),
         {0x00, 0x00, 0x02, 0x00, 0x00, 0x00},
         }
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            auto first = tc.serialized.data();
            auto size = tc.serialized.size();
            deserialization_context ctx(first, first + size);
            ok_view actual{};
            deserialize_errc err = deserialize(ctx, actual);

            // No error
            BOOST_TEST(err == deserialize_errc::ok);

            // Iterator advanced
            BOOST_TEST(ctx.first() == first + size);

            // Actual value
            BOOST_TEST(actual.affected_rows == tc.expected.affected_rows);
            BOOST_TEST(actual.last_insert_id == tc.expected.last_insert_id);
            BOOST_TEST(actual.status_flags == tc.expected.status_flags);
            BOOST_TEST(actual.warnings == tc.expected.warnings);
            BOOST_TEST(actual.info == tc.expected.info);
        }
    }
}

BOOST_AUTO_TEST_CASE(ok_view_error)
{
    struct
    {
        const char* name;
        std::vector<std::uint8_t> serialized;
    } test_cases[] = {
        {"empty",                {}                                        },
        {"error_affected_rows",  {0xff}                                    },
        {"error_last_insert_id", {0x01, 0xff}                              },
        {"error_last_insert_id", {0x01, 0x06, 0x02}                        },
        {"error_warnings",       {0x01, 0x06, 0x02, 0x00, 0x00}            },
        {"error_info",           {0x04, 0x00, 0x22, 0x00, 0x00, 0x00, 0x28}},
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            deserialization_context ctx(tc.serialized.data(), tc.serialized.data() + tc.serialized.size());

            ok_view value{};
            deserialize_errc err = deserialize(ctx, value);
            BOOST_TEST(err == deserialize_errc::incomplete_message);
        }
    }
}

// error packet
BOOST_AUTO_TEST_CASE(err_view_success)
{
    struct
    {
        const char* name;
        err_view expected;
        std::vector<std::uint8_t> serialized;
    } test_cases[] = {
        {
         "wrong_use_database",                             {1049, "Unknown database 'a'"},
         {0x19, 0x04, 0x23, 0x34, 0x32, 0x30, 0x30, 0x30, 0x55, 0x6e, 0x6b, 0x6e, 0x6f, 0x77,
             0x6e, 0x20, 0x64, 0x61, 0x74, 0x61, 0x62, 0x61, 0x73, 0x65, 0x20, 0x27, 0x61, 0x27},
         },
        {
         "unknown_table",                                                {1146, "Table 'awesome.unknown' doesn't exist"},
         {0x7a, 0x04, 0x23, 0x34, 0x32, 0x53, 0x30, 0x32, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x20, 0x27,
             0x61, 0x77, 0x65, 0x73, 0x6f, 0x6d, 0x65, 0x2e, 0x75, 0x6e, 0x6b, 0x6e, 0x6f, 0x77, 0x6e,
             0x27, 0x20, 0x64, 0x6f, 0x65, 0x73, 0x6e, 0x27, 0x74, 0x20, 0x65, 0x78, 0x69, 0x73, 0x74},
         },
        {
         "failed_login",{1045, "Access denied for user 'root'@'localhost' (using password: YES)"},
         {0x15, 0x04, 0x23, 0x32, 0x38, 0x30, 0x30, 0x30, 0x41, 0x63, 0x63, 0x65, 0x73, 0x73, 0x20,
             0x64, 0x65, 0x6e, 0x69, 0x65, 0x64, 0x20, 0x66, 0x6f, 0x72, 0x20, 0x75, 0x73, 0x65, 0x72,
             0x20, 0x27, 0x72, 0x6f, 0x6f, 0x74, 0x27, 0x40, 0x27, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x68,
             0x6f, 0x73, 0x74, 0x27, 0x20, 0x28, 0x75, 0x73, 0x69, 0x6e, 0x67, 0x20, 0x70, 0x61, 0x73,
             0x73, 0x77, 0x6f, 0x72, 0x64, 0x3a, 0x20, 0x59, 0x45, 0x53, 0x29},
         },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            auto first = tc.serialized.data();
            auto size = tc.serialized.size();
            deserialization_context ctx(first, first + size);
            err_view actual{};
            deserialize_errc err = deserialize(ctx, actual);

            // No error
            BOOST_TEST(err == deserialize_errc::ok);

            // Iterator advanced
            BOOST_TEST(ctx.first() == first + size);

            // Actual value
            BOOST_TEST(actual.error_code == tc.expected.error_code);
            BOOST_TEST(actual.error_message == tc.expected.error_message);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

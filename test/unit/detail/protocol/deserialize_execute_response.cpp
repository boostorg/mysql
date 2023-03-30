//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/protocol/capabilities.hpp>
#include <boost/mysql/detail/protocol/db_flavor.hpp>
#include <boost/mysql/detail/protocol/deserialize_execute_response.hpp>

#include <boost/test/unit_test.hpp>

#include "test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::client_errc;
using boost::mysql::common_server_errc;
using boost::mysql::diagnostics;
using boost::mysql::error_code;
using boost::mysql::detail::capabilities;
using boost::mysql::detail::db_flavor;
using boost::mysql::detail::execute_response;

BOOST_TEST_DONT_PRINT_LOG_VALUE(execute_response::type_t)

namespace {

BOOST_AUTO_TEST_SUITE(test_deserialize_execute_response)

BOOST_AUTO_TEST_CASE(ok_packet)
{
    std::uint8_t msg[] = {0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00};
    diagnostics diag;
    auto response = deserialize_execute_response(
        boost::asio::buffer(msg),
        capabilities(),
        db_flavor::mariadb,
        diag
    );
    BOOST_TEST(response.type == execute_response::type_t::ok_packet);
    BOOST_TEST(response.data.ok_pack.affected_rows.value == 0u);
    BOOST_TEST(response.data.ok_pack.status_flags == 2u);
}

BOOST_AUTO_TEST_CASE(num_fields)
{
    struct
    {
        const char* name;
        std::vector<std::uint8_t> msg;
        std::size_t num_fields;
    } test_cases[] = {
        {"1",                    {0x01},             1     },
        {"0xfa",                 {0xfa},             0xfa  },
        {"0xfb_no_local_infile", {0xfb},             0xfb  }, // legal when LOCAL INFILE capability not enabled
        {"0xfb_local_infile",    {0xfc, 0xfb, 0x00}, 0xfb  }, // sent LOCAL INFILE capability is enabled
        {"0xff",                 {0xfc, 0xff, 0x00}, 0xff  },
        {"0x01ff",               {0xfc, 0x00, 0x01}, 0x01ff},
        {"max",                  {0xfc, 0xff, 0xff}, 0xffff},
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            std::uint8_t msg[] = {0xfc, 0xff, 0x00};
            diagnostics diag;
            auto response = deserialize_execute_response(
                boost::asio::buffer(msg),
                capabilities(),
                db_flavor::mysql,
                diag
            );
            BOOST_TEST(response.type == execute_response::type_t::num_fields);
            BOOST_TEST(response.data.num_fields == 0xffu);
            BOOST_TEST(diag.server_message() == "");
        }
    }
}

BOOST_AUTO_TEST_CASE(error)
{
    struct
    {
        const char* name;
        std::vector<std::uint8_t> msg;
        error_code err;
        const char* expected_info;
    } test_cases[] = {
        {"server_error",
         {0xff, 0x7a, 0x04, 0x23, 0x34, 0x32, 0x53, 0x30, 0x32, 0x54, 0x61, 0x62, 0x6c, 0x65,
          0x20, 0x27, 0x6d, 0x79, 0x74, 0x65, 0x73, 0x74, 0x2e, 0x61, 0x62, 0x63, 0x27, 0x20,
          0x64, 0x6f, 0x65, 0x73, 0x6e, 0x27, 0x74, 0x20, 0x65, 0x78, 0x69, 0x73, 0x74},
         common_server_errc::er_no_such_table,
         "Table 'mytest.abc' doesn't exist"                                                                                   },
        {"bad_server_error", {0xff, 0x00},                                               client_errc::incomplete_message,   ""},
        {"bad_ok_packet",    {0x00, 0xff},                                               client_errc::incomplete_message,   ""},
        {"bad_num_fields",   {0xfc, 0xff, 0x00, 0x01},                                   client_errc::extra_bytes,          ""},
        {"zero_num_fields",  {0xfc, 0x00, 0x00},                                         client_errc::protocol_value_error, ""},
        {"3byte_integer",    {0xfd, 0xff, 0xff, 0xff},                                   client_errc::protocol_value_error, ""},
        {"8byte_integer",
         {0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
         client_errc::protocol_value_error,
         ""                                                                                                                   },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            diagnostics diag;
            auto response = deserialize_execute_response(
                boost::asio::buffer(tc.msg),
                capabilities(),
                db_flavor::mysql,
                diag
            );
            BOOST_TEST(response.type == execute_response::type_t::error);
            BOOST_TEST(response.data.err == tc.err);
            BOOST_TEST(diag.server_message() == tc.expected_info);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
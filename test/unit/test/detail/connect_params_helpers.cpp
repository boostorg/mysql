//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/connect_params_helpers.hpp>

#include <boost/test/unit_test.hpp>

#include "test_common/printing.hpp"
#include "test_unit/printing.hpp"

using namespace boost::mysql::detail;
using boost::mysql::address_type;
using boost::mysql::connect_params;
using boost::mysql::ssl_mode;
using boost::mysql::string_view;

BOOST_AUTO_TEST_SUITE(test_connect_params_helpers)

BOOST_AUTO_TEST_CASE(adjust_ssl_mode_)
{
    struct
    {
        string_view name;
        address_type addr_type;
        ssl_mode input;
        ssl_mode expected;
    } test_cases[] = {
        {"tcp_disable",  address_type::host_and_port, ssl_mode::disable, ssl_mode::disable},
        {"tcp_enable",   address_type::host_and_port, ssl_mode::enable,  ssl_mode::enable },
        {"tcp_require",  address_type::host_and_port, ssl_mode::require, ssl_mode::require},
        {"unix_disable", address_type::unix_path,     ssl_mode::disable, ssl_mode::disable},
        {"unix_enable",  address_type::unix_path,     ssl_mode::enable,  ssl_mode::disable},
        {"unix_require", address_type::unix_path,     ssl_mode::require, ssl_mode::disable},
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            auto actual = adjust_ssl_mode(tc.input, tc.addr_type);
            BOOST_TEST(actual == tc.expected);
        }
    }
}

BOOST_AUTO_TEST_CASE(make_stable_strings)
{
    struct
    {
        string_view name;
        address_type addr_type;
        string_view address;
        string_view username;
        string_view password;
        string_view database;
    } test_cases[] = {
        {"all_nonempty",   address_type::host_and_port, "myhost",    "myuser",                            "mypasswd", "mydb"                 },
        {"host_empty",     address_type::host_and_port, "",          "myuser",                            "mypasswd", "mydb"                 },
        {"username_empty", address_type::host_and_port, "myhost",    "",                                  "mypasswd", "mydb"                 },
        {"passwd_empty",   address_type::host_and_port, "myhost",    "myuser",                            "",         "mydb"                 },
        {"db_empty",       address_type::host_and_port, "myhost",    "myuser",                            "mypasswd", ""                     },
        {"long",
         address_type::host_and_port,
         "kjasdhkahhkasklchnasklscc",                                "jbhi832askjcbakjcbnakjsbajbkjaakj",
         "mypasswd",                                                                                                  "ajbhu82329eckdsckjscb"},
        {"all_empty",      address_type::host_and_port, "",          "",                                  "",         ""                     },
        {"unix_nonempty",  address_type::unix_path,     "/var/sock", "myuser",                            "mypasswd", "mydb"                 },
        {"unix_empty",     address_type::unix_path,     "",          "myuser",                            "mypasswd", "mydb"                 },
        {"unix_all_empty", address_type::unix_path,     "",          "",                                  "",         ""                     },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            connect_params input;
            if (tc.addr_type == address_type::host_and_port)
                input.server_address.emplace_host_and_port(tc.address, 1000);
            else
                input.server_address.emplace_unix_path(tc.address);
            input.username = tc.username;
            input.password = tc.password;
            input.database = tc.database;

            auto stable = make_stable(input);

            input.server_address.emplace_host_and_port("otherh", 2000);
            input.username = "user22";
            input.password = "passwd22";
            input.database = "db22";

            auto addr = stable.address;
            auto hparams = stable.hparams;
            auto buff = std::move(stable.string_buffer);
            stable = make_stable(connect_params{});

            auto expected_port = tc.addr_type == address_type::host_and_port ? std::uint16_t(1000)
                                                                             : std::uint16_t(0);
            BOOST_TEST(addr.address == tc.address);
            BOOST_TEST(addr.port == expected_port);
            BOOST_TEST(addr.type == tc.addr_type);
            BOOST_TEST(hparams.username() == tc.username);
            BOOST_TEST(hparams.password() == tc.password);
            BOOST_TEST(hparams.database() == tc.database);
        }
    }
}

BOOST_AUTO_TEST_CASE(make_stable_1)
{
    connect_params input;
    input.server_address.emplace_host_and_port("myhost", 2000);
    input.username = "myuser";
    input.password = "mypass";
    input.database = "mydb";
    input.connection_collation = std::uint16_t(100);
    input.ssl = ssl_mode::require;
    input.multi_queries = true;

    auto stable = make_stable(input);

    BOOST_TEST(stable.address.address == "myhost");
    BOOST_TEST(stable.address.port == std::uint16_t(2000));
    BOOST_TEST(stable.address.type == address_type::host_and_port);
    BOOST_TEST(stable.hparams.username() == "myuser");
    BOOST_TEST(stable.hparams.password() == "mypass");
    BOOST_TEST(stable.hparams.database() == "mydb");
    BOOST_TEST(stable.hparams.connection_collation() == std::uint16_t(100));
    BOOST_TEST(stable.hparams.ssl() == ssl_mode::require);
    BOOST_TEST(stable.hparams.multi_queries());
}

BOOST_AUTO_TEST_CASE(make_stable_2)
{
    connect_params input;
    input.server_address.emplace_unix_path("/var/sock");
    input.username = "myuser2";
    input.password = "mypass2";
    input.database = "mydb2";
    input.connection_collation = std::uint16_t(200);
    input.ssl = ssl_mode::enable;
    input.multi_queries = false;

    auto stable = make_stable(input);

    BOOST_TEST(stable.address.address == "/var/sock");
    BOOST_TEST(stable.address.port == std::uint16_t(0));
    BOOST_TEST(stable.address.type == address_type::unix_path);
    BOOST_TEST(stable.hparams.username() == "myuser2");
    BOOST_TEST(stable.hparams.password() == "mypass2");
    BOOST_TEST(stable.hparams.database() == "mydb2");
    BOOST_TEST(stable.hparams.connection_collation() == std::uint16_t(200));
    BOOST_TEST(stable.hparams.ssl() == ssl_mode::disable);  // SSL mode was adjusted (UNIX)
    BOOST_TEST(!stable.hparams.multi_queries());
}

BOOST_AUTO_TEST_CASE(make_hparams_1)
{
    connect_params input;
    input.server_address.emplace_host_and_port("myhost", 2000);
    input.username = "myuser";
    input.password = "mypass";
    input.database = "mydb";
    input.connection_collation = std::uint16_t(100);
    input.ssl = ssl_mode::require;
    input.multi_queries = true;

    auto hparams = make_hparams(input);

    BOOST_TEST(hparams.username() == "myuser");
    BOOST_TEST(hparams.password() == "mypass");
    BOOST_TEST(hparams.database() == "mydb");
    BOOST_TEST(hparams.connection_collation() == std::uint16_t(100));
    BOOST_TEST(hparams.ssl() == ssl_mode::require);
    BOOST_TEST(hparams.multi_queries());
}

BOOST_AUTO_TEST_CASE(make_hparams_2)
{
    connect_params input;
    input.server_address.emplace_unix_path("/var/sock");
    input.username = "myuser2";
    input.password = "mypass2";
    input.database = "mydb2";
    input.connection_collation = std::uint16_t(200);
    input.ssl = ssl_mode::require;
    input.multi_queries = false;

    auto hparams = make_hparams(input);

    BOOST_TEST(hparams.username() == "myuser2");
    BOOST_TEST(hparams.password() == "mypass2");
    BOOST_TEST(hparams.database() == "mydb2");
    BOOST_TEST(hparams.connection_collation() == std::uint16_t(200));
    BOOST_TEST(hparams.ssl() == ssl_mode::disable);  // SSL mode was adjusted (UNIX)
    BOOST_TEST(!hparams.multi_queries());
}

BOOST_AUTO_TEST_SUITE_END()

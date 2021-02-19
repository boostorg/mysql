//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/connection.hpp"
#include "metadata_validator.hpp"
#include "integration_test_common.hpp"
#include "test_common.hpp"

namespace net = boost::asio;
using namespace boost::mysql::test;
using boost::mysql::detail::make_error_code;
using boost::mysql::field_metadata;
using boost::mysql::field_type;
using boost::mysql::errc;
using boost::mysql::resultset;

BOOST_AUTO_TEST_SUITE(test_query)

BOOST_MYSQL_NETWORK_TEST(insert_ok, network_fixture, network_ssl_gen)
{
    this->connect(sample.ssl);
    this->start_transaction();

    // Issue query
    auto result = sample.net->query(this->conn,
        "INSERT INTO inserts_table (field_varchar, field_date) VALUES ('v0', '2010-10-11')");

    // Verify resultset
    result.validate_no_error();
    BOOST_TEST(result.value.fields().empty());
    BOOST_TEST(result.value.valid());
    BOOST_TEST(result.value.complete());
    BOOST_TEST(result.value.affected_rows() == 1);
    BOOST_TEST(result.value.warning_count() == 0);
    BOOST_TEST(result.value.last_insert_id() > 0);
    BOOST_TEST(result.value.info() == "");

    // Verify insertion took place
    BOOST_TEST(this->get_table_size("inserts_table") == 1);
}

BOOST_MYSQL_NETWORK_TEST(insert_error, network_fixture, network_ssl_gen)
{
    this->connect(sample.ssl);
    this->start_transaction();
    auto result = sample.net->query(this->conn,
        "INSERT INTO bad_table (field_varchar, field_date) VALUES ('v0', '2010-10-11')");
    result.validate_error(errc::no_such_table, {"table", "doesn't exist", "bad_table"});
    BOOST_TEST(!result.value.valid());
}

BOOST_MYSQL_NETWORK_TEST(update_ok, network_fixture, network_ssl_gen)
{
    this->connect(sample.ssl);
    this->start_transaction();

    // Issue the query
    auto result = sample.net->query(this->conn,
        "UPDATE updates_table SET field_int = field_int+10");

    // Validate resultset
    result.validate_no_error();
    BOOST_TEST(result.value.fields().empty());
    BOOST_TEST(result.value.valid());
    BOOST_TEST(result.value.complete());
    BOOST_TEST(result.value.affected_rows() == 2);
    BOOST_TEST(result.value.warning_count() == 0);
    BOOST_TEST(result.value.last_insert_id() == 0);
    validate_string_contains(std::string(result.value.info()), {"rows matched"});

    // Validate it took effect
    result = sample.net->query(this->conn,
        "SELECT field_int FROM updates_table WHERE field_varchar = 'f0'");
    result.validate_no_error();
    auto updated_value = result.value.read_all().at(0).values().at(0).template get<std::int64_t>();
    BOOST_TEST(updated_value == 52); // initial value was 42
}

BOOST_MYSQL_NETWORK_TEST(select_ok, network_fixture, network_ssl_gen)
{
    this->connect(sample.ssl);
    auto result = sample.net->query(this->conn, "SELECT * FROM empty_table");
    result.validate_no_error();
    BOOST_TEST(result.value.valid());
    BOOST_TEST(!result.value.complete());
    this->validate_2fields_meta(result.value, "empty_table");
}

BOOST_MYSQL_NETWORK_TEST(select_error, network_fixture, network_ssl_gen)
{
    this->connect(sample.ssl);
    auto result = sample.net->query(this->conn,
        "SELECT field_varchar, field_bad FROM one_row_table");
    result.validate_error(errc::bad_field_error, {"unknown column", "field_bad"});
    BOOST_TEST(!result.value.valid());
}

BOOST_AUTO_TEST_SUITE_END() // test_query

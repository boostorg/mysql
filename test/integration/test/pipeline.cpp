//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/pipeline.hpp>
#include <boost/mysql/ssl_mode.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/test/unit_test.hpp>

#include "test_common/create_basic.hpp"
#include "test_common/create_diagnostics.hpp"
#include "test_common/netfun_helpers.hpp"
#include "test_common/network_result.hpp"
#include "test_common/printing.hpp"
#include "test_integration/common.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
namespace asio = boost::asio;
using boost::test_tools::per_element;

namespace {

BOOST_AUTO_TEST_SUITE(test_pipeline)

struct fixture
{
    asio::io_context ctx;
    any_connection conn{ctx};

    fixture() { conn.connect(default_connect_params(ssl_mode::disable)); }
};

BOOST_FIXTURE_TEST_CASE(dynamic_success, fixture)
{
    // Setup
    pipeline_request req;
    pipeline_request::response_type res;

    // Populate the request
    req.add(reset_connection_stage())
        .add(execute_stage("SET @myvar = 15"))
        .add(prepare_statement_stage("SELECT * FROM one_row_table WHERE id = ?"));

    // Run it
    auto result = create_initial_netresult<void>();
    conn.run_pipeline(req, res, result.err, *result.diag);
    result.validate_no_error();

    // Check results
    BOOST_TEST_REQUIRE(res.size() == 3u);
    BOOST_TEST(res[0].error() == errcode_with_diagnostics{});
    BOOST_TEST(!res[0].has_results());
    BOOST_TEST(!res[0].has_statement());
    BOOST_TEST(res[1].has_results());
    auto stmt = res[2].as_statement();
    BOOST_TEST(stmt.valid());
    BOOST_TEST(conn.current_character_set().error() == client_errc::unknown_character_set);

    // Re-populate the pipeline, with different stages
    req.clear();
    req.add(set_character_set_stage(utf8mb4_charset))
        .add(execute_stage(stmt, {0}))
        .add(execute_stage(stmt, {1}))
        .add(close_statement_stage(stmt));

    // Run it
    result = create_initial_netresult<void>();
    conn.run_pipeline(req, res, result.err, *result.diag);
    result.validate_no_error();

    // Check results
    BOOST_TEST_REQUIRE(res.size() == 4u);
    BOOST_TEST(res[0].error() == errcode_with_diagnostics{});
    BOOST_TEST(!res[0].has_results());
    BOOST_TEST(!res[0].has_statement());
    BOOST_TEST(res[1].as_results().rows() == rows(), per_element());
    BOOST_TEST(res[2].as_results().rows() == makerows(2, 1, "f0"), per_element());
    BOOST_TEST(res[3].error() == errcode_with_diagnostics{});
    BOOST_TEST(!res[3].has_results());
    BOOST_TEST(!res[3].has_statement());
}

BOOST_FIXTURE_TEST_CASE(dynamic_errors, fixture)
{
    // Setup
    pipeline_request req;
    pipeline_request::response_type res;

    // Populate the request with some successes and some errors
    req.add(execute_stage("SET @myvar = 42"))                                  // OK
        .add(prepare_statement_stage("SELECT * FROM bad_table WHERE id = ?"))  // error: bad table
        .add(execute_stage(""))                                                // error: empty query
        .add(execute_stage("SELECT @myvar"))                                   // OK
        .add(set_character_set_stage(character_set("bad_charset", utf8mb4_charset.next_char)))  // bad charset
        .add(execute_stage("SELECT 'abc'"));                                                    // OK

    // Run it. The result of the operation is the first encountered error
    auto result = create_initial_netresult<void>();
    conn.run_pipeline(req, res, result.err, *result.diag);
    result.validate_error_exact(
        common_server_errc::er_no_such_table,
        "Table 'boost_mysql_integtests.bad_table' doesn't exist"
    );

    // Check results
    BOOST_TEST_REQUIRE(res.size() == 6u);
    BOOST_TEST(res[0].has_results());
    BOOST_TEST(
        res[1].error() == (errcode_with_diagnostics{
                              common_server_errc::er_no_such_table,
                              create_server_diag("Table 'boost_mysql_integtests.bad_table' doesn't exist")
                          })
    );
    BOOST_TEST(
        res[2].error() ==
        (errcode_with_diagnostics{common_server_errc::er_empty_query, create_server_diag("Query was empty")})
    );
    BOOST_TEST(res[3].as_results().rows() == makerows(1, 42), per_element());
    BOOST_TEST(
        res[4].error() == (errcode_with_diagnostics{
                              common_server_errc::er_unknown_character_set,
                              create_server_diag("Unknown character set: 'bad_charset'")
                          })
    );
    BOOST_TEST(res[5].as_results().rows() == makerows(1, "abc"), per_element());
}

BOOST_FIXTURE_TEST_CASE(static_success, fixture)
{
    // Setup
    auto req = make_pipeline_request(
        reset_connection_stage(),
        execute_stage("SET @myvar = 15"),
        prepare_statement_stage("SELECT * FROM one_row_table WHERE id = ?")
    );
    decltype(req)::response_type res;

    // Run it
    auto result = create_initial_netresult<void>();
    conn.run_pipeline(req, res, result.err, *result.diag);
    result.validate_no_error();

    // Check results
    BOOST_TEST(std::get<0>(res) == errcode_with_diagnostics{});
    BOOST_TEST(std::get<1>(res).has_value());
    auto stmt = std::get<2>(res).value();
    BOOST_TEST(stmt.valid());
    BOOST_TEST(conn.current_character_set().error() == client_errc::unknown_character_set);

    // Create another pipeline
    auto req2 = make_pipeline_request(
        set_character_set_stage(utf8mb4_charset),
        execute_stage(stmt, {0}),
        execute_stage(stmt, {1}),
        close_statement_stage(stmt)
    );
    decltype(req2)::response_type res2;

    // Run it
    result = create_initial_netresult<void>();
    conn.run_pipeline(req2, res2, result.err, *result.diag);
    result.validate_no_error();

    // Check results
    BOOST_TEST(std::get<0>(res2) == errcode_with_diagnostics{});
    BOOST_TEST(std::get<1>(res2).value().rows() == rows(), per_element());
    BOOST_TEST(std::get<2>(res2).value().rows() == makerows(2, 1, "f0"), per_element());
    BOOST_TEST(std::get<3>(res2) == errcode_with_diagnostics{});
}

BOOST_FIXTURE_TEST_CASE(static_errors, fixture)
{
    // Setup
    auto req = make_pipeline_request(
        execute_stage("SET @myvar = 42"),                                 // OK
        prepare_statement_stage("SELECT * FROM bad_table WHERE id = ?"),  // error: bad table
        execute_stage(""),                                                // error: empty query
        execute_stage("SELECT @myvar"),                                   // OK
        set_character_set_stage(character_set("bad_charset", utf8mb4_charset.next_char)),  // bad charset
        execute_stage("SELECT 'abc'")                                                      // OK
    );
    decltype(req)::response_type res;

    // Run it. The result of the operation is the first encountered error
    auto result = create_initial_netresult<void>();
    conn.run_pipeline(req, res, result.err, *result.diag);
    result.validate_error_exact(
        common_server_errc::er_no_such_table,
        "Table 'boost_mysql_integtests.bad_table' doesn't exist"
    );

    // Check results
    BOOST_TEST(std::get<0>(res).has_value());
    BOOST_TEST(
        std::get<1>(res).error() ==
        (errcode_with_diagnostics{
            common_server_errc::er_no_such_table,
            create_server_diag("Table 'boost_mysql_integtests.bad_table' doesn't exist")
        })
    );
    BOOST_TEST(
        std::get<2>(res).error() ==
        (errcode_with_diagnostics{common_server_errc::er_empty_query, create_server_diag("Query was empty")})
    );
    BOOST_TEST(std::get<3>(res).value().rows() == makerows(1, 42), per_element());
    BOOST_TEST(
        std::get<4>(res) == (errcode_with_diagnostics{
                                common_server_errc::er_unknown_character_set,
                                create_server_diag("Unknown character set: 'bad_charset'")
                            })
    );
    BOOST_TEST(std::get<5>(res).value().rows() == makerows(1, "abc"), per_element());
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
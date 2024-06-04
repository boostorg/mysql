//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/pipeline.hpp>
#include <boost/mysql/ssl_mode.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/test/unit_test.hpp>

#include "test_common/create_basic.hpp"
#include "test_common/netfun_helpers.hpp"
#include "test_common/network_result.hpp"
#include "test_common/printing.hpp"
#include "test_integration/common.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
namespace asio = boost::asio;

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
    BOOST_TEST(res[1].as_results().rows() == rows(), boost::test_tools::per_element());
    BOOST_TEST(res[2].as_results().rows() == makerows(2, 1, "f0"), boost::test_tools::per_element());
    BOOST_TEST(res[3].error() == errcode_with_diagnostics{});
    BOOST_TEST(!res[3].has_results());
    BOOST_TEST(!res[3].has_statement());
}

// dynamic, with non fatal errors at each step
// static, with all step kinds, success
// static, with non fatal errors at each step
// dynamic, empty (should this be unit?)
// dynamic, fatal error (unit)

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
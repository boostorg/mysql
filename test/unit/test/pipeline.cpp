//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/pipeline.hpp>
#include <boost/mysql/results.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/pipeline.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/test/unit_test.hpp>

#include "test_common/create_diagnostics.hpp"
#include "test_common/printing.hpp"
#include "test_unit/create_execution_processor.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_statement.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;

BOOST_AUTO_TEST_SUITE(test_pipeline)

static void check_error(
    const errcode_with_diagnostics& actual,
    error_code expected_ec,
    const diagnostics& expected_diag = {}
)
{
    BOOST_TEST(actual.code == expected_ec);
    BOOST_TEST(actual.diag == expected_diag);
}

BOOST_AUTO_TEST_SUITE(any_stage_response_)

BOOST_AUTO_TEST_CASE(default_ctor)
{
    // Construct
    any_stage_response r;

    // Contains an empty error
    BOOST_TEST(!r.has_results());
    BOOST_TEST(!r.has_statement());
    check_error(r.error(), error_code());
}

BOOST_AUTO_TEST_CASE(underlying_error)
{
    // Setup
    any_stage_response r;
    detail::access::get_impl(r).set_error(client_errc::invalid_encoding, create_server_diag("my_message"));

    // Check
    BOOST_TEST(!r.has_results());
    BOOST_TEST(!r.has_statement());
    check_error(r.error(), client_errc::invalid_encoding, create_server_diag("my_message"));
}

BOOST_AUTO_TEST_CASE(underlying_statement)
{
    // Setup
    any_stage_response r;
    detail::access::get_impl(r).set_result(statement_builder().id(3).build());

    // Check
    BOOST_TEST(!r.has_results());
    BOOST_TEST(r.has_statement());
    BOOST_TEST(r.as_statement().id() == 3u);
    BOOST_TEST(r.get_statement().id() == 3u);

    // error() can be called and returns an empty error
    check_error(r.error(), error_code());
}

BOOST_AUTO_TEST_CASE(underlying_results)
{
    // Setup
    any_stage_response r;
    detail::access::get_impl(r).setup(detail::pipeline_stage_kind::execute);
    add_ok(detail::access::get_impl(r).get_processor(), ok_builder().info("some_info").build());

    // Check
    BOOST_TEST(r.has_results());
    BOOST_TEST(!r.has_statement());
    BOOST_TEST(r.get_results().info() == "some_info");
    BOOST_TEST(r.as_results().info() == "some_info");

    // Rvalue reference accessors work
    results&& ref1 = std::move(r).get_results();
    results&& ref2 = std::move(r).as_results();
    boost::ignore_unused(ref1);
    boost::ignore_unused(ref2);

    // error() can be called and returns an empty error
    check_error(r.error(), error_code());
}

/**
 * changing between them
 * as_results, as_statement exceptions
 * setup
 */

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

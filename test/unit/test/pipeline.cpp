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
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/pipeline.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/core/span.hpp>
#include <boost/optional/optional.hpp>
#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <stdexcept>
#include <vector>

#include "test_common/assert_buffer_equals.hpp"
#include "test_common/create_diagnostics.hpp"
#include "test_common/printing.hpp"
#include "test_unit/create_execution_processor.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_query_frame.hpp"
#include "test_unit/create_statement.hpp"
#include "test_unit/printing.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using detail::pipeline_stage_kind;
using detail::resultset_encoding;

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
    detail::access::get_impl(r).emplace_results();
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

BOOST_AUTO_TEST_CASE(as_results_error)
{
    // Empty error
    any_stage_response r;
    BOOST_CHECK_THROW(r.as_results(), std::invalid_argument);
    BOOST_CHECK_THROW(std::move(r).as_results(), std::invalid_argument);

    // Non-empty error
    detail::access::get_impl(r).set_error(client_errc::extra_bytes, create_client_diag("my_msg"));
    BOOST_CHECK_THROW(r.as_results(), std::invalid_argument);
    BOOST_CHECK_THROW(std::move(r).as_results(), std::invalid_argument);

    // Statement
    detail::access::get_impl(r).set_result(statement_builder().build());
    BOOST_CHECK_THROW(r.as_results(), std::invalid_argument);
    BOOST_CHECK_THROW(std::move(r).as_results(), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(as_statement_error)
{
    // Empty error
    any_stage_response r;
    BOOST_CHECK_THROW(r.as_statement(), std::invalid_argument);

    // Non-empty error
    detail::access::get_impl(r).set_error(client_errc::extra_bytes, create_client_diag("my_msg"));
    BOOST_CHECK_THROW(r.as_statement(), std::invalid_argument);

    // results
    detail::access::get_impl(r).emplace_results();
    BOOST_CHECK_THROW(r.as_statement(), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(change_type)
{
    any_stage_response r;

    // Set results
    detail::access::get_impl(r).emplace_results();
    BOOST_TEST(r.has_results());

    // Set an error
    detail::access::get_impl(r).set_error(client_errc::extra_bytes, create_client_diag("abc"));
    BOOST_TEST(!r.has_results());
    check_error(r.error(), client_errc::extra_bytes, create_client_diag("abc"));

    // Reset the error
    detail::access::get_impl(r).emplace_error();
    check_error(r.error(), error_code());

    // Set a statement
    detail::access::get_impl(r).set_result(statement_builder().build());
    BOOST_TEST(r.has_statement());

    // Set results again
    detail::access::get_impl(r).emplace_results();
    BOOST_TEST(r.has_results());
    BOOST_TEST(!r.has_statement());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(stage_creation)

template <class PipelineStageType>
static detail::pipeline_request_stage check_stage_creation(
    PipelineStageType stage,
    const std::vector<std::uint8_t>& expected_buffer,
    pipeline_stage_kind expected_kind
)
{
    // Serialize the request
    std::vector<std::uint8_t> buff{0xde, 0xad};
    auto erased_stage = detail::pipeline_stage_access::create(stage, buff);

    // Check
    std::vector<std::uint8_t> expected{0xde, 0xad};
    expected.insert(expected.end(), expected_buffer.begin(), expected_buffer.end());
    BOOST_TEST(erased_stage.kind == expected_kind);
    BOOST_TEST(erased_stage.seqnum == 1u);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buff, expected);

    return erased_stage;
}

static void check_execute_stage_creation(
    execute_stage stage,
    const std::vector<std::uint8_t>& expected_buffer,
    resultset_encoding expected_encoding
)
{
    auto erased_stage = check_stage_creation(stage, expected_buffer, pipeline_stage_kind::execute);
    BOOST_TEST(erased_stage.stage_specific.enc == expected_encoding);
}

BOOST_AUTO_TEST_CASE(execute_text_query)
{
    check_execute_stage_creation(
        execute_stage("SELECT 1"),
        create_query_frame(0, "SELECT 1"),
        resultset_encoding::text
    );
}

BOOST_AUTO_TEST_CASE(execute_statement_individual_parameters)
{
    check_execute_stage_creation(
        execute_stage(statement_builder().id(2).num_params(3).build(), {42, "abc", nullptr}),
        {0x1e, 0x00, 0x00, 0x00, 0x17, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
         0x00, 0x00, 0x04, 0x01, 0x08, 0x00, 0xfe, 0x00, 0x06, 0x00, 0x2a, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x61, 0x62, 0x63},
        resultset_encoding::binary
    );
}

BOOST_AUTO_TEST_CASE(execute_statement_individual_fields_no_params)
{
    // We run the required writable field transformations
    check_execute_stage_creation(
        execute_stage(statement_builder().id(2).num_params(0).build(), {}),
        {0x0a, 0x00, 0x00, 0x00, 0x17, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00},
        resultset_encoding::binary
    );
}

BOOST_AUTO_TEST_CASE(execute_statement_writable_fields)
{
    // We run the required writable field transformations
    check_execute_stage_creation(
        execute_stage(
            statement_builder().id(2).num_params(3).build(),
            {boost::optional<int>(42), "abc", boost::optional<int>()}
        ),
        {0x1e, 0x00, 0x00, 0x00, 0x17, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
         0x00, 0x00, 0x04, 0x01, 0x08, 0x00, 0xfe, 0x00, 0x06, 0x00, 0x2a, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x61, 0x62, 0x63},
        resultset_encoding::binary
    );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

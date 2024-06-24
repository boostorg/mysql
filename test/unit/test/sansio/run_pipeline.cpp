//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/pipeline.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/pipeline.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/mysql/impl/internal/sansio/run_pipeline.hpp>

#include <boost/asio/error.hpp>
#include <boost/core/span.hpp>
#include <boost/test/tools/detail/per_element_manip.hpp>
#include <boost/test/unit_test.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "test_common/buffer_concat.hpp"
#include "test_common/create_basic.hpp"
#include "test_common/create_diagnostics.hpp"
#include "test_unit/algo_test.hpp"
#include "test_unit/create_coldef_frame.hpp"
#include "test_unit/create_err.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/create_meta.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"
#include "test_unit/create_prepare_statement_response.hpp"
#include "test_unit/create_row_message.hpp"
#include "test_unit/mock_execution_processor.hpp"
#include "test_unit/printing.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
namespace asio = boost::asio;
using boost::span;
using boost::test_tools::per_element;
using detail::pipeline_request_stage;
using detail::pipeline_stage_kind;
using detail::resultset_encoding;

BOOST_AUTO_TEST_SUITE(test_run_pipeline)

const std::vector<std::uint8_t> mock_request{1, 2, 3, 4, 5, 6, 7, 9, 21};

/**
TODO: tests without a response
    close statement success, error
    set character set success, error
    reset connection success, error
    ping success, error
    any of the above after a fatal error
re-using responses
    changing type from/to execute
    increasing/decreasing size
 */

struct fixture : algo_fixture_base
{
    detail::run_pipeline_algo algo;
    std::vector<any_stage_response> resp;

    fixture(span<const pipeline_request_stage> stages, span<const std::uint8_t> req_buffer = mock_request)
        : algo({&diag, req_buffer, stages, &resp})
    {
    }

    // Verify that all stages succeeded
    void check_all_stages_succeeded()
    {
        for (const auto& item : resp)
            BOOST_TEST(item.error() == errcode_with_diagnostics{});
    }

    // Verify that a certain step failed
    void check_stage_error(std::size_t i, const errcode_with_diagnostics& expected)
    {
        BOOST_TEST(resp.at(i).error() == expected);
    }
};

// All stage kinds work properly
BOOST_AUTO_TEST_CASE(execute_success)
{
    // Setup. Each step has a different encoding
    const std::array<pipeline_request_stage, 2> stages{
        {
         {pipeline_stage_kind::execute, 42u, resultset_encoding::binary},
         {pipeline_stage_kind::execute, 11u, resultset_encoding::text},
         }
    };
    fixture fix(stages);

    // Run the test
    algo_test()
        .expect_write(mock_request)
        .expect_read(create_ok_frame(42, ok_builder().info("1st").build()))  // 1st op ok
        .expect_read(create_frame(11, {0x01}))                               // 2nd op OK, 1 column
        .expect_read(create_coldef_frame(12, meta_builder().type(column_type::tinyint).build_coldef()))
        .expect_read(buffer_builder()
                         .add(create_text_row_message(13, 42))
                         .add(create_text_row_message(14, 43))
                         .add(create_eof_frame(15, ok_builder().info("2nd").build()))
                         .build())
        .check(fix);

    // All stages succeeded
    BOOST_TEST_REQUIRE(fix.resp.size() == stages.size());
    fix.check_all_stages_succeeded();

    // Check results
    const auto& res0 = fix.resp[0].as_results();
    BOOST_TEST(res0.rows() == rows(), per_element());
    BOOST_TEST(res0.info() == "1st");
    BOOST_TEST(detail::access::get_impl(res0).encoding() == resultset_encoding::binary);

    const auto& res1 = fix.resp[1].as_results();
    BOOST_TEST(res1.rows() == makerows(1, 42, 43), per_element());
    BOOST_TEST(res1.info() == "2nd");
    BOOST_TEST(detail::access::get_impl(res1).encoding() == resultset_encoding::text);
}

BOOST_AUTO_TEST_CASE(prepare_statement_success)
{
    // Setup
    const std::array<pipeline_request_stage, 2> stages{
        {
         {pipeline_stage_kind::prepare_statement, 42u, {}},
         {pipeline_stage_kind::prepare_statement, 11u, {}},
         }
    };
    fixture fix(stages);

    // Run the test. 1st statement has 2 meta, 2nd has 1
    algo_test()
        .expect_write(mock_request)
        .expect_read(prepare_stmt_response_builder().seqnum(42).id(7).num_columns(0).num_params(2).build())
        .expect_read(create_coldef_frame(43, meta_builder().name("abc").build_coldef()))
        .expect_read(create_coldef_frame(44, meta_builder().name("def").build_coldef()))
        .expect_read(prepare_stmt_response_builder().seqnum(11).id(9).num_columns(0).num_params(1).build())
        .expect_read(create_coldef_frame(12, meta_builder().name("aaa").build_coldef()))
        .check(fix);

    // All stages succeeded
    BOOST_TEST_REQUIRE(fix.resp.size() == stages.size());
    fix.check_all_stages_succeeded();

    // Check the resulting statements
    auto stmt0 = fix.resp[0].as_statement();
    BOOST_TEST(stmt0.id() == 7u);
    BOOST_TEST(stmt0.num_params() == 2u);

    auto stmt1 = fix.resp[1].as_statement();
    BOOST_TEST(stmt1.id() == 9u);
    BOOST_TEST(stmt1.num_params() == 1u);
}

BOOST_AUTO_TEST_CASE(close_statement_success)
{
    // Setup
    const std::array<pipeline_request_stage, 2> stages{
        {
         {pipeline_stage_kind::close_statement, 3u, {}},
         {pipeline_stage_kind::close_statement, 8u, {}},
         }
    };
    fixture fix(stages);

    // Run the test. Close statement doesn't have a response
    algo_test().expect_write(mock_request).check(fix);

    // All stages succeeded
    BOOST_TEST_REQUIRE(fix.resp.size() == stages.size());
    fix.check_all_stages_succeeded();
}

BOOST_AUTO_TEST_CASE(reset_connection)
{
    // Setup
    const std::array<pipeline_request_stage, 1> stages{
        {
         {pipeline_stage_kind::reset_connection, 3u, {}},
         }
    };
    fixture fix(stages);
    fix.st.current_charset = utf8mb4_charset;

    // Run the test
    algo_test().expect_write(mock_request).expect_read(create_ok_frame(3, ok_builder().build())).check(fix);

    // All stages succeeded
    BOOST_TEST_REQUIRE(fix.resp.size() == stages.size());
    fix.check_all_stages_succeeded();

    // The current character set was reset
    BOOST_TEST(fix.st.charset_ptr() == nullptr);
}

BOOST_AUTO_TEST_CASE(set_character_set)
{
    // Setup
    const std::array<pipeline_request_stage, 1> stages{
        {
         {pipeline_stage_kind::set_character_set, 19u, utf8mb4_charset},
         }
    };
    fixture fix(stages);

    // Run the test
    algo_test().expect_write(mock_request).expect_read(create_ok_frame(19, ok_builder().build())).check(fix);

    // All stages succeeded
    BOOST_TEST_REQUIRE(fix.resp.size() == stages.size());
    fix.check_all_stages_succeeded();

    // The current character set was set
    BOOST_TEST(fix.st.charset_ptr()->name == "utf8mb4");
}

BOOST_AUTO_TEST_CASE(ping)
{
    // Setup
    const std::array<pipeline_request_stage, 1> stages{
        {
         {pipeline_stage_kind::ping, 32u, {}},
         }
    };
    fixture fix(stages);

    // Run the test
    algo_test()
        .expect_write(mock_request)
        .expect_read(create_ok_frame(32, ok_builder().no_backslash_escapes(true).build()))
        .check(fix);

    // All stages succeeded
    BOOST_TEST_REQUIRE(fix.resp.size() == stages.size());
    fix.check_all_stages_succeeded();

    // The OK packet was processed successfully
    BOOST_TEST(fix.st.backslash_escapes == false);
}

BOOST_AUTO_TEST_CASE(combination)
{
    // Setup. Typical connection setup pipeline, where we reset, set names,
    // set the time_zone and prepare some statements
    const std::array<pipeline_request_stage, 5> stages{
        {
         {pipeline_stage_kind::reset_connection, 32u, {}},
         {pipeline_stage_kind::set_character_set, 16u, utf8mb4_charset},
         {pipeline_stage_kind::execute, 10u, resultset_encoding::text},
         {pipeline_stage_kind::prepare_statement, 0u, {}},
         {pipeline_stage_kind::prepare_statement, 1u, {}},
         }
    };
    fixture fix(stages);
    fix.st.backslash_escapes = false;

    // Run the test
    algo_test()
        .expect_write(mock_request)
        .expect_read(create_ok_frame(32, ok_builder().build()))
        .expect_read(create_ok_frame(16, ok_builder().build()))
        .expect_read(create_ok_frame(10, ok_builder().build()))
        .expect_read(prepare_stmt_response_builder().seqnum(0).id(3).num_columns(1).num_params(1).build())
        .expect_read(create_coldef_frame(1, meta_builder().name("abc").build_coldef()))
        .expect_read(create_coldef_frame(2, meta_builder().name("def").build_coldef()))
        .expect_read(prepare_stmt_response_builder().seqnum(1).id(1).num_columns(0).num_params(0).build())
        .check(fix);

    // All stages succeeded
    BOOST_TEST_REQUIRE(fix.resp.size() == stages.size());
    fix.check_all_stages_succeeded();

    // The pipeline had its intended effect
    BOOST_TEST(fix.st.backslash_escapes == true);
    BOOST_TEST(fix.st.charset_ptr()->name == "utf8mb4");
    BOOST_TEST(fix.resp.items.at(3).stmt.id() == 3u);
    BOOST_TEST(fix.resp.items.at(4).stmt.id() == 1u);
}

BOOST_AUTO_TEST_CASE(no_requests)
{
    // Setup
    fixture fix({}, {});

    // Run the test. We complete immediately
    algo_test().check(fix);

    // Setup was called correctly
    fix.check_setup({});
}

BOOST_AUTO_TEST_CASE(error_writing_request)
{
    // Setup
    const std::array<pipeline_request_stage, 3> stages{
        {
         {pipeline_stage_kind::reset_connection, 32u, {}},
         {pipeline_stage_kind::set_character_set, 16u, utf8mb4_charset},
         {pipeline_stage_kind::execute, 10u, resultset_encoding::text},
         }
    };
    fixture fix(stages);

    // Run the test. No response reading is attempted
    algo_test().expect_write(mock_request, asio::error::eof).check(fix, asio::error::eof);

    // Setup was called correctly
    fix.check_setup(stages);

    // All requests were marked as failed
    fix.check_stage_error(0, {asio::error::eof, {}});
    fix.check_stage_error(1, {asio::error::eof, {}});
    fix.check_stage_error(2, {asio::error::eof, {}});
}

BOOST_AUTO_TEST_CASE(nonfatal_errors)
{
    // Setup
    const std::array<pipeline_request_stage, 3> stages{
        {
         {pipeline_stage_kind::prepare_statement, 32u, {}},
         {pipeline_stage_kind::prepare_statement, 16u, {}},
         {pipeline_stage_kind::execute, 10u, resultset_encoding::text},
         }
    };
    fixture fix(stages);

    // Run the test. Steps 1 and 3 fail.
    // The first error is the operation's result
    algo_test()
        .expect_write(mock_request)
        .expect_read(err_builder()
                         .seqnum(32)
                         .code(common_server_errc::er_bad_db_error)
                         .message("my_message")
                         .build_frame())
        .expect_read(prepare_stmt_response_builder().seqnum(16).id(3).num_columns(0).num_params(0).build())
        .expect_read(err_builder()
                         .seqnum(10)
                         .code(common_server_errc::er_bad_field_error)
                         .message("other_msg")
                         .build_frame())
        .check(fix, common_server_errc::er_bad_db_error, create_server_diag("my_message"));

    // Setup was called correctly
    fix.check_setup(stages);

    // Stage errors
    fix.check_stage_error(0, {common_server_errc::er_bad_db_error, create_server_diag("my_message")});
    fix.check_stage_error(1, {error_code(), {}});
    fix.check_stage_error(2, {common_server_errc::er_bad_field_error, create_server_diag("other_msg")});

    // The operation that succeeded had its result set
    BOOST_TEST(fix.resp.items.at(1).stmt.id() == 3u);
}

BOOST_AUTO_TEST_CASE(nonfatal_errors_middle)
{
    // Setup
    const std::array<pipeline_request_stage, 3> stages{
        {
         {pipeline_stage_kind::prepare_statement, 32u, {}},
         {pipeline_stage_kind::prepare_statement, 16u, {}},
         {pipeline_stage_kind::execute, 10u, resultset_encoding::text},
         }
    };
    fixture fix(stages);

    // Run the test. Steps 1 and 3 fail.
    // The first error is the operation's result
    algo_test()
        .expect_write(mock_request)
        .expect_read(prepare_stmt_response_builder().seqnum(32).id(3).num_columns(0).num_params(0).build())
        .expect_read(err_builder()
                         .seqnum(16)
                         .code(common_server_errc::er_bad_db_error)
                         .message("my_message")
                         .build_frame())
        .expect_read(create_ok_frame(10, ok_builder().no_backslash_escapes(true).build()))
        .check(fix, common_server_errc::er_bad_db_error, create_server_diag("my_message"));

    // Setup was called correctly
    fix.check_setup(stages);

    // Stage errors
    fix.check_stage_error(0, {});
    fix.check_stage_error(1, {common_server_errc::er_bad_db_error, create_server_diag("my_message")});
    fix.check_stage_error(2, {});

    // We processed the OK packet correctly
    BOOST_TEST(fix.st.backslash_escapes == false);
}

BOOST_AUTO_TEST_CASE(fatal_error_first)
{
    // Setup
    const std::array<pipeline_request_stage, 3> stages{
        {
         {pipeline_stage_kind::reset_connection, 32u, {}},
         {pipeline_stage_kind::set_character_set, 16u, utf8mb4_charset},
         {pipeline_stage_kind::execute, 10u, resultset_encoding::text},
         }
    };
    fixture fix(stages);

    // Run the test. Reading the first response fails, and we don't further reading
    algo_test()
        .expect_write(mock_request)
        .expect_read(asio::error::network_reset)
        .check(fix, asio::error::network_reset);

    // Setup was called correctly
    fix.check_setup(stages);

    // All subsequent requests were marked as failed
    fix.check_stage_error(0, {asio::error::network_reset, {}});
    fix.check_stage_error(1, {asio::error::network_reset, {}});
    fix.check_stage_error(2, {asio::error::network_reset, {}});
}

BOOST_AUTO_TEST_CASE(fatal_error_middle)
{
    // Setup
    const std::array<pipeline_request_stage, 3> stages{
        {
         {pipeline_stage_kind::reset_connection, 32u, {}},
         {pipeline_stage_kind::set_character_set, 16u, utf8mb4_charset},
         {pipeline_stage_kind::execute, 10u, resultset_encoding::text},
         }
    };
    fixture fix(stages);

    // Run the test
    algo_test()
        .expect_write(mock_request)
        .expect_read(create_ok_frame(32, ok_builder().build()))
        .expect_read(asio::error::network_reset)
        .check(fix, asio::error::network_reset);

    // Setup was called correctly
    fix.check_setup(stages);

    // All subsequent requests were marked as failed
    fix.check_stage_error(0, {});
    fix.check_stage_error(1, {asio::error::network_reset, {}});
    fix.check_stage_error(2, {asio::error::network_reset, {}});
}

// If there are fatal and non-fatal errors, the fatal one is the result of the operation
BOOST_AUTO_TEST_CASE(nonfatal_then_fatal_error)
{
    // Setup
    const std::array<pipeline_request_stage, 3> stages{
        {
         {pipeline_stage_kind::reset_connection, 32u, {}},
         {pipeline_stage_kind::set_character_set, 16u, utf8mb4_charset},
         {pipeline_stage_kind::execute, 10u, resultset_encoding::text},
         }
    };
    fixture fix(stages);

    // Run the test
    algo_test()
        .expect_write(mock_request)
        .expect_read(err_builder()
                         .seqnum(32)
                         .code(common_server_errc::er_bad_db_error)
                         .message("my_message")
                         .build_frame())
        .expect_read(asio::error::already_connected)
        .check(fix, asio::error::already_connected);

    // Setup was called correctly
    fix.check_setup(stages);

    // Stage results
    fix.check_stage_error(0, {common_server_errc::er_bad_db_error, create_server_diag("my_message")});
    fix.check_stage_error(1, {asio::error::already_connected, {}});
    fix.check_stage_error(2, {asio::error::already_connected, {}});
}

// Edge case: fatal error with non-empty diagnostics
BOOST_AUTO_TEST_CASE(fatal_error_with_diag)
{
    // Setup
    const std::array<pipeline_request_stage, 3> stages{
        {
         {pipeline_stage_kind::reset_connection, 32u, {}},
         {pipeline_stage_kind::set_character_set, 16u, utf8mb4_charset},
         {pipeline_stage_kind::execute, 10u, resultset_encoding::text},
         }
    };
    fixture fix(stages);

    // Run the test
    algo_test()
        .expect_write(mock_request)
        .expect_read(
            err_builder().seqnum(32).code(common_server_errc::er_bad_db_error).message("bad db").build_frame()
        )
        .expect_read(err_builder()
                         .seqnum(16)
                         .code(common_server_errc::er_aborting_connection)
                         .message("aborting connection")
                         .build_frame())
        .check(fix, common_server_errc::er_aborting_connection, create_server_diag("aborting connection"));

    // Setup was called correctly
    fix.check_setup(stages);

    // Stage results
    fix.check_stage_error(0, {common_server_errc::er_bad_db_error, create_server_diag("bad db")});
    fix.check_stage_error(
        1,
        {common_server_errc::er_aborting_connection, create_server_diag("aborting connection")}
    );
    fix.check_stage_error(
        2,
        {common_server_errc::er_aborting_connection, create_server_diag("aborting connection")}
    );
}

BOOST_AUTO_TEST_SUITE_END()

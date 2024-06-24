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
#include <boost/mysql/statement.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/pipeline.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/mysql/impl/internal/sansio/run_pipeline.hpp>

#include <boost/asio/error.hpp>
#include <boost/core/span.hpp>
#include <boost/test/unit_test.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "test_common/buffer_concat.hpp"
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

namespace boost {
namespace mysql {
namespace test {

struct mock_pipeline_response
{
    struct item
    {
        mock_execution_processor proc;
        statement stmt;
        errcode_with_diagnostics err;
    };

    std::size_t setup_num_calls{0u};
    span<const detail::pipeline_request_stage> setup_args;
    std::vector<item> items;
};

}  // namespace test

namespace detail {

template <>
struct pipeline_response_traits<test::mock_pipeline_response>
{
    using response_type = test::mock_pipeline_response;

    static void setup(response_type& self, span<const pipeline_request_stage> request)
    {
        ++self.setup_num_calls;
        self.setup_args = request;
        self.items.resize(request.size());
    }

    static execution_processor& get_processor(response_type& self, std::size_t idx)
    {
        // This function is only defined for execute stages
        BOOST_TEST_REQUIRE(idx < self.items.size());
        BOOST_TEST(self.setup_args[idx].kind == pipeline_stage_kind::execute);
        return self.items[idx].proc;
    }

    static void set_result(response_type& self, std::size_t idx, statement stmt)
    {
        // This function is only defined for prepare statement stages
        BOOST_TEST_REQUIRE(idx < self.items.size());
        BOOST_TEST(self.setup_args[idx].kind == pipeline_stage_kind::prepare_statement);
        self.items[idx].stmt = stmt;
    }

    static void set_error(response_type& self, std::size_t idx, error_code ec, diagnostics&& diag)
    {
        BOOST_TEST_REQUIRE(idx < self.items.size());
        self.items[idx].err = {ec, std::move(diag)};
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

BOOST_AUTO_TEST_SUITE(test_run_pipeline)

const std::vector<std::uint8_t> mock_request{1, 2, 3, 4, 5, 6, 7, 9, 21};

struct fixture : algo_fixture_base
{
    detail::run_pipeline_algo algo;
    mock_pipeline_response resp;

    fixture(span<const pipeline_request_stage> stages, span<const std::uint8_t> req_buffer = mock_request)
        : algo({
              &diag,
              detail::pipeline_request_view{req_buffer, stages},
              detail::pipeline_response_ref(resp)
    })
    {
    }

    // Verify that the stages we passed match the ones used in setup
    void check_setup(span<const pipeline_request_stage> stages)
    {
        BOOST_TEST(resp.setup_num_calls == 1u);
        BOOST_TEST(resp.setup_args == stages, per_element());
    }

    // Verify that all stages succeeded
    void check_all_stages_succeeded()
    {
        for (const auto& item : resp.items)
            BOOST_TEST(item.err == errcode_with_diagnostics{});
    }

    // Verify that a certain step failed
    void check_stage_error(std::size_t i, const errcode_with_diagnostics& expected)
    {
        BOOST_TEST(resp.items.at(i).err == expected);
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

    // Setup was called correctly and all stages succeeded
    fix.check_setup(stages);
    fix.check_all_stages_succeeded();

    // Check each processor calls
    auto& proc0 = fix.resp.items.at(0).proc;
    proc0.num_calls().reset(1).on_head_ok_packet(1).validate();
    BOOST_TEST(proc0.info() == "1st");
    BOOST_TEST(proc0.encoding() == resultset_encoding::binary);

    auto& proc1 = fix.resp.items.at(1).proc;
    proc1.num_calls()
        .reset(1)
        .on_num_meta(1)
        .on_meta(1)
        .on_row_batch_start(1)
        .on_row(2)
        .on_row_batch_finish(1)
        .on_row_ok_packet(1)
        .validate();
    BOOST_TEST(proc1.info() == "2nd");
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

    // Setup was called correctly and all stages succeeded
    fix.check_setup(stages);
    fix.check_all_stages_succeeded();

    // Check the resulting statements
    auto stmt0 = fix.resp.items.at(0).stmt;
    BOOST_TEST(stmt0.id() == 7u);
    BOOST_TEST(stmt0.num_params() == 2u);

    auto stmt1 = fix.resp.items.at(1).stmt;
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

    // Setup was called correctly and all stages succeeded
    fix.check_setup(stages);
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

    // Setup was called correctly and all stages succeeded
    fix.check_setup(stages);
    fix.check_all_stages_succeeded();

    // The current character set was reset
    BOOST_TEST(fix.st.current_charset == character_set());
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

    // Setup was called correctly and all stages succeeded
    fix.check_setup(stages);
    fix.check_all_stages_succeeded();

    // The current character set was set
    BOOST_TEST(fix.st.current_charset == utf8mb4_charset);
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

    // Setup was called correctly and all stages succeeded
    fix.check_setup(stages);
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

    // Setup was called correctly and all stages succeeded
    fix.check_setup(stages);
    fix.check_all_stages_succeeded();

    // The pipeline had its intended effect
    BOOST_TEST(fix.st.backslash_escapes == true);
    BOOST_TEST(fix.st.current_charset == utf8mb4_charset);
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

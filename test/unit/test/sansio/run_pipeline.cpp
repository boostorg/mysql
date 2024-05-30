//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/pipeline.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/mysql/impl/internal/sansio/run_pipeline.hpp>

#include <boost/core/span.hpp>
#include <boost/test/unit_test.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ostream>
#include <vector>

#include "test_common/buffer_concat.hpp"
#include "test_unit/algo_test.hpp"
#include "test_unit/create_coldef_frame.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/create_meta.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"
#include "test_unit/create_row_message.hpp"
#include "test_unit/mock_execution_processor.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
using boost::span;
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

const char* to_string(pipeline_stage_kind v)
{
    switch (v)
    {
    case pipeline_stage_kind::execute: return "pipeline_stage_kind::execute";
    case pipeline_stage_kind::prepare_statement: return "pipeline_stage_kind::prepare_statement";
    case pipeline_stage_kind::close_statement: return "pipeline_stage_kind::close_statement";
    case pipeline_stage_kind::reset_connection: return "pipeline_stage_kind::reset_connection";
    case pipeline_stage_kind::set_character_set: return "pipeline_stage_kind::set_character_set";
    case pipeline_stage_kind::ping: return "pipeline_stage_kind::ping";
    default: return "<unknown pipeline_stage_kind>";
    }
}

std::ostream& operator<<(std::ostream& os, pipeline_stage_kind v) { return os << to_string(v); }

}  // namespace detail
}  // namespace mysql
}  // namespace boost

BOOST_AUTO_TEST_SUITE(test_run_pipeline)

/**
 * execute
 * prepare statement
 * close statement
 * reset connection
 * set character set
 * ping
 * combination
 * no requests
 * error in each request kind
 * error writing request
 * error last
 * some errors and some successes
 * fatal error in the middle
 * error, fatal error
 * spotcheck: reset connection then set character set sets st.charset
 * seqnums get correctly set for all stage kinds
 * resultset encoding gets correctly propagated
 * two steps with same kind together
 */

constexpr std::uint8_t mock_request[] = {1, 2, 3, 4, 5, 6, 7, 9, 21};

static std::vector<std::uint8_t> mock_request_as_vector()
{
    return {std::begin(mock_request), std::end(mock_request)};
}

struct fixture : algo_fixture_base
{
    detail::run_pipeline_algo algo;
    mock_pipeline_response resp;

    fixture(span<const pipeline_request_stage> stages)
        : algo({&diag, mock_request, stages, detail::pipeline_response_ref(resp)})
    {
    }

    // Verify that the stages we passed match the ones used in setup
    void check_setup(span<const pipeline_request_stage> stages)
    {
        // Only kind needs to be validated, as seqnum and others don't contribute to usual setup effects
        // and are validated by other means
        BOOST_TEST_REQUIRE(resp.setup_args.size() == stages.size());
        for (std::size_t i = 0; i < stages.size(); ++i)
            BOOST_TEST(resp.setup_args[i].kind == stages[i].kind);
    }

    // Verify that all stages succeeded
    void check_all_stages_succeeded()
    {
        for (const auto& item : resp.items)
        {
            BOOST_TEST(item.err.code == error_code());
            BOOST_TEST(item.err.diag == diagnostics());
        }
    }
};

// All stage kinds work properly
BOOST_AUTO_TEST_CASE(execute_success)
{
    // Setup
    const pipeline_request_stage stages[] = {
        {pipeline_stage_kind::execute, 42u, resultset_encoding::binary},
        {pipeline_stage_kind::execute, 11u, resultset_encoding::text  },
    };
    fixture fix(stages);

    // Run the test
    algo_test()
        .expect_write(mock_request_as_vector())
        .expect_read(create_ok_frame(42, ok_builder().info("1st").build()))  // 1st op ok
        .expect_read(create_frame(11, {0x01}))                               // 2nd op OK, 1 column
        .expect_read(create_coldef_frame(12, meta_builder().type(column_type::tinyint).build_coldef()))
        .expect_read(buffer_builder()
                         .add(create_text_row_message(13, 42))
                         .add(create_text_row_message(14, 43))
                         .add(create_eof_frame(15, ok_builder().info("2nd").build()))
                         .build())
        .check(fix);

    // Setup was called correctly
    fix.check_setup(stages);

    // All stages succeeded
    fix.check_all_stages_succeeded();

    // Check each processor calls
    auto& proc0 = fix.resp.items[0].proc;
    proc0.num_calls().reset(1).on_head_ok_packet(1).validate();
    BOOST_TEST(proc0.info() == "1st");
    BOOST_TEST(proc0.encoding() == resultset_encoding::binary);

    auto& proc1 = fix.resp.items[1].proc;
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

// BOOST_AUTO_TEST_CASE(read_response_error_network)
// {
//     algo_test()
//         .expect_read(create_ok_frame(57, ok_builder().build()))
//         .check_network_errors<read_response_fixture>();
// }

// BOOST_AUTO_TEST_CASE(read_response_error_packet)
// {
//     // Setup
//     read_response_fixture fix;

//     // Run the test
//     algo_test()
//         .expect_read(err_builder()
//                          .seqnum(57)
//                          .code(common_server_errc::er_bad_db_error)
//                          .message("my_message")
//                          .build_frame())  // Error response
//         .check(fix, common_server_errc::er_bad_db_error, create_server_diag("my_message"));
// }

BOOST_AUTO_TEST_SUITE_END()

//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/column_type.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/metadata_mode.hpp>

#include <boost/mysql/detail/any_execution_request.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/start_execution.hpp>

#include <boost/asio/error.hpp>
#include <boost/test/unit_test.hpp>

#include <array>
#include <string>

#include "test_common/check_meta.hpp"
#include "test_common/create_basic.hpp"
#include "test_common/create_diagnostics.hpp"
#include "test_unit/algo_test.hpp"
#include "test_unit/create_coldef_frame.hpp"
#include "test_unit/create_err.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/create_meta.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"
#include "test_unit/create_query_frame.hpp"
#include "test_unit/mock_execution_processor.hpp"
#include "test_unit/printing.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
namespace asio = boost::asio;
using detail::any_execution_request;
using detail::connection_status;
using detail::resultset_encoding;

BOOST_AUTO_TEST_SUITE(test_start_execution)

struct fixture : algo_fixture_base
{
    mock_execution_processor proc;
    detail::start_execution_algo algo;

    fixture(
        any_execution_request req = any_execution_request("SELECT 1"),
        std::size_t max_bufsize = default_max_buffsize
    )
        : algo_fixture_base(max_bufsize), algo({req, &proc})
    {
    }

    fixture(bool is_top_level, connection_status initial_status)
        : algo({any_execution_request("SELECT 1"), &proc}, is_top_level)
    {
        st.status = initial_status;
    }
};

// Test cases to verify is_top_level = true and false.
// With is_top_level = false, status checks and transitions are not performed.
// Using connection_status::not_connected in this case helps verify that no transition is performed.
constexpr struct
{
    const char* name;
    bool is_top_level;
    connection_status initial_status;
} top_level_test_cases[] = {
    {"top_level",   true,  connection_status::ready        },
    {"subordinate", false, connection_status::not_connected},
};

//
// State transitions and errors
//
BOOST_AUTO_TEST_CASE(success_rows)
{
    for (const auto& tc : top_level_test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            fixture fix(tc.is_top_level, tc.initial_status);

            // Run the algo
            algo_test()
                .expect_write(create_query_frame(0, "SELECT 1"))
                .expect_read(create_frame(1, {0x01}))
                .expect_read(create_coldef_frame(2, meta_builder().type(column_type::varchar).build_coldef()))
                .will_set_status(
                    // Initiates a multi-function op. Transition performed only if is_top_level = true
                    tc.is_top_level ? connection_status::engaged_in_multi_function : fix.st.status
                )
                .check(fix);

            // Verify
            BOOST_TEST(fix.proc.is_reading_rows());
            check_meta(fix.proc.meta(), {column_type::varchar});
        }
    }
}

BOOST_AUTO_TEST_CASE(success_eof)
{
    for (const auto& tc : top_level_test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            fixture fix(tc.is_top_level, tc.initial_status);

            // Run the algo. No state transition happens here in either case
            // (the multi-function operation started and finished here)
            algo_test()
                .expect_write(create_query_frame(0, "SELECT 1"))
                .expect_read(create_ok_frame(1, ok_builder().build()))
                .check(fix);

            // Verify
            BOOST_TEST(fix.proc.is_complete());
        }
    }
}

BOOST_AUTO_TEST_CASE(error_network_write_request)
{
    for (const auto& tc : top_level_test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            fixture fix(tc.is_top_level, tc.initial_status);

            // Run the algo. If writing the request failed, the multi-function operation
            // isn't really started
            algo_test()
                .expect_write(create_query_frame(0, "SELECT 1"), asio::error::network_reset)
                .check(fix, asio::error::network_reset);
        }
    }
}

BOOST_AUTO_TEST_CASE(error_read_resultset_head)
{
    for (const auto& tc : top_level_test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            fixture fix(tc.is_top_level, tc.initial_status);

            // Run the algo. If is_top_level = false, no transition occurs.
            // Otherwise, the operation is started and finished straight away
            algo_test()
                .expect_write(create_query_frame(0, "SELECT 1"))
                .expect_read(err_builder()
                                 .seqnum(1)
                                 .code(common_server_errc::er_syntax_error)
                                 .message("Some error")
                                 .build_frame())
                .check(fix, common_server_errc::er_syntax_error, create_server_diag("Some error"));
        }
    }
}

BOOST_AUTO_TEST_CASE(error_max_buffer_size)
{
    // Setup
    std::string query(512, 'a');
    fixture fix(any_execution_request(query), 512u);

    // Run the algo
    algo_test().check(fix, client_errc::max_buffer_size_exceeded);
}

// Connection status checked correctly
BOOST_AUTO_TEST_CASE(error_invalid_connection_status)
{
    struct
    {
        connection_status status;
        error_code expected_err;
    } test_cases[] = {
        {connection_status::not_connected,             client_errc::not_connected            },
        {connection_status::engaged_in_multi_function, client_errc::engaged_in_multi_function},
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.status)
        {
            // Setup
            fixture fix;
            fix.st.status = tc.status;

            // Run the algo
            algo_test().check(fix, tc.expected_err);
        }
    }
}

//
// Different execution requests
//
BOOST_AUTO_TEST_CASE(text_query)
{
    // Setup
    fixture fix(any_execution_request("SELECT 1"));

    // Run the algo
    algo_test()
        .expect_write(create_query_frame(0, "SELECT 1"))
        .expect_read(create_frame(1, {0x01}))
        .expect_read(create_coldef_frame(2, meta_builder().type(column_type::varchar).build_coldef()))
        .will_set_status(detail::connection_status::engaged_in_multi_function)  // Starts a multi-function op
        .check(fix);

    // Verify
    BOOST_TEST(fix.proc.encoding() == resultset_encoding::text);
    BOOST_TEST(fix.proc.sequence_number() == 3u);
    BOOST_TEST(fix.proc.is_reading_rows());
    check_meta(fix.proc.meta(), {column_type::varchar});
    fix.proc.num_calls().reset(1).on_num_meta(1).on_meta(1).validate();
}

BOOST_AUTO_TEST_CASE(stmt_success)
{
    // Setup
    const auto params = make_fv_arr("test", nullptr);
    fixture fix(any_execution_request({std::uint32_t(1u), std::uint16_t(2u), params}));

    // Run the algo
    algo_test()
        .expect_write(create_frame(
            0,
            {
                0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
                0x01, 0xfe, 0x00, 0x06, 0x00, 0x04, 0x74, 0x65, 0x73, 0x74,
            }
        ))
        .expect_read(create_frame(1, {0x01}))
        .expect_read(create_coldef_frame(2, meta_builder().type(column_type::varchar).build_coldef()))
        .will_set_status(detail::connection_status::engaged_in_multi_function)  // Starts a multi-function op
        .check(fix);

    // Verify
    BOOST_TEST(fix.proc.encoding() == resultset_encoding::binary);
    BOOST_TEST(fix.proc.sequence_number() == 3u);
    BOOST_TEST(fix.proc.is_reading_rows());
    check_meta(fix.proc.meta(), {column_type::varchar});
    fix.proc.num_calls().reset(1).on_num_meta(1).on_meta(1).validate();
}

BOOST_AUTO_TEST_CASE(stmt_error_num_params)
{
    // Setup
    const auto params = make_fv_arr("test", nullptr, 42);  // too many params
    fixture fix(any_execution_request({std::uint32_t(1u), std::uint16_t(2u), params}));

    // Run the algo. Nothing should be written to the server
    algo_test().check(fix, client_errc::wrong_num_params);
}

BOOST_AUTO_TEST_CASE(with_params_success)
{
    // Setup
    const std::array<format_arg, 2> args{
        {{"", "abc"}, {"", 42}}
    };
    fixture fix(any_execution_request({"SELECT {}, {}", args}));
    fix.st.current_charset = utf8mb4_charset;

    // Run the algo
    algo_test()
        .expect_write(create_query_frame(0, "SELECT 'abc', 42"))
        .expect_read(create_ok_frame(1, ok_builder().build()))
        .check(fix);

    // Verify
    BOOST_TEST(fix.proc.encoding() == resultset_encoding::text);
    BOOST_TEST(fix.proc.sequence_number() == 2u);
    BOOST_TEST(fix.proc.is_complete());
    fix.proc.num_calls().reset(1).on_head_ok_packet(1).validate();
}

BOOST_AUTO_TEST_CASE(with_params_error_unknown_charset)
{
    // Setup
    const std::array<format_arg, 2> args{
        {{"", "abc"}, {"", 42}}
    };
    fixture fix(any_execution_request({"SELECT {}, {}", args}));
    fix.st.current_charset = {};

    // The algo fails immediately
    algo_test().check(fix, client_errc::unknown_character_set);
}

BOOST_AUTO_TEST_CASE(with_params_error_formatting)
{
    // Setup
    const std::array<format_arg, 1> args{{{"", "abc"}}};
    fixture fix(any_execution_request({"SELECT {}, {}", args}));
    fix.st.current_charset = utf8mb4_charset;

    // The algo fails immediately
    algo_test().check(fix, client_errc::format_arg_not_found);
}

BOOST_AUTO_TEST_SUITE_END()

//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/column_type.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/metadata_mode.hpp>

#include <boost/mysql/detail/any_execution_request.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/start_execution.hpp>

#include <boost/test/unit_test.hpp>

#include "test_common/assert_buffer_equals.hpp"
#include "test_common/check_meta.hpp"
#include "test_common/create_basic.hpp"
#include "test_unit/algo_test.hpp"
#include "test_unit/create_coldef_frame.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/create_meta.hpp"
#include "test_unit/create_statement.hpp"
#include "test_unit/mock_execution_processor.hpp"
#include "test_unit/printing.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using boost::mysql::detail::any_execution_request;
using boost::mysql::detail::resultset_encoding;

BOOST_AUTO_TEST_SUITE(test_start_execution)

struct fixture : algo_fixture_base
{
    mock_execution_processor proc;
    detail::start_execution_algo algo;

    fixture(any_execution_request req) : algo({&diag, req, &proc}) {}
};

BOOST_AUTO_TEST_CASE(text_query)
{
    // Setup
    fixture fix(any_execution_request("SELECT 1"));

    // Run the algo
    algo_test()
        .expect_write(create_frame(0, {0x03, 0x53, 0x45, 0x4c, 0x45, 0x43, 0x54, 0x20, 0x31}))
        .expect_read(create_frame(1, {0x01}))
        .expect_read(create_coldef_frame(2, meta_builder().type(column_type::varchar).build_coldef()))
        .check(fix);

    // Verify
    BOOST_TEST(fix.proc.encoding() == resultset_encoding::text);
    BOOST_TEST(fix.proc.sequence_number() == 3u);
    BOOST_TEST(fix.proc.is_reading_rows());
    check_meta(fix.proc.meta(), {column_type::varchar});
    fix.proc.num_calls().reset(1).on_num_meta(1).on_meta(1).validate();
}

BOOST_AUTO_TEST_CASE(prepared_statement)
{
    // Setup
    auto stmt = statement_builder().id(1).num_params(2).build();
    const auto params = make_fv_arr("test", nullptr);
    fixture fix(any_execution_request(stmt, params));

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
        .check(fix);

    // Verify
    BOOST_TEST(fix.proc.encoding() == resultset_encoding::binary);
    BOOST_TEST(fix.proc.sequence_number() == 3u);
    BOOST_TEST(fix.proc.is_reading_rows());
    check_meta(fix.proc.meta(), {column_type::varchar});
    fix.proc.num_calls().reset(1).on_num_meta(1).on_meta(1).validate();
}

BOOST_AUTO_TEST_CASE(error_num_params)
{
    // Setup
    auto stmt = statement_builder().id(1).num_params(2).build();
    const auto params = make_fv_arr("test", nullptr, 42);  // too many params
    fixture fix(any_execution_request(stmt, params));

    // Run the algo. Nothing should be written to the server
    algo_test().check(fix, client_errc::wrong_num_params);

    // We didn't modify the processor
    fix.proc.num_calls().validate();
}

// This covers errors in both writing the request and calling read_resultset_head
BOOST_AUTO_TEST_CASE(error_network_error)
{
    // check_network_errors<F>() requires F to be default-constructible
    struct query_fixture : fixture
    {
        query_fixture() : fixture(any_execution_request("SELECT 1")) {}
    };

    // This will test for errors writing the execution request
    // and reading the response and metadata (thus, calling read_resultset_head)
    algo_test()
        .expect_write(create_frame(0, {0x03, 0x53, 0x45, 0x4c, 0x45, 0x43, 0x54, 0x20, 0x31}))
        .expect_read(create_frame(1, {0x01}))
        .expect_read(create_coldef_frame(2, meta_builder().type(column_type::varchar).build_coldef()))
        .check_network_errors<query_fixture>();
}

BOOST_AUTO_TEST_SUITE_END()

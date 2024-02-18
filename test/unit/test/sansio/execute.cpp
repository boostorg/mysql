//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/column_type.hpp>

#include <boost/mysql/detail/any_execution_request.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/mysql/impl/internal/sansio/execute.hpp>

#include <boost/test/unit_test.hpp>

#include "test_common/assert_buffer_equals.hpp"
#include "test_common/buffer_concat.hpp"
#include "test_common/check_meta.hpp"
#include "test_unit/algo_test.hpp"
#include "test_unit/create_coldef_frame.hpp"
#include "test_unit/create_execution_processor.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/create_meta.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"
#include "test_unit/create_row_message.hpp"
#include "test_unit/create_statement.hpp"
#include "test_unit/mock_execution_processor.hpp"
#include "test_unit/printing.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
using boost::mysql::detail::any_execution_request;
using boost::mysql::detail::execution_processor;
using boost::mysql::detail::resultset_encoding;

BOOST_AUTO_TEST_SUITE(test_execute)

struct fixture : algo_fixture_base
{
    mock_execution_processor proc;
    detail::execute_algo algo;

    fixture(any_execution_request req = {"SELECT 1"}) : algo(st, {&diag, req, &proc}) {}
};

// The serialized form of a SELECT 1 query request
static constexpr std::uint8_t serialized_select_1[] = {0x03, 0x53, 0x45, 0x4c, 0x45, 0x43, 0x54, 0x20, 0x31};

BOOST_AUTO_TEST_CASE(eof)
{
    // Setup
    fixture fix;

    // Run the algo
    algo_test()
        .expect_write(create_frame(0, serialized_select_1))
        .expect_read(create_ok_frame(1, ok_builder().affected_rows(60u).info("abc").build()))
        .check(fix);

    // Verify
    fix.proc.num_calls().reset(1).on_head_ok_packet(1).validate();
    BOOST_TEST(fix.proc.encoding() == resultset_encoding::text);
    BOOST_TEST(fix.proc.affected_rows() == 60u);
    BOOST_TEST(fix.proc.info() == "abc");
}

BOOST_AUTO_TEST_CASE(single_row_batch)
{
    // Setup
    fixture fix;

    // Run the algo. Rows and OK are received in a single go (one call to read_some_rows)
    algo_test()
        .expect_write(create_frame(0, serialized_select_1))
        .expect_read(create_frame(1, {0x01}))  // OK, 1 column
        .expect_read(create_coldef_frame(2, meta_builder().type(column_type::bigint).build_coldef()))
        .expect_read(buffer_builder()
                         .add(create_text_row_message(3, 42))
                         .add(create_text_row_message(4, 43))
                         .add(create_eof_frame(5, ok_builder().affected_rows(10u).info("1st").build()))
                         .build())
        .check(fix);

    // Verify
    fix.proc.num_calls()
        .reset(1)
        .on_num_meta(1)
        .on_meta(1)
        .on_row_batch_start(1)
        .on_row(2)
        .on_row_batch_finish(1)
        .on_row_ok_packet(1)
        .validate();
    BOOST_TEST(fix.proc.encoding() == resultset_encoding::text);
    BOOST_TEST(fix.proc.num_meta() == 1u);
    check_meta(fix.proc.meta(), {column_type::bigint});
    BOOST_TEST(fix.proc.affected_rows() == 10u);
    BOOST_TEST(fix.proc.info() == "1st");
}

BOOST_AUTO_TEST_CASE(multiple_row_batches)
{
    // Setup
    fixture fix;

    // Run the algo. Multiple read_some_rows calls are required
    algo_test()
        .expect_write(create_frame(0, serialized_select_1))
        .expect_read(create_frame(1, {0x01}))  // OK, 1 column
        .expect_read(create_coldef_frame(2, meta_builder().type(column_type::tinyint).build_coldef()))
        .expect_read(create_text_row_message(3, 42))
        .expect_read(create_text_row_message(4, 43))
        .expect_read(create_eof_frame(5, ok_builder().affected_rows(10u).info("1st").build()))
        .check(fix);

    // Verify
    fix.proc.num_calls()
        .reset(1)
        .on_num_meta(1)
        .on_meta(1)
        .on_row_batch_start(3)
        .on_row(2)
        .on_row_batch_finish(3)
        .on_row_ok_packet(1)
        .validate();
    BOOST_TEST(fix.proc.encoding() == resultset_encoding::text);
    BOOST_TEST(fix.proc.num_meta() == 1u);
    check_meta(fix.proc.meta(), {column_type::tinyint});
    BOOST_TEST(fix.proc.affected_rows() == 10u);
    BOOST_TEST(fix.proc.info() == "1st");
}

BOOST_AUTO_TEST_CASE(multiple_resultsets)
{
    // Setup
    fixture fix;

    // Run the algo. Multiple read_some_rows calls are required
    algo_test()
        .expect_write(create_frame(0, serialized_select_1))
        .expect_read(create_frame(1, {0x01}))  // OK, 1 column
        .expect_read(create_coldef_frame(2, meta_builder().type(column_type::tinyint).build_coldef()))
        .expect_read(create_text_row_message(3, 42))
        .expect_read(
            create_eof_frame(4, ok_builder().affected_rows(10u).info("1st").more_results(true).build())
        )
        .expect_read(create_frame(5, {0x01}))  // OK, 1 column
        .expect_read(create_coldef_frame(6, meta_builder().type(column_type::varchar).build_coldef()))
        .expect_read(
            create_eof_frame(7, ok_builder().affected_rows(11u).info("2nd").more_results(true).build())
        )
        .expect_read(create_ok_frame(8, ok_builder().affected_rows(12u).info("3rd").build()))
        .check(fix);

    // Verify
    fix.proc.num_calls()
        .reset(1)
        .on_num_meta(2)
        .on_meta(2)
        .on_row_batch_start(3)
        .on_row(1)
        .on_row_batch_finish(3)
        .on_row_ok_packet(2)
        .on_head_ok_packet(1)
        .validate();
    BOOST_TEST(fix.proc.encoding() == resultset_encoding::text);
    BOOST_TEST(fix.proc.num_meta() == 1u);
    BOOST_TEST(fix.proc.affected_rows() == 12u);
    BOOST_TEST(fix.proc.info() == "3rd");
}

// Test that immediate completion with errors in start_execution are propagated correctlty
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

// Tests error on write, while reading head and while reading rows (error spotcheck)
BOOST_AUTO_TEST_CASE(error_network_error)
{
    algo_test()
        .expect_write(create_frame(0, serialized_select_1))
        .expect_read(create_frame(1, {0x01}))  // OK, 1 column
        .expect_read(create_coldef_frame(2, meta_builder().type(column_type::tinyint).build_coldef()))
        .expect_read(create_text_row_message(3, 42))
        .expect_read(create_text_row_message(4, 43))
        .expect_read(create_eof_frame(5, ok_builder().affected_rows(10u).info("1st").build()))
        .check_network_errors<fixture>();
}

BOOST_AUTO_TEST_SUITE_END()

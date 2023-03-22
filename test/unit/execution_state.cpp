//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/metadata_mode.hpp>

#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/test/unit_test.hpp>

#include "check_meta.hpp"
#include "creation/create_execution_state.hpp"
#include "creation/create_message_struct.hpp"
#include "test_common.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
using detail::protocol_field_type;

namespace {
BOOST_AUTO_TEST_SUITE(test_execution_state)

// The functionality has been tested in execution_state_impl already.
// Just spotchecks here
BOOST_AUTO_TEST_CASE(spotchecks)
{
    std::vector<field_view> fields;
    execution_state st;
    auto& impl = boost::mysql::detail::execution_state_access::get_impl(st);

    // Initial
    BOOST_TEST(st.should_start_op());
    BOOST_TEST(!st.should_read_head());
    BOOST_TEST(!st.should_read_rows());
    BOOST_TEST(!st.complete());
    BOOST_TEST(!st.has_ok_data());

    // Reset
    impl.reset(detail::resultset_encoding::text, &fields);
    BOOST_TEST(st.should_start_op());
    BOOST_TEST(!st.should_read_head());
    BOOST_TEST(!st.should_read_rows());
    BOOST_TEST(!st.complete());
    BOOST_TEST(!st.has_ok_data());

    // Reading meta
    impl.on_num_meta(1);
    BOOST_TEST(!st.should_start_op());
    BOOST_TEST(st.should_read_head());
    BOOST_TEST(!st.should_read_rows());
    BOOST_TEST(!st.complete());
    BOOST_TEST(!st.has_ok_data());

    // Reading rows
    impl.on_meta(create_coldef(protocol_field_type::bit), metadata_mode::full);
    BOOST_TEST(!st.should_start_op());
    BOOST_TEST(!st.should_read_head());
    BOOST_TEST(st.should_read_rows());
    BOOST_TEST(!st.complete());
    BOOST_TEST(!st.has_ok_data());
    check_meta(st.meta(), {column_type::bit});

    // Reading a row leaves it in the same state
    impl.on_row_batch_start();
    *impl.add_row() = field_view(42u);
    impl.on_row_batch_finish();
    BOOST_TEST(!st.should_start_op());
    BOOST_TEST(!st.should_read_head());
    BOOST_TEST(st.should_read_rows());
    BOOST_TEST(!st.complete());
    BOOST_TEST(!st.has_ok_data());
    BOOST_TEST(fields == make_fv_vector(42u));

    // End of first resultset
    impl.on_row_batch_start();
    impl.on_row_ok_packet(ok_builder()
                              .affected_rows(1)
                              .last_insert_id(2)
                              .warnings(4)
                              .info("abc")
                              .more_results(true)
                              .out_params(true)
                              .build());
    impl.on_row_batch_finish();
    BOOST_TEST(!st.should_start_op());
    BOOST_TEST(st.should_read_head());
    BOOST_TEST(!st.should_read_rows());
    BOOST_TEST(!st.complete());
    BOOST_TEST(st.has_ok_data());
    check_meta(st.meta(), {column_type::bit});
    BOOST_TEST(st.affected_rows() == 1u);
    BOOST_TEST(st.last_insert_id() == 2u);
    BOOST_TEST(st.warning_count() == 4u);
    BOOST_TEST(st.info() == "abc");
    BOOST_TEST(st.is_out_params());

    // Second resultset meta
    impl.on_num_meta(1);
    impl.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::full);
    BOOST_TEST(!st.should_start_op());
    BOOST_TEST(!st.should_read_head());
    BOOST_TEST(st.should_read_rows());
    BOOST_TEST(!st.complete());
    BOOST_TEST(!st.has_ok_data());
    check_meta(st.meta(), {column_type::tinyint});

    // Complete
    impl.on_row_batch_start();
    impl.on_row_ok_packet(ok_builder().affected_rows(5).last_insert_id(6).warnings(7).info("bhu").build());
    impl.on_row_batch_finish();
    BOOST_TEST(!st.should_start_op());
    BOOST_TEST(!st.should_read_head());
    BOOST_TEST(!st.should_read_rows());
    BOOST_TEST(st.complete());
    BOOST_TEST(st.has_ok_data());
    check_meta(st.meta(), {column_type::tinyint});
    BOOST_TEST(st.affected_rows() == 5u);
    BOOST_TEST(st.last_insert_id() == 6u);
    BOOST_TEST(st.warning_count() == 7u);
    BOOST_TEST(st.info() == "bhu");
    BOOST_TEST(!st.is_out_params());
}

// Verify that the lifetime guarantees we make are correct
BOOST_AUTO_TEST_CASE(move_constructor)
{
    // Construction
    execution_state st = exec_builder(false)
                             .meta({protocol_field_type::var_string})
                             .ok(ok_builder().info("small").build())
                             .build_state();

    // Obtain references
    auto meta = st.meta();
    auto info = st.info();

    // Move construct
    execution_state st2(std::move(st));
    st = execution_state();  // Regression check

    // Make sure that views are still valid
    check_meta(meta, {column_type::varchar});
    BOOST_TEST(info == "small");

    // The new object holds the same data
    BOOST_TEST_REQUIRE(st2.complete());
    check_meta(st2.meta(), {column_type::varchar});
    BOOST_TEST(st2.info() == "small");
}

BOOST_AUTO_TEST_CASE(move_assignment)
{
    // Construct an execution_state with value
    execution_state st = exec_builder(false)
                             .meta({protocol_field_type::var_string})
                             .ok(ok_builder().info("small").build())
                             .build_state();

    // Obtain references
    auto meta = st.meta();
    auto info = st.info();

    // Move assign
    execution_state st2;
    st2 = std::move(st);
    st = execution_state();  // Regression check - std::string impl SBO buffer

    // Make sure that views are still valid
    check_meta(meta, {column_type::varchar});
    BOOST_TEST(info == "small");

    // The new object holds the same data
    BOOST_TEST_REQUIRE(st2.complete());
    check_meta(st2.meta(), {column_type::varchar});
    BOOST_TEST(st2.info() == "small");
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

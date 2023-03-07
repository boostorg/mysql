//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/execution_state_impl.hpp>

#include <boost/core/span.hpp>

#include "create_message.hpp"
#include "test_common.hpp"

using boost::mysql::column_type;
using boost::mysql::field_view;
using boost::mysql::metadata_collection_view;
using boost::mysql::row_view;
using boost::mysql::rows_view;
using boost::mysql::detail::execution_state_impl;
using boost::mysql::detail::protocol_field_type;
using boost::mysql::detail::SERVER_MORE_RESULTS_EXISTS;

using boost::mysql::metadata_mode;
using boost::mysql::string_view;

using namespace boost::mysql::test;

/**
with append_mode = false
    + 1 resultset with data
    + 1 resultset with metadata but without rows
    + 1 resultset but empty
    2 resultsets with data
    2 resultsets, metadata without rows, then rows
    2 resultsets, rows, then metadata without rows
    2 resultsets, empty, then rows
    2 resultsets, rows, then empty
    2 resultsets, metadata without rows, then empty
    2 resultsets, empty, then metadata without rows
    2 resultsets, empty
    more than that
    reset coming from any other state
    resultsets with/without info
        with-with
        with-without
        without-with
        without-without
        with-without-with
    metadata mode
    info is actually copied
    ps output params (even if they're not in the right position)
 */

namespace {

// State checking
void check_should_read_head(const execution_state_impl& st)
{
    BOOST_TEST(st.should_read_head());
    BOOST_TEST(!st.should_read_meta());
    BOOST_TEST(!st.should_read_rows());
    BOOST_TEST(!st.complete());
}

void check_should_read_meta(const execution_state_impl& st)
{
    BOOST_TEST(!st.should_read_head());
    BOOST_TEST(st.should_read_meta());
    BOOST_TEST(!st.should_read_rows());
    BOOST_TEST(!st.complete());
}

void check_should_read_rows(const execution_state_impl& st)
{
    BOOST_TEST(!st.should_read_head());
    BOOST_TEST(!st.should_read_meta());
    BOOST_TEST(st.should_read_rows());
    BOOST_TEST(!st.complete());
}

void check_complete(const execution_state_impl& st)
{
    BOOST_TEST(!st.should_read_head());
    BOOST_TEST(!st.should_read_meta());
    BOOST_TEST(!st.should_read_rows());
    BOOST_TEST(st.complete());
}

// Metadata checking
void check_meta_r1(metadata_collection_view meta)
{
    BOOST_TEST_REQUIRE(meta.size() == 2);
    BOOST_TEST(meta[0].type() == column_type::tinyint);
    BOOST_TEST(meta[1].type() == column_type::varchar);
}

void check_meta_r2(metadata_collection_view meta)
{
    BOOST_TEST_REQUIRE(meta.size() == 1);
    BOOST_TEST(meta[0].type() == column_type::bit);
}

void check_meta_empty(metadata_collection_view meta) { BOOST_TEST(meta.size() == 0); }

// OK packet data checking
boost::mysql::detail::ok_packet create_ok_r1(std::uint16_t flags = 0)
{
    return create_ok_packet(1, 2, flags, 4, "Information");
}

boost::mysql::detail::ok_packet create_ok_r2(std::uint16_t flags = 0)
{
    return create_ok_packet(5, 6, flags | boost::mysql::detail::SERVER_PS_OUT_PARAMS, 8, "more_info");
}

void check_ok_r1(const execution_state_impl& st, std::size_t idx)
{
    BOOST_TEST(st.get_affected_rows(idx) == 1);
    BOOST_TEST(st.get_last_insert_id(idx) == 2);
    BOOST_TEST(st.get_warning_count(idx) == 4);
    BOOST_TEST(st.get_info(idx) == "Information");
    BOOST_TEST(st.get_is_out_params(idx) == false);
}

void check_ok_r2(const execution_state_impl& st, std::size_t idx)
{
    BOOST_TEST(st.get_affected_rows(idx) == 5);
    BOOST_TEST(st.get_last_insert_id(idx) == 6);
    BOOST_TEST(st.get_warning_count(idx) == 8);
    BOOST_TEST(st.get_info(idx) == "more_info");
    BOOST_TEST(st.get_is_out_params(idx) == true);
}

BOOST_AUTO_TEST_SUITE(test_execution_state_impl)

BOOST_AUTO_TEST_CASE(append_false_1resultset_data)
{
    // Initial
    execution_state_impl st(false);
    check_should_read_head(st);

    // Head indicates resultset with metadata
    st.on_num_meta(2);
    check_should_read_meta(st);

    // First metadata
    st.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::minimal);
    check_should_read_meta(st);

    // Second metadata, ready to read rows
    st.on_meta(create_coldef(protocol_field_type::var_string), metadata_mode::minimal);
    check_should_read_rows(st);
    check_meta_r1(st.current_resultset_meta());
    check_meta_r1(st.get_meta(0));

    // Read one row
    st.on_row();
    check_should_read_rows(st);
    check_meta_r1(st.current_resultset_meta());
    check_meta_r1(st.get_meta(0));

    // End of resultset
    st.on_row_ok_packet(create_ok_r1());
    check_complete(st);
    check_meta_r1(st.get_meta(0));
    check_ok_r1(st, 0);
}

BOOST_AUTO_TEST_CASE(append_false_1resultset_data_norows)
{
    // Initial
    execution_state_impl st(false);
    check_should_read_head(st);

    // Resultset head, same as previous test
    st.on_num_meta(2);
    st.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::minimal);
    st.on_meta(create_coldef(protocol_field_type::var_string), metadata_mode::minimal);
    check_should_read_rows(st);
    check_meta_r1(st.current_resultset_meta());
    check_meta_r1(st.get_meta(0));

    // Directly end of resultset, no rows
    st.on_row_ok_packet(create_ok_r1());
    check_meta_r1(st.get_meta(0));
    check_ok_r1(st, 0);
}

BOOST_AUTO_TEST_CASE(append_false_1resultset_empty)
{
    // Initial
    execution_state_impl st(false);
    check_should_read_head(st);

    // Directly end of resultet, no meta
    st.on_head_ok_packet(create_ok_r1());
    check_complete(st);
    check_meta_empty(st.get_meta(0));
    check_ok_r1(st, 0);
}

BOOST_AUTO_TEST_CASE(append_false_2resultsets_data_data)
{
    // Resultset r1: equivalent to single resultset case
    execution_state_impl st(false);
    check_should_read_head(st);
    st.on_num_meta(2);
    st.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::minimal);
    st.on_meta(create_coldef(protocol_field_type::var_string), metadata_mode::minimal);
    st.on_row();

    // OK packet indicates more results
    st.on_row_ok_packet(create_ok_r1(SERVER_MORE_RESULTS_EXISTS));
    check_should_read_head(st);
    check_meta_r1(st.get_meta(0));
    check_ok_r1(st, 0);

    // Resultset r2: indicates resultset with meta
    st.on_num_meta(1);
    check_should_read_meta(st);

    // First packet
    st.on_meta(create_coldef(protocol_field_type::bit), metadata_mode::minimal);
    check_should_read_rows(st);
    check_meta_r2(st.current_resultset_meta());
    check_meta_r2(st.get_meta(0));

    // First row
    st.on_row();
    check_should_read_rows(st);
    check_meta_r2(st.current_resultset_meta());
    check_meta_r2(st.get_meta(0));

    // OK packet, no more resultsets
    st.on_row_ok_packet(create_ok_r2());
    check_complete(st);
    check_meta_r2(st.get_meta(0));
    check_ok_r2(st, 0);
}

BOOST_AUTO_TEST_CASE(append_false_2resultsets_empty_data)
{
    // Resultset r1: same as the single case
    execution_state_impl st(false);
    st.on_head_ok_packet(create_ok_r1(SERVER_MORE_RESULTS_EXISTS));
    check_should_read_head(st);
    check_meta_empty(st.get_meta(0));
    check_ok_r1(st, 0);

    // Resultset r2: indicates data
    st.on_num_meta(1);
    check_should_read_meta(st);

    // Metadata packet
    st.on_meta(create_coldef(protocol_field_type::bit), metadata_mode::minimal);
    check_should_read_rows(st);
    check_meta_r2(st.current_resultset_meta());
    check_meta_r2(st.get_meta(0));

    // Rows
    st.on_row();
    check_should_read_rows(st);
    check_meta_r2(st.current_resultset_meta());
    check_meta_r2(st.get_meta(0));

    // Final OK packet
    st.on_row_ok_packet(create_ok_r2());
    check_complete(st);
    check_meta_r2(st.get_meta(0));
    check_ok_r2(st, 0);
}

BOOST_AUTO_TEST_CASE(append_false_2resultsets_data_empty)
{
    // Resultset r1: equivalent to single resultset case
    execution_state_impl st(false);
    st.on_num_meta(2);
    st.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::minimal);
    st.on_meta(create_coldef(protocol_field_type::var_string), metadata_mode::minimal);
    st.on_row();

    // OK packet indicates more results
    st.on_row_ok_packet(create_ok_r1(SERVER_MORE_RESULTS_EXISTS));
    check_should_read_head(st);
    check_meta_r1(st.get_meta(0));
    check_ok_r1(st, 0);

    // OK packet for 2nd result
    st.on_head_ok_packet(create_ok_r2());
    check_complete(st);
    check_meta_empty(st.get_meta(0));
    check_ok_r2(st, 0);
}

BOOST_AUTO_TEST_CASE(append_false_2resultsets_empty_empty)
{
    // Resultset r1: equivalent to single resultset case
    execution_state_impl st(false);

    // OK packet indicates more results
    st.on_head_ok_packet(create_ok_r1(SERVER_MORE_RESULTS_EXISTS));
    check_should_read_head(st);
    check_meta_empty(st.get_meta(0));
    check_ok_r1(st, 0);

    // OK packet for 2nd result
    st.on_head_ok_packet(create_ok_r2());
    check_complete(st);
    check_meta_empty(st.get_meta(0));
    check_ok_r2(st, 0);
}

// empty-empty-data
//

//
// ----- append true
//

// BOOST_AUTO_TEST_CASE(append_true_1resultset_data)
// {
//     execution_state_impl st(true);
//     BOOST_TEST(st.should_read_head());

//     st.on_num_meta(2);
//     BOOST_TEST(st.should_read_meta());

//     st.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::minimal);
//     BOOST_TEST(st.should_read_meta());

//     st.on_meta(create_coldef(protocol_field_type::var_string), metadata_mode::minimal);
//     BOOST_TEST(st.should_read_rows());
//     check_meta_r1(st.current_resultset_meta());

//     auto fvarr = make_fv_arr(42, "abc");
//     st.rows().assign(fvarr.data(), fvarr.size());
//     st.on_row();
//     BOOST_TEST(st.should_read_rows());
//     check_meta_r1(st.current_resultset_meta());

//     st.on_row_ok_packet(create_ok_packet(1, 2, 0, 4, "Information"));
//     BOOST_TEST(st.complete());
//     BOOST_TEST(st.num_resultsets() == 1);
//     check_meta_r1(st.get_meta(0));
//     BOOST_TEST(st.get_rows(0) == makerows(2, 42, "abc"));
//     BOOST_TEST(st.get_affected_rows(0) == 1);
//     BOOST_TEST(st.get_last_insert_id(0) == 2);
//     BOOST_TEST(st.get_warning_count(0) == 4);
//     BOOST_TEST(st.get_info(0) == "Information");
//     BOOST_TEST(st.get_is_out_params(0) == false);
//     BOOST_TEST(st.get_out_params() == row_view());
// }

// BOOST_AUTO_TEST_CASE(append_true_1resultset_data_norows)
// {
//     execution_state_impl st(true);
//     BOOST_TEST(st.should_read_head());

//     st.on_num_meta(2);
//     BOOST_TEST(st.should_read_meta());

//     st.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::minimal);
//     BOOST_TEST(st.should_read_meta());

//     st.on_meta(create_coldef(protocol_field_type::var_string), metadata_mode::minimal);
//     BOOST_TEST(st.should_read_rows());
//     check_meta_r1(st.current_resultset_meta());

//     st.on_row_ok_packet(create_ok_packet(1, 2, 0, 4, "Information"));
//     BOOST_TEST(st.complete());
//     BOOST_TEST(st.num_resultsets() == 1);
//     check_meta_r1(st.get_meta(0));
//     BOOST_TEST(st.get_rows(0) == rows_view());
//     BOOST_TEST(st.get_affected_rows(0) == 1);
//     BOOST_TEST(st.get_last_insert_id(0) == 2);
//     BOOST_TEST(st.get_warning_count(0) == 4);
//     BOOST_TEST(st.get_info(0) == "Information");
//     BOOST_TEST(st.get_is_out_params(0) == false);
//     BOOST_TEST(st.get_out_params() == row_view());
// }

// BOOST_AUTO_TEST_CASE(append_true_1resultset_empty)
// {
//     execution_state_impl st(true);
//     BOOST_TEST(st.should_read_head());

//     st.on_row_ok_packet(create_ok_packet(1, 2, 0, 4, "Information"));
//     BOOST_TEST(st.complete());
//     BOOST_TEST(st.num_resultsets() == 1);
//     check_meta_r1(st.get_meta(0));
//     BOOST_TEST(st.get_rows(0) == row_view());
//     BOOST_TEST(st.get_affected_rows(0) == 1);
//     BOOST_TEST(st.get_last_insert_id(0) == 2);
//     BOOST_TEST(st.get_warning_count(0) == 4);
//     BOOST_TEST(st.get_info(0) == "Information");
//     BOOST_TEST(st.get_is_out_params(0) == false);
//     BOOST_TEST(st.get_out_params() == row_view());
// }

// BOOST_AUTO_TEST_CASE(append_true_2resultsets_data_data)
// {
//     execution_state_impl st(true);
//     BOOST_TEST(st.should_read_head());

//     st.on_num_meta(2);
//     BOOST_TEST(st.should_read_meta());

//     st.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::minimal);
//     BOOST_TEST(st.should_read_meta());

//     st.on_meta(create_coldef(protocol_field_type::var_string), metadata_mode::minimal);
//     BOOST_TEST(st.should_read_rows());
//     check_meta_r1(st.current_resultset_meta());

//     auto fvarr = make_fv_arr(42, "abc");
//     st.rows().assign(fvarr.data(), fvarr.size());
//     st.on_row();
//     BOOST_TEST(st.should_read_rows());
//     check_meta_r1(st.current_resultset_meta());

//     st.on_row_ok_packet(create_ok_packet(1, 2, SERVER_MORE_RESULTS_EXISTS, 4, "Information"));
//     BOOST_TEST(st.should_read_head());

//     st.on_num_meta(1);
//     BOOST_TEST(st.should_read_meta());

//     st.on_meta(create_coldef(protocol_field_type::bit), metadata_mode::minimal);
//     BOOST_TEST(st.should_read_rows());
//     check_meta_r2(st.current_resultset_meta());

//     auto* fields = st.rows().add_fields(2);
//     fields[0] = field_view(50);
//     fields[1] = field_view(100);
//     st.on_row();
//     BOOST_TEST(st.should_read_rows());
//     check_meta_r2(st.current_resultset_meta());
//     st.on_row();
//     BOOST_TEST(st.should_read_rows());
//     check_meta_r2(st.current_resultset_meta());

//     st.on_row_ok_packet(create_ok_packet(6, 7, 0, 9, "more_info"));
//     BOOST_TEST(st.complete());
//     BOOST_TEST(st.num_resultsets() == 2);
//     check_meta_r1(st.get_meta(0));
//     check_meta_r2(st.get_meta(1));
//     BOOST_TEST(st.get_rows(0) == makerows(2, 42, "abc"));
//     BOOST_TEST(st.get_rows(1) == makerows(1, 50, 100));
//     BOOST_TEST(st.get_affected_rows(0) == 1);
//     BOOST_TEST(st.get_affected_rows(1) == 6);
//     BOOST_TEST(st.get_last_insert_id(0) == 2);
//     BOOST_TEST(st.get_last_insert_id(1) == 7);
//     BOOST_TEST(st.get_warning_count(0) == 4);
//     BOOST_TEST(st.get_warning_count(1) == 9);
//     BOOST_TEST(st.get_info(0) == "Information");
//     BOOST_TEST(st.get_info(1) == "more_info");
//     BOOST_TEST(st.get_is_out_params(0) == false);
//     BOOST_TEST(st.get_out_params() == row_view());
// }

// BOOST_AUTO_TEST_CASE(append_false_2resultsets_norow_data)
// {
//     execution_state_impl st(false);
//     BOOST_TEST(st.should_read_head());

//     st.on_num_meta(2);
//     BOOST_TEST(st.should_read_meta());

//     st.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::minimal);
//     BOOST_TEST(st.should_read_meta());

//     st.on_meta(create_coldef(protocol_field_type::var_string), metadata_mode::minimal);
//     BOOST_TEST(st.should_read_rows());
//     check_2fields_meta(st.current_resultset_meta());
//     check_2fields_meta(st.get_meta(0));

//     st.on_row_ok_packet(create_ok_packet(1, 2, SERVER_MORE_RESULTS_EXISTS, 4, "Information"));
//     BOOST_TEST(st.should_read_head());
//     check_2fields_meta(st.get_meta(0));
//     BOOST_TEST(st.get_affected_rows(0) == 1);
//     BOOST_TEST(st.get_last_insert_id(0) == 2);
//     BOOST_TEST(st.get_warning_count(0) == 4);
//     BOOST_TEST(st.get_info(0) == "Information");
//     BOOST_TEST(st.get_is_out_params(0) == false);

//     st.on_num_meta(1);
//     BOOST_TEST(st.should_read_meta());

//     st.on_meta(create_coldef(protocol_field_type::bit), metadata_mode::minimal);
//     BOOST_TEST(st.should_read_rows());
//     check_1fields_meta(st.current_resultset_meta());
//     check_1fields_meta(st.get_meta(0));

//     st.on_row();
//     BOOST_TEST(st.should_read_rows());
//     check_1fields_meta(st.current_resultset_meta());
//     check_1fields_meta(st.get_meta(0));

//     st.on_row_ok_packet(create_ok_packet(6, 7, 0, 9, "more_info"));
//     BOOST_TEST(st.complete());
//     check_1fields_meta(st.get_meta(0));
//     BOOST_TEST(st.get_affected_rows(0) == 6);
//     BOOST_TEST(st.get_last_insert_id(0) == 7);
//     BOOST_TEST(st.get_warning_count(0) == 9);
//     BOOST_TEST(st.get_info(0) == "more_info");
//     BOOST_TEST(st.get_is_out_params(0) == false);
// }

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
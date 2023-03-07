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
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/core/span.hpp>
#include <boost/test/unit_test_suite.hpp>

#include "create_message.hpp"
#include "printing.hpp"
#include "test_common.hpp"

using boost::mysql::column_type;
using boost::mysql::metadata_collection_view;
using boost::mysql::row_view;
using boost::mysql::rows_view;
using boost::mysql::detail::execution_state_impl;
using boost::mysql::detail::protocol_field_type;
using boost::mysql::detail::SERVER_MORE_RESULTS_EXISTS;

using boost::mysql::metadata_mode;
using boost::mysql::string_view;
using boost::mysql::detail::resultset_encoding;

using namespace boost::mysql::test;

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

void check_meta_r3(metadata_collection_view meta)
{
    BOOST_TEST_REQUIRE(meta.size() == 3);
    BOOST_TEST(meta[0].type() == column_type::float_);
    BOOST_TEST(meta[1].type() == column_type::double_);
    BOOST_TEST(meta[2].type() == column_type::tinyint);
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

boost::mysql::detail::ok_packet create_ok_r3(std::uint16_t flags = 0)
{
    return create_ok_packet(10, 11, flags, 12, "");
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

void check_ok_r3(const execution_state_impl& st, std::size_t idx)
{
    BOOST_TEST(st.get_affected_rows(idx) == 10);
    BOOST_TEST(st.get_last_insert_id(idx) == 11);
    BOOST_TEST(st.get_warning_count(idx) == 12);
    BOOST_TEST(st.get_info(idx) == "");
    BOOST_TEST(st.get_is_out_params(idx) == false);
}

// Rows. Note that this doesn't handle copying strings into the internal rows -
// this is not the responsibility of this component
template <class... Args>
void add_fields(execution_state_impl& st, const Args&... args)
{
    auto fields = make_fv_arr(args...);
    auto* storage = st.rows().add_fields(fields.size());
    for (std::size_t i = 0; i < fields.size(); ++i)
    {
        storage[i] = fields[i];
    }
}

BOOST_AUTO_TEST_SUITE(test_execution_state_impl)

BOOST_AUTO_TEST_SUITE(append_false)

BOOST_AUTO_TEST_CASE(one_resultset_data)
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

BOOST_AUTO_TEST_CASE(one_resultset_norows)
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

BOOST_AUTO_TEST_CASE(one_resultset_empty)
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

BOOST_AUTO_TEST_CASE(two_resultsets_data_data)
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

BOOST_AUTO_TEST_CASE(two_resultsets_empty_data)
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

BOOST_AUTO_TEST_CASE(two_resultsets_data_empty)
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

BOOST_AUTO_TEST_CASE(two_resultsets_empty_empty)
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

BOOST_AUTO_TEST_CASE(three_resultsets_empty_empty_data)
{
    // Two first resultsets
    execution_state_impl st(false);
    st.on_head_ok_packet(create_ok_r1(SERVER_MORE_RESULTS_EXISTS));
    st.on_head_ok_packet(create_ok_r2(SERVER_MORE_RESULTS_EXISTS));
    check_should_read_head(st);
    check_meta_empty(st.get_meta(0));
    check_ok_r2(st, 0);

    // Resultset r3: head indicates resultset with metadata
    st.on_num_meta(3);
    check_should_read_meta(st);

    // Metadata
    st.on_meta(create_coldef(protocol_field_type::float_), metadata_mode::minimal);
    check_should_read_meta(st);

    st.on_meta(create_coldef(protocol_field_type::double_), metadata_mode::minimal);
    check_should_read_meta(st);

    st.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::minimal);
    check_should_read_rows(st);
    check_meta_r3(st.current_resultset_meta());
    check_meta_r3(st.get_meta(0));

    // Read one row
    st.on_row();
    check_should_read_rows(st);
    check_meta_r3(st.current_resultset_meta());
    check_meta_r3(st.get_meta(0));

    // End of resultset
    st.on_row_ok_packet(create_ok_r3());
    check_complete(st);
    check_meta_r3(st.get_meta(0));
    check_ok_r3(st, 0);
}

BOOST_AUTO_TEST_CASE(three_resultsets_data_empty_data)
{
    // Two first resultsets
    execution_state_impl st(false);
    st.on_num_meta(2);
    st.on_meta(create_coldef(protocol_field_type::float_), metadata_mode::minimal);
    st.on_meta(create_coldef(protocol_field_type::double_), metadata_mode::minimal);
    st.on_row_ok_packet(create_ok_r1(SERVER_MORE_RESULTS_EXISTS));
    st.on_head_ok_packet(create_ok_r2(SERVER_MORE_RESULTS_EXISTS));
    check_should_read_head(st);
    check_meta_empty(st.get_meta(0));
    check_ok_r2(st, 0);

    // Resultset r3: head indicates resultset with metadata
    st.on_num_meta(3);
    check_should_read_meta(st);

    // Metadata
    st.on_meta(create_coldef(protocol_field_type::float_), metadata_mode::minimal);
    st.on_meta(create_coldef(protocol_field_type::double_), metadata_mode::minimal);
    st.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::minimal);
    check_should_read_rows(st);
    check_meta_r3(st.current_resultset_meta());
    check_meta_r3(st.get_meta(0));

    // End of resultset
    st.on_row_ok_packet(create_ok_r3());
    check_complete(st);
    check_meta_r3(st.get_meta(0));
    check_ok_r3(st, 0);
}

BOOST_AUTO_TEST_CASE(info_string_ownserhip)
{
    execution_state_impl st(false);

    // OK packet received, doesn't own the string
    std::string info = "Some info";
    st.on_head_ok_packet(create_ok_packet(0, 0, SERVER_MORE_RESULTS_EXISTS, 0, info));

    // st does, so changing info doesn't affect
    info = "other info";
    BOOST_TEST(st.get_info(0) == "Some info");

    // Repeat the process for row OK packet
    st.on_num_meta(1);
    st.on_meta(create_coldef(protocol_field_type::bit), metadata_mode::full);
    st.on_row_ok_packet(create_ok_packet(0, 0, 0, 0, info));
    info = "abcdfefgh";
    BOOST_TEST(st.get_info(0) == "other info");
}

BOOST_AUTO_TEST_CASE(reset)
{
    execution_state_impl st(false);

    // From intermediate state
    st.reset(resultset_encoding::binary);
    st.on_num_meta(4);
    st.on_meta(create_coldef(protocol_field_type::bit), metadata_mode::full);
    st.sequence_number() = 80;

    // Reset
    st.reset(resultset_encoding::text);
    check_should_read_head(st);
    BOOST_TEST(!st.is_append_mode());  // reset doesn't clear append mode
    BOOST_TEST(st.sequence_number() == 0);
    BOOST_TEST(st.encoding() == resultset_encoding::text);

    // Can be used normally
    st.on_num_meta(1);
    st.on_meta(create_coldef(protocol_field_type::float_), metadata_mode::full);
    check_meta_r1(st.current_resultset_meta());
    st.on_row_ok_packet(create_ok_r1());
    check_complete(st);
    check_meta_r1(st.get_meta(0));
    check_ok_r1(st, 0);
}
BOOST_AUTO_TEST_SUITE_END()  // append_false

//
// append true
//
BOOST_AUTO_TEST_SUITE(append_true)

BOOST_AUTO_TEST_CASE(one_resultset_data)
{
    // Initial
    execution_state_impl st(true);
    check_should_read_head(st);

    // Head indicates resultset with two columns
    st.on_num_meta(2);
    check_should_read_meta(st);

    // First meta
    st.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::minimal);
    check_should_read_meta(st);

    // Second meta, ready to read rows
    st.on_meta(create_coldef(protocol_field_type::var_string), metadata_mode::minimal);
    check_should_read_rows(st);
    check_meta_r1(st.current_resultset_meta());

    // Row
    add_fields(st, 42, "abc");
    st.on_row();
    check_should_read_rows(st);
    check_meta_r1(st.current_resultset_meta());

    // End of resultset
    st.on_row_ok_packet(create_ok_r1());
    check_complete(st);
    check_meta_r1(st.get_meta(0));
    check_ok_r1(st, 0);
    BOOST_TEST(st.num_resultsets() == 1);
    BOOST_TEST(st.get_rows(0) == makerows(2, 42, "abc"));
    BOOST_TEST(st.get_out_params() == row_view());
}

BOOST_AUTO_TEST_CASE(one_resultset_norows)
{
    // Initial
    execution_state_impl st(true);
    check_should_read_head(st);

    // Metadata
    st.on_num_meta(2);
    st.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::minimal);
    st.on_meta(create_coldef(protocol_field_type::var_string), metadata_mode::minimal);
    check_should_read_rows(st);
    check_meta_r1(st.current_resultset_meta());

    // End of resultset
    st.on_row_ok_packet(create_ok_r1());
    check_complete(st);
    check_meta_r1(st.get_meta(0));
    check_ok_r1(st, 0);
    BOOST_TEST(st.num_resultsets() == 1);
    BOOST_TEST(st.get_rows(0) == makerows(2));  // empty but with 2 columns
    BOOST_TEST(st.get_out_params() == row_view());
}

BOOST_AUTO_TEST_CASE(one_resultset_empty)
{
    // Initial
    execution_state_impl st(true);
    check_should_read_head(st);

    // End of resultset
    st.on_head_ok_packet(create_ok_r1());
    check_complete(st);
    check_meta_empty(st.get_meta(0));
    check_ok_r1(st, 0);
    BOOST_TEST(st.num_resultsets() == 1);
    BOOST_TEST(st.get_rows(0) == rows_view());
    BOOST_TEST(st.get_out_params() == row_view());
}

BOOST_AUTO_TEST_CASE(two_resultsets_data_data)
{
    // Resultset r1: equivalent to single resultset case
    execution_state_impl st(true);
    st.on_num_meta(2);
    st.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::minimal);
    st.on_meta(create_coldef(protocol_field_type::var_string), metadata_mode::minimal);
    add_fields(st, 42, "abc", 50, "def");
    st.on_row();
    st.on_row();

    // OK packet indicates more results
    st.on_row_ok_packet(create_ok_r1(SERVER_MORE_RESULTS_EXISTS));
    check_should_read_head(st);

    // Resultset r2: indicates resultset with meta
    st.on_num_meta(1);
    check_should_read_meta(st);

    // Meta
    st.on_meta(create_coldef(protocol_field_type::bit), metadata_mode::minimal);
    check_should_read_rows(st);
    check_meta_r2(st.current_resultset_meta());

    // Row
    add_fields(st, 70);
    st.on_row();
    check_should_read_rows(st);

    // OK packet, no more resultsets
    st.on_row_ok_packet(create_ok_r2());
    check_complete(st);
    check_meta_r1(st.get_meta(0));
    check_meta_r2(st.get_meta(1));
    check_ok_r1(st, 0);
    check_ok_r2(st, 1);
    BOOST_TEST(st.num_resultsets() == 2);
    BOOST_TEST(st.get_rows(0) == makerows(2, 42, "abc", 50, "def"));
    BOOST_TEST(st.get_rows(1) == makerows(1, 70));
    BOOST_TEST(st.get_out_params() == makerow(70));
}

BOOST_AUTO_TEST_CASE(two_resultsets_empty_data)
{
    // Resultset r1: same as the single case
    execution_state_impl st(true);
    st.on_head_ok_packet(create_ok_r1(SERVER_MORE_RESULTS_EXISTS));
    check_should_read_head(st);

    // Resultset r2: indicates data
    st.on_num_meta(1);
    check_should_read_meta(st);

    // Metadata packet
    st.on_meta(create_coldef(protocol_field_type::bit), metadata_mode::minimal);
    check_should_read_rows(st);
    check_meta_r2(st.current_resultset_meta());

    // Rows
    add_fields(st, 70);
    st.on_row();
    check_should_read_rows(st);

    // Final OK packet
    st.on_row_ok_packet(create_ok_r2());
    check_complete(st);
    check_meta_empty(st.get_meta(0));
    check_meta_r2(st.get_meta(1));
    check_ok_r1(st, 0);
    check_ok_r2(st, 1);
    BOOST_TEST(st.num_resultsets() == 2);
    BOOST_TEST(st.get_rows(0) == rows_view());
    BOOST_TEST(st.get_rows(1) == makerows(1, 70));
    BOOST_TEST(st.get_out_params() == makerow(70));
}

// Note: this tests also an edge case where a resultset indicates
// that contains OUT parameters but is empty
BOOST_AUTO_TEST_CASE(two_resultsets_data_empty)
{
    // Resultset r1: equivalent to single resultset case
    execution_state_impl st(true);
    st.on_num_meta(2);
    st.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::minimal);
    st.on_meta(create_coldef(protocol_field_type::var_string), metadata_mode::minimal);
    add_fields(st, 42, "abc", 50, "def");
    st.on_row();
    st.on_row();

    // OK packet indicates more results
    st.on_row_ok_packet(create_ok_r1(SERVER_MORE_RESULTS_EXISTS));
    check_should_read_head(st);

    // OK packet for 2nd result
    st.on_head_ok_packet(create_ok_r2());
    check_complete(st);
    check_meta_r1(st.get_meta(0));
    check_meta_empty(st.get_meta(1));
    check_ok_r1(st, 0);
    check_ok_r2(st, 1);
    BOOST_TEST(st.num_resultsets() == 2);
    BOOST_TEST(st.get_rows(0) == makerows(2, 42, "abc", 50, "def"));
    BOOST_TEST(st.get_rows(1) == rows_view());
    BOOST_TEST(st.get_out_params() == row_view());
}

BOOST_AUTO_TEST_CASE(two_resultsets_empty_empty)
{
    // Resultset r1: equivalent to single resultset case
    execution_state_impl st(true);
    st.on_head_ok_packet(create_ok_r1(SERVER_MORE_RESULTS_EXISTS));
    check_should_read_head(st);

    // OK packet for 2nd result
    st.on_head_ok_packet(create_ok_r2());
    check_complete(st);
    check_meta_empty(st.get_meta(0));
    check_meta_empty(st.get_meta(1));
    check_ok_r1(st, 0);
    check_ok_r2(st, 1);
    BOOST_TEST(st.num_resultsets() == 2);
    BOOST_TEST(st.get_rows(0) == rows_view());
    BOOST_TEST(st.get_rows(1) == rows_view());
    BOOST_TEST(st.get_out_params() == row_view());
}

BOOST_AUTO_TEST_CASE(three_resultsets_empty_empty_data)
{
    // Two first resultsets
    execution_state_impl st(true);
    st.on_head_ok_packet(create_ok_r1(SERVER_MORE_RESULTS_EXISTS));
    st.on_head_ok_packet(create_ok_r2(SERVER_MORE_RESULTS_EXISTS));
    check_should_read_head(st);

    // Resultset r3: head indicates resultset with metadata
    st.on_num_meta(3);
    check_should_read_meta(st);

    // Metadata
    st.on_meta(create_coldef(protocol_field_type::float_), metadata_mode::minimal);
    check_should_read_meta(st);

    st.on_meta(create_coldef(protocol_field_type::double_), metadata_mode::minimal);
    check_should_read_meta(st);

    st.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::minimal);
    check_should_read_rows(st);
    check_meta_r3(st.current_resultset_meta());

    // Read rows
    add_fields(st, 4.2f, 5.0, 8, 42.0f, 50.0, 80);
    st.on_row();
    check_should_read_rows(st);
    st.on_row();
    check_should_read_rows(st);

    // End of resultset
    st.on_row_ok_packet(create_ok_r3());
    check_complete(st);
    check_meta_empty(st.get_meta(0));
    check_meta_empty(st.get_meta(1));
    check_meta_r3(st.get_meta(2));
    check_ok_r1(st, 0);
    check_ok_r2(st, 1);
    check_ok_r3(st, 2);
    BOOST_TEST(st.num_resultsets() == 3);
    BOOST_TEST(st.get_rows(0) == rows_view());
    BOOST_TEST(st.get_rows(1) == rows_view());
    BOOST_TEST(st.get_rows(2) == makerows(3, 4.2f, 5.0, 8, 42.0f, 50.0, 80));
    BOOST_TEST(st.get_out_params() == row_view());
}

// Verify that we do row slicing correctly
BOOST_AUTO_TEST_CASE(three_resultsets_data_data_data)
{
    // First resultset
    execution_state_impl st(true);
    st.on_num_meta(2);
    st.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::minimal);
    st.on_meta(create_coldef(protocol_field_type::var_string), metadata_mode::minimal);
    add_fields(st, 42, "abc", 50, "def");
    st.on_row();
    st.on_row();
    st.on_row_ok_packet(create_ok_r1(SERVER_MORE_RESULTS_EXISTS));

    // Second resultset
    st.on_num_meta(1);
    st.on_meta(create_coldef(protocol_field_type::bit), metadata_mode::minimal);
    add_fields(st, 60);
    st.on_row();
    st.on_row_ok_packet(create_ok_r2(SERVER_MORE_RESULTS_EXISTS));

    // Third resultset
    st.on_num_meta(3);
    st.on_meta(create_coldef(protocol_field_type::float_), metadata_mode::minimal);
    st.on_meta(create_coldef(protocol_field_type::double_), metadata_mode::minimal);
    st.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::minimal);
    add_fields(st, 4.2f, 5.0, 8, 42.0f, 50.0, 80);
    st.on_row();
    st.on_row();
    st.on_row_ok_packet(create_ok_r3());

    // Check results
    check_complete(st);
    check_meta_r1(st.get_meta(0));
    check_meta_r2(st.get_meta(1));
    check_meta_r3(st.get_meta(2));
    check_ok_r1(st, 0);
    check_ok_r2(st, 1);
    check_ok_r3(st, 2);
    BOOST_TEST(st.num_resultsets() == 3);
    BOOST_TEST(st.get_rows(0) == makerows(2, 42, "abc", 50, "def"));
    BOOST_TEST(st.get_rows(1) == makerows(1, 60));
    BOOST_TEST(st.get_rows(2) == makerows(3, 4.2f, 5.0, 8, 42.0f, 50.0, 80));
    BOOST_TEST(st.get_out_params() == makerow(60));
}

BOOST_AUTO_TEST_CASE(info_string_ownserhip)
{
    execution_state_impl st(true);

    // Head OK packet
    std::string info = "Some info";
    st.on_head_ok_packet(create_ok_packet(0, 0, SERVER_MORE_RESULTS_EXISTS, 0, info));

    // Empty OK packet
    info = "";
    st.on_head_ok_packet(create_ok_packet(0, 0, SERVER_MORE_RESULTS_EXISTS, 0, info));

    // Row OK packet
    info = "other info";
    st.on_num_meta(1);
    st.on_meta(create_coldef(protocol_field_type::bit), metadata_mode::full);
    st.on_row_ok_packet(create_ok_packet(0, 0, 0, 0, info));
    info = "abcdfefgh";
    BOOST_TEST(st.get_info(0) == "Some info");
    BOOST_TEST(st.get_info(1) == "");
    BOOST_TEST(st.get_info(2) == "other info");
}

BOOST_AUTO_TEST_CASE(reset_from_intermediate)
{
    execution_state_impl st(true);

    // From intermediate state
    st.reset(resultset_encoding::binary);
    st.on_num_meta(4);
    st.on_meta(create_coldef(protocol_field_type::bit), metadata_mode::full);
    st.sequence_number() = 80;

    // Reset
    st.reset(resultset_encoding::text);
    check_should_read_head(st);
    BOOST_TEST(st.is_append_mode());  // reset doesn't clear append mode
    BOOST_TEST(st.sequence_number() == 0);
    BOOST_TEST(st.encoding() == resultset_encoding::text);

    // Can be used normally
    st.on_num_meta(1);
    st.on_meta(create_coldef(protocol_field_type::float_), metadata_mode::full);
    st.on_row_ok_packet(create_ok_r1());

    // Verify that there were no leftovers
    check_complete(st);
    check_meta_r1(st.get_meta(0));
    check_ok_r1(st, 0);
    BOOST_TEST(st.num_resultsets() == 1);
    BOOST_TEST(st.get_rows(0) == makerows(1));
}

BOOST_AUTO_TEST_CASE(reset_from_complete)
{
    // Run to completion
    execution_state_impl st(true);
    st.on_num_meta(2);
    st.on_meta(create_coldef(protocol_field_type::float_), metadata_mode::full);
    st.on_meta(create_coldef(protocol_field_type::double_), metadata_mode::full);
    add_fields(st, 4.2f, 4.2, 42.0f, 42.0);
    st.on_row();
    st.on_row();
    st.on_row_ok_packet(create_ok_packet(1, 2, SERVER_MORE_RESULTS_EXISTS, 4, "abc"));

    st.on_head_ok_packet(create_ok_packet(5, 6, SERVER_MORE_RESULTS_EXISTS, 7, "fgh"));

    st.on_num_meta(1);
    st.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::full);
    add_fields(st, 50);
    st.on_row();

    // Reset
    st.reset(resultset_encoding::binary);
    check_should_read_head(st);
    BOOST_TEST(st.is_append_mode());  // reset doesn't clear append mode
    BOOST_TEST(st.sequence_number() == 0);
    BOOST_TEST(st.encoding() == resultset_encoding::binary);

    // Can be used normally
    st.on_num_meta(1);
    st.on_meta(create_coldef(protocol_field_type::float_), metadata_mode::full);
    add_fields(st, 4.2f, 50.0f);
    st.on_row();
    st.on_row();
    st.on_row_ok_packet(create_ok_r1());

    // Verify that there were no leftovers
    check_complete(st);
    check_meta_r1(st.get_meta(0));
    check_ok_r1(st, 0);
    BOOST_TEST(st.num_resultsets() == 1);
    BOOST_TEST(st.get_rows(0) == makerows(1, 42.0f, 50.0f));
}
BOOST_AUTO_TEST_SUITE_END()  // append true

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
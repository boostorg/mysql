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
#include <boost/mysql/rows.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/execution_state_impl.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/core/span.hpp>
#include <boost/test/unit_test_suite.hpp>

#include "creation/create_execution_state.hpp"
#include "creation/create_message_struct.hpp"
#include "printing.hpp"
#include "test_common.hpp"

using boost::mysql::column_type;
using boost::mysql::field_view;
using boost::mysql::metadata_collection_view;
using boost::mysql::metadata_mode;
using boost::mysql::row_view;
using boost::mysql::rows;
using boost::mysql::rows_view;
using boost::mysql::string_view;
using boost::mysql::detail::execution_state_impl;
using boost::mysql::detail::protocol_field_type;
using boost::mysql::detail::resultset_container;
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

// Metadata
std::vector<protocol_field_type> create_meta_r1()
{
    return {protocol_field_type::tiny, protocol_field_type::var_string};
}

std::vector<protocol_field_type> create_meta_r2() { return {protocol_field_type::bit}; }

void check_meta_r1(metadata_collection_view meta)
{
    BOOST_TEST_REQUIRE(meta.size() == 2u);
    BOOST_TEST(meta[0].type() == column_type::tinyint);
    BOOST_TEST(meta[1].type() == column_type::varchar);
}

void check_meta_r2(metadata_collection_view meta)
{
    BOOST_TEST_REQUIRE(meta.size() == 1u);
    BOOST_TEST(meta[0].type() == column_type::bit);
}

void check_meta_r3(metadata_collection_view meta)
{
    BOOST_TEST_REQUIRE(meta.size() == 3u);
    BOOST_TEST(meta[0].type() == column_type::float_);
    BOOST_TEST(meta[1].type() == column_type::double_);
    BOOST_TEST(meta[2].type() == column_type::tinyint);
}

void check_meta_empty(metadata_collection_view meta) { BOOST_TEST(meta.size() == 0u); }

// OK packet data checking
boost::mysql::detail::ok_packet create_ok_r1(bool more_results = false)
{
    return ok_builder()
        .affected_rows(1)
        .last_insert_id(2)
        .warnings(4)
        .info("Information")
        .more_results(more_results)
        .build();
}

boost::mysql::detail::ok_packet create_ok_r2(bool more_results = false)
{
    return ok_builder()
        .affected_rows(5)
        .last_insert_id(6)
        .warnings(8)
        .info("more_info")
        .more_results(more_results)
        .out_params(true)
        .build();
}

boost::mysql::detail::ok_packet create_ok_r3()
{
    return ok_builder().affected_rows(10).last_insert_id(11).warnings(12).info("").build();
}

void check_ok_r1(const execution_state_impl& st, std::size_t idx)
{
    BOOST_TEST(st.get_affected_rows(idx) == 1u);
    BOOST_TEST(st.get_last_insert_id(idx) == 2u);
    BOOST_TEST(st.get_warning_count(idx) == 4u);
    BOOST_TEST(st.get_info(idx) == "Information");
    BOOST_TEST(st.get_is_out_params(idx) == false);
}

void check_ok_r2(const execution_state_impl& st, std::size_t idx)
{
    BOOST_TEST(st.get_affected_rows(idx) == 5u);
    BOOST_TEST(st.get_last_insert_id(idx) == 6u);
    BOOST_TEST(st.get_warning_count(idx) == 8u);
    BOOST_TEST(st.get_info(idx) == "more_info");
    BOOST_TEST(st.get_is_out_params(idx) == true);
}

void check_ok_r3(const execution_state_impl& st, std::size_t idx)
{
    BOOST_TEST(st.get_affected_rows(idx) == 10u);
    BOOST_TEST(st.get_last_insert_id(idx) == 11u);
    BOOST_TEST(st.get_warning_count(idx) == 12u);
    BOOST_TEST(st.get_info(idx) == "");
    BOOST_TEST(st.get_is_out_params(idx) == false);
}

// Rows. Note that this doesn't handle copying strings into the internal rows -
// this is not the responsibility of this component
template <class... Args>
void add_row(execution_state_impl& st, const Args&... args)
{
    assert(sizeof...(Args) == st.current_resultset_meta().size());
    auto fields = make_fv_arr(args...);
    auto* storage = st.add_row();
    for (std::size_t i = 0; i < fields.size(); ++i)
    {
        storage[i] = fields[i];
    }
}

BOOST_AUTO_TEST_SUITE(test_execution_state_impl)

BOOST_AUTO_TEST_SUITE(test_resultset_container)
BOOST_AUTO_TEST_CASE(append_from_empty)
{
    // Initial
    resultset_container c;
    BOOST_TEST(c.empty());
    BOOST_TEST(c.size() == 0u);

    // Append first
    c.emplace_back().num_rows = 1;
    BOOST_TEST(!c.empty());
    BOOST_TEST(c.size() == 1u);
    BOOST_TEST(c[0].num_rows == 1u);
    BOOST_TEST(c.back().num_rows == 1u);

    // Append second
    c.emplace_back().num_rows = 2;
    BOOST_TEST(!c.empty());
    BOOST_TEST(c.size() == 2u);
    BOOST_TEST(c[0].num_rows == 1u);
    BOOST_TEST(c[1].num_rows == 2u);
    BOOST_TEST(c.back().num_rows == 2u);

    // Append third
    c.emplace_back().num_rows = 3;
    BOOST_TEST(!c.empty());
    BOOST_TEST(c.size() == 3u);
    BOOST_TEST(c[0].num_rows == 1u);
    BOOST_TEST(c[1].num_rows == 2u);
    BOOST_TEST(c[2].num_rows == 3u);
    BOOST_TEST(c.back().num_rows == 3u);
}

BOOST_AUTO_TEST_CASE(append_from_cleared)
{
    // Initial
    resultset_container c;
    c.emplace_back().num_rows = 42u;
    c.emplace_back().num_rows = 43u;

    // Clear
    c.clear();
    BOOST_TEST(c.empty());
    BOOST_TEST(c.size() == 0u);

    // Append first
    c.emplace_back().num_rows = 1;
    BOOST_TEST(!c.empty());
    BOOST_TEST(c.size() == 1u);
    BOOST_TEST(c[0].num_rows == 1u);
    BOOST_TEST(c.back().num_rows == 1u);

    // Append second
    c.emplace_back().num_rows = 2;
    BOOST_TEST(!c.empty());
    BOOST_TEST(c.size() == 2u);
    BOOST_TEST(c[0].num_rows == 1u);
    BOOST_TEST(c[1].num_rows == 2u);
    BOOST_TEST(c.back().num_rows == 2u);

    // Append third
    c.emplace_back().num_rows = 3;
    BOOST_TEST(!c.empty());
    BOOST_TEST(c.size() == 3u);
    BOOST_TEST(c[0].num_rows == 1u);
    BOOST_TEST(c[1].num_rows == 2u);
    BOOST_TEST(c[2].num_rows == 3u);
    BOOST_TEST(c.back().num_rows == 3u);
}

BOOST_AUTO_TEST_CASE(clear_empty)
{
    // Initial
    resultset_container c;
    c.clear();
    BOOST_TEST(c.empty());
    BOOST_TEST(c.size() == 0u);
}

BOOST_AUTO_TEST_CASE(several_clears)
{
    // Initial
    resultset_container c;
    c.emplace_back().num_rows = 42u;

    // Clear
    c.clear();
    BOOST_TEST(c.empty());
    BOOST_TEST(c.size() == 0u);

    // Append again
    c.emplace_back().num_rows = 1;
    c.emplace_back().num_rows = 2;

    // Clear again
    c.clear();
    BOOST_TEST(c.empty());
    BOOST_TEST(c.size() == 0u);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(append_false)

struct append_false_fixture
{
    std::vector<field_view> fields;
    execution_state_impl st{false};

    append_false_fixture() { st.reset(resultset_encoding::text, &fields); }
};

BOOST_FIXTURE_TEST_CASE(one_resultset_data, append_false_fixture)
{
    // Initial. Verify that we clear any previous result
    st = exec_builder(false)
             .reset(&fields)
             .meta({protocol_field_type::geometry})
             .rows(makerows(1, makebv("\0\0"), makebv("")))
             .build();
    st.reset(resultset_encoding::text, &fields);
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

    // Rows
    st.on_row_batch_start();
    add_row(st, 10, "abc");
    add_row(st, 20, "cdef");

    // End of resultset
    st.on_row_ok_packet(create_ok_r1());
    st.on_row_batch_finish();
    check_complete(st);
    check_meta_r1(st.get_meta(0));
    BOOST_TEST(st.get_external_rows() == makerows(2, 10, "abc", 20, "cdef"));
    check_ok_r1(st, 0);
}

BOOST_FIXTURE_TEST_CASE(one_resultset_empty, append_false_fixture)
{
    // Directly end of resultet, no meta
    st.on_head_ok_packet(create_ok_r1());
    check_complete(st);
    check_meta_empty(st.get_meta(0));
    check_ok_r1(st, 0);
}

BOOST_FIXTURE_TEST_CASE(two_resultsets_data_data, append_false_fixture)
{
    // Resultset r1
    st = exec_builder(false)
             .reset(&fields)
             .meta(create_meta_r1())
             .rows(makerows(2, 10, "abc", 20, "def"))
             .build();

    // OK packet indicates more results
    st.on_row_ok_packet(create_ok_r1(true));
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

    // Rows
    st.on_row_batch_start();
    add_row(st, 90u);

    // OK packet, no more resultsets
    st.on_row_ok_packet(create_ok_r2());
    st.on_row_batch_finish();
    check_complete(st);
    check_meta_r2(st.get_meta(0));
    BOOST_TEST(st.get_external_rows() == makerows(1, 90u));
    check_ok_r2(st, 0);
}

BOOST_FIXTURE_TEST_CASE(two_resultsets_empty_data, append_false_fixture)
{
    // Resultset r1
    st.on_head_ok_packet(create_ok_r1(true));
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
    st.on_row_batch_start();
    add_row(st, 90u);

    // Final OK packet
    st.on_row_ok_packet(create_ok_r2());
    st.on_row_batch_finish();
    check_complete(st);
    check_meta_r2(st.get_meta(0));
    BOOST_TEST(st.get_external_rows() == makerows(1, 90u));
    check_ok_r2(st, 0);
}

BOOST_FIXTURE_TEST_CASE(two_resultsets_data_empty, append_false_fixture)
{
    // Resultset r1
    st = exec_builder(false).reset(&fields).meta(create_meta_r1()).build();

    // OK packet indicates more results
    st.on_row_ok_packet(create_ok_r1(true));
    check_should_read_head(st);
    check_meta_r1(st.get_meta(0));
    check_ok_r1(st, 0);

    // OK packet for 2nd result
    st.on_head_ok_packet(create_ok_r2());
    check_complete(st);
    check_meta_empty(st.get_meta(0));
    check_ok_r2(st, 0);
}

BOOST_FIXTURE_TEST_CASE(two_resultsets_empty_empty, append_false_fixture)
{
    // OK packet indicates more results
    st.on_head_ok_packet(create_ok_r1(true));
    check_should_read_head(st);
    check_meta_empty(st.get_meta(0));
    check_ok_r1(st, 0);

    // OK packet for 2nd result
    st.on_head_ok_packet(create_ok_r2());
    check_complete(st);
    check_meta_empty(st.get_meta(0));
    check_ok_r2(st, 0);
}

BOOST_FIXTURE_TEST_CASE(three_resultsets_empty_empty_data, append_false_fixture)
{
    // Two first resultsets
    st.on_head_ok_packet(create_ok_r1(true));
    st.on_head_ok_packet(create_ok_r2(true));
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

    // Rows
    st.on_row_batch_start();
    add_row(st, 4.2f, 90.0, 9);

    // End of resultset
    st.on_row_ok_packet(create_ok_r3());
    st.on_row_batch_finish();
    check_complete(st);
    check_meta_r3(st.get_meta(0));
    BOOST_TEST(st.get_external_rows() == makerows(3, 4.2f, 90.0, 9));
    check_ok_r3(st, 0);
}

BOOST_FIXTURE_TEST_CASE(three_resultsets_data_empty_data, append_false_fixture)
{
    // Two first resultsets
    st = exec_builder(false)
             .reset(&fields)
             .meta(create_meta_r1())
             .rows(makerows(2, 40, "abc", 50, "def"))
             .ok(create_ok_r1(true))
             .build();
    st.on_head_ok_packet(create_ok_r2(true));
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

    // Rows
    st.on_row_batch_start();
    add_row(st, 4.2f, 90.0, 9);

    // End of resultset
    st.on_row_ok_packet(create_ok_r3());
    st.on_row_batch_finish();
    check_complete(st);
    check_meta_r3(st.get_meta(0));
    BOOST_TEST(st.get_external_rows() == makerows(3, 4.2f, 90.0, 9));
    check_ok_r3(st, 0);
}

BOOST_FIXTURE_TEST_CASE(info_string_ownserhip, append_false_fixture)
{
    // OK packet received, doesn't own the string
    std::string info = "Some info";
    st.on_head_ok_packet(ok_builder().more_results(true).info(info).build());

    // st does, so changing info doesn't affect
    info = "other info";
    BOOST_TEST(st.get_info(0) == "Some info");

    // Repeat the process for row OK packet
    st.on_num_meta(1);
    st.on_meta(create_coldef(protocol_field_type::bit), metadata_mode::full);
    st.on_row_ok_packet(ok_builder().info(info).build());
    info = "abcdfefgh";
    BOOST_TEST(st.get_info(0) == "other info");
}

BOOST_FIXTURE_TEST_CASE(multiple_row_batches, append_false_fixture)
{
    // Head and meta
    st = exec_builder(false)
             .reset(&fields)
             .meta({protocol_field_type::tiny, protocol_field_type::var_string})
             .build();

    // Row batch 1
    st.on_row_batch_start();
    add_row(st, 10, "abc");
    add_row(st, 20, "cdef");
    st.on_row_batch_finish();
    check_should_read_rows(st);
    BOOST_TEST(st.get_external_rows() == makerows(2, 10, "abc", 20, "cdef"));

    // Row batch 2
    st.on_row_batch_start();
    add_row(st, 40, nullptr);
    st.on_row_ok_packet(create_ok_r1());
    st.on_row_batch_finish();
    check_complete(st);
    BOOST_TEST(st.get_external_rows() == makerows(2, 40, nullptr));
    BOOST_TEST(fields.size() == 2u);  // space was re-used
}

BOOST_FIXTURE_TEST_CASE(empty_row_batch, append_false_fixture)
{
    // Head and meta
    st = exec_builder(false)
             .reset(&fields)
             .meta({protocol_field_type::tiny, protocol_field_type::var_string})
             .build();

    // Row batch 1
    st.on_row_batch_start();
    add_row(st, 10, "abc");
    add_row(st, 20, "cdef");
    st.on_row_batch_finish();
    BOOST_TEST(st.get_external_rows() == makerows(2, 10, "abc", 20, "cdef"));

    // Row batch 2
    st.on_row_batch_start();
    add_row(st, 40, nullptr);
    st.on_row_batch_finish();
    BOOST_TEST(st.get_external_rows() == makerows(2, 40, nullptr));
    BOOST_TEST(fields.size() == 2u);  // space was re-used

    // End of resultset
    st.on_row_batch_start();
    st.on_row_ok_packet(create_ok_r1());
    st.on_row_batch_finish();
    BOOST_TEST(st.get_external_rows() == makerows(2));
    check_complete(st);
}
BOOST_AUTO_TEST_SUITE_END()  // append_false

//
// append true
//
BOOST_AUTO_TEST_SUITE(append_true)

BOOST_AUTO_TEST_CASE(one_resultset_data)
{
    // Initial. Check that we reset any previous state
    auto st = exec_builder(true)
                  .reset(resultset_encoding::binary)
                  .meta({protocol_field_type::geometry})
                  .rows(makerows(1, makebv("\0\0"), makebv("abc")))
                  .ok(ok_builder().affected_rows(40).info("some_info").more_results(true).build())
                  .meta({protocol_field_type::var_string})
                  .rows(makerows(1, "aaaa", "bbbb"))
                  .ok(ok_builder().info("more_info").more_results(true).build())
                  .build();
    st.reset(resultset_encoding::binary, nullptr);
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

    // Rows
    st.on_row_batch_start();
    add_row(st, 42, "abc");

    // End of resultset
    st.on_row_ok_packet(create_ok_r1());
    st.on_row_batch_finish();  // EOF is part of the batch
    check_complete(st);
    check_meta_r1(st.get_meta(0));
    check_ok_r1(st, 0);
    BOOST_TEST(st.num_resultsets() == 1u);
    BOOST_TEST(st.get_rows(0) == makerows(2, 42, "abc"));
    BOOST_TEST(st.get_out_params() == row_view());
}

BOOST_AUTO_TEST_CASE(one_resultset_empty)
{
    // Initial
    execution_state_impl st(true);
    st.reset(resultset_encoding::text, nullptr);
    check_should_read_head(st);

    // End of resultset
    st.on_head_ok_packet(create_ok_r1());
    check_complete(st);
    check_meta_empty(st.get_meta(0));
    check_ok_r1(st, 0);
    BOOST_TEST(st.num_resultsets() == 1u);
    BOOST_TEST(st.get_rows(0) == rows_view());
    BOOST_TEST(st.get_out_params() == row_view());
}

BOOST_AUTO_TEST_CASE(two_resultsets_data_data)
{
    // Resultset r1
    auto st = exec_builder(true).meta(create_meta_r1()).rows(makerows(2, 42, "abc", 50, "def")).build();

    // OK packet indicates more results
    st.on_row_ok_packet(create_ok_r1(true));
    check_should_read_head(st);

    // Resultset r2: indicates resultset with meta
    st.on_num_meta(1);
    check_should_read_meta(st);

    // Meta
    st.on_meta(create_coldef(protocol_field_type::bit), metadata_mode::minimal);
    check_should_read_rows(st);
    check_meta_r2(st.current_resultset_meta());

    // Row
    st.on_row_batch_start();
    add_row(st, 70);
    check_should_read_rows(st);

    // OK packet, no more resultsets
    st.on_row_ok_packet(create_ok_r2());
    st.on_row_batch_finish();
    check_complete(st);
    check_meta_r1(st.get_meta(0));
    check_meta_r2(st.get_meta(1));
    check_ok_r1(st, 0);
    check_ok_r2(st, 1);
    BOOST_TEST(st.num_resultsets() == 2u);
    BOOST_TEST(st.get_rows(0) == makerows(2, 42, "abc", 50, "def"));
    BOOST_TEST(st.get_rows(1) == makerows(1, 70));
    BOOST_TEST(st.get_out_params() == makerow(70));
}

BOOST_AUTO_TEST_CASE(two_resultsets_empty_data)
{
    // Resultset r1: same as the single case
    execution_state_impl st(true);
    st.on_head_ok_packet(create_ok_r1(true));
    check_should_read_head(st);

    // Resultset r2: indicates data
    st.on_num_meta(1);
    check_should_read_meta(st);

    // Metadata packet
    st.on_meta(create_coldef(protocol_field_type::bit), metadata_mode::minimal);
    check_should_read_rows(st);
    check_meta_r2(st.current_resultset_meta());

    // Rows
    st.on_row_batch_start();
    add_row(st, 70);
    check_should_read_rows(st);

    // Final OK packet
    st.on_row_ok_packet(create_ok_r2());
    st.on_row_batch_finish();
    check_complete(st);
    check_meta_empty(st.get_meta(0));
    check_meta_r2(st.get_meta(1));
    check_ok_r1(st, 0);
    check_ok_r2(st, 1);
    BOOST_TEST(st.num_resultsets() == 2u);
    BOOST_TEST(st.get_rows(0) == rows_view());
    BOOST_TEST(st.get_rows(1) == makerows(1, 70));
    BOOST_TEST(st.get_out_params() == makerow(70));
}

// Note: this tests also an edge case where a resultset indicates
// that contains OUT parameters but is empty
BOOST_AUTO_TEST_CASE(two_resultsets_data_empty)
{
    // Resultset r1: equivalent to single resultset case
    auto st = exec_builder(true).meta(create_meta_r1()).rows(makerows(2, 42, "abc", 50, "def")).build();

    // OK packet indicates more results
    st.on_row_ok_packet(create_ok_r1(true));
    check_should_read_head(st);

    // OK packet for 2nd result
    st.on_head_ok_packet(create_ok_r2());
    check_complete(st);
    check_meta_r1(st.get_meta(0));
    check_meta_empty(st.get_meta(1));
    check_ok_r1(st, 0);
    check_ok_r2(st, 1);
    BOOST_TEST(st.num_resultsets() == 2u);
    BOOST_TEST(st.get_rows(0) == makerows(2, 42, "abc", 50, "def"));
    BOOST_TEST(st.get_rows(1) == rows_view());
    BOOST_TEST(st.get_out_params() == row_view());
}

BOOST_AUTO_TEST_CASE(two_resultsets_empty_empty)
{
    // Resultset r1: equivalent to single resultset case
    execution_state_impl st(true);
    st.on_head_ok_packet(create_ok_r1(true));
    check_should_read_head(st);

    // OK packet for 2nd result
    st.on_head_ok_packet(create_ok_r2());
    check_complete(st);
    check_meta_empty(st.get_meta(0));
    check_meta_empty(st.get_meta(1));
    check_ok_r1(st, 0);
    check_ok_r2(st, 1);
    BOOST_TEST(st.num_resultsets() == 2u);
    BOOST_TEST(st.get_rows(0) == rows_view());
    BOOST_TEST(st.get_rows(1) == rows_view());
    BOOST_TEST(st.get_out_params() == row_view());
}

BOOST_AUTO_TEST_CASE(three_resultsets_empty_empty_data)
{
    // Two first resultsets
    auto st = exec_builder(true).ok(create_ok_r1(true)).build();
    st.on_head_ok_packet(create_ok_r2(true));
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
    st.on_row_batch_start();
    add_row(st, 4.2f, 5.0, 8);
    add_row(st, 42.0f, 50.0, 80);

    // End of resultset
    st.on_row_ok_packet(create_ok_r3());
    st.on_row_batch_finish();
    check_complete(st);
    check_meta_empty(st.get_meta(0));
    check_meta_empty(st.get_meta(1));
    check_meta_r3(st.get_meta(2));
    check_ok_r1(st, 0);
    check_ok_r2(st, 1);
    check_ok_r3(st, 2);
    BOOST_TEST(st.num_resultsets() == 3u);
    BOOST_TEST(st.get_rows(0) == rows_view());
    BOOST_TEST(st.get_rows(1) == rows_view());
    BOOST_TEST(st.get_rows(2) == makerows(3, 4.2f, 5.0, 8, 42.0f, 50.0, 80));
    BOOST_TEST(st.get_out_params() == row_view());
}

// Verify that we do row slicing correctly
BOOST_AUTO_TEST_CASE(three_resultsets_data_data_data)
{
    // Two first resultets
    auto st = exec_builder(true)
                  .meta(create_meta_r1())
                  .rows(makerows(2, 42, "abc", 50, "def"))
                  .ok(create_ok_r1(true))
                  .meta(create_meta_r2())
                  .rows(makerows(1, 60))
                  .build();

    // OK packet indicates more results
    st.on_row_ok_packet(create_ok_r2(true));

    // Third resultset
    st.on_num_meta(3);
    st.on_meta(create_coldef(protocol_field_type::float_), metadata_mode::minimal);
    st.on_meta(create_coldef(protocol_field_type::double_), metadata_mode::minimal);
    st.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::minimal);
    st.on_row_batch_start();
    add_row(st, 4.2f, 5.0, 8);
    add_row(st, 42.0f, 50.0, 80);
    st.on_row_ok_packet(create_ok_r3());
    st.on_row_batch_finish();

    // Check results
    check_complete(st);
    check_meta_r1(st.get_meta(0));
    check_meta_r2(st.get_meta(1));
    check_meta_r3(st.get_meta(2));
    check_ok_r1(st, 0);
    check_ok_r2(st, 1);
    check_ok_r3(st, 2);
    BOOST_TEST(st.num_resultsets() == 3u);
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
    st.on_head_ok_packet(ok_builder().more_results(true).info(info).build());

    // Empty OK packet
    info = "";
    st.on_head_ok_packet(ok_builder().more_results(true).info(info).build());

    // Row OK packet
    info = "other info";
    st.on_num_meta(1);
    st.on_meta(create_coldef(protocol_field_type::bit), metadata_mode::full);
    st.on_row_ok_packet(ok_builder().info(info).build());
    info = "abcdfefgh";
    BOOST_TEST(st.get_info(0) == "Some info");
    BOOST_TEST(st.get_info(1) == "");
    BOOST_TEST(st.get_info(2) == "other info");
}

BOOST_AUTO_TEST_CASE(multiple_row_batches)
{
    // Initial
    auto st = exec_builder(true).meta({protocol_field_type::tiny, protocol_field_type::var_string}).build();

    // First batch
    st.on_row_batch_start();
    add_row(st, 42, "abc");
    add_row(st, 50, "bdef");
    st.on_row_batch_finish();

    // Second batch (only one row)
    st.on_row_batch_start();
    add_row(st, 60, "pov");

    // End of resultset
    st.on_row_ok_packet(create_ok_r1());
    st.on_row_batch_finish();
    check_complete(st);
    BOOST_TEST(st.num_resultsets() == 1u);
    BOOST_TEST(st.get_rows(0) == makerows(2, 42, "abc", 50, "bdef", 60, "pov"));
}

BOOST_AUTO_TEST_CASE(empty_row_batch)
{
    // Initial
    auto st = exec_builder(true).meta({protocol_field_type::tiny, protocol_field_type::var_string}).build();

    // No rows, directly eof
    st.on_row_batch_start();
    st.on_row_ok_packet(create_ok_r1());
    st.on_row_batch_finish();
    check_complete(st);
    BOOST_TEST(st.num_resultsets() == 1u);
    BOOST_TEST(st.get_rows(0) == makerows(2));  // empty but with 2 cols
}
BOOST_AUTO_TEST_SUITE_END()  // append true

BOOST_AUTO_TEST_CASE(reset)
{
    auto st = exec_builder(true)
                  .reset(resultset_encoding::binary)
                  .seqnum(90u)
                  .meta({protocol_field_type::bit})
                  .build();
    st.reset(resultset_encoding::text, nullptr);
    BOOST_TEST(st.encoding() == resultset_encoding::text);
    BOOST_TEST(st.sequence_number() == 0u);
    BOOST_TEST(st.is_append_mode() == true);  // doesn't get reset
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
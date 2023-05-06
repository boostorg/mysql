//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/column_type.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/execution_processor/execution_state_impl.hpp>
#include <boost/mysql/detail/protocol/capabilities.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/deserialization_context.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/core/span.hpp>
#include <boost/test/unit_test.hpp>

#include "check_meta.hpp"
#include "creation/create_execution_state.hpp"
#include "creation/create_message_struct.hpp"
#include "creation/create_row_message.hpp"
#include "printing.hpp"
#include "test_common.hpp"

using namespace boost::mysql;
using boost::mysql::detail::execution_state_impl;
using boost::mysql::detail::output_ref;
using boost::mysql::detail::protocol_field_type;
using boost::mysql::detail::resultset_encoding;

using namespace boost::mysql::test;

namespace {

// Metadata
std::vector<protocol_field_type> create_meta_r1()
{
    return {protocol_field_type::tiny, protocol_field_type::var_string};
}

void check_meta_r1(metadata_collection_view meta)
{
    check_meta(meta, {column_type::tinyint, column_type::varchar});
}

void check_meta_r2(metadata_collection_view meta) { check_meta(meta, {column_type::bigint}); }

void check_meta_r3(metadata_collection_view meta)
{
    check_meta(meta, {column_type::float_, column_type::double_, column_type::tinyint});
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

void check_ok_r1(const execution_state_impl& st)
{
    BOOST_TEST(st.get_affected_rows() == 1u);
    BOOST_TEST(st.get_last_insert_id() == 2u);
    BOOST_TEST(st.get_warning_count() == 4u);
    BOOST_TEST(st.get_info() == "Information");
    BOOST_TEST(st.get_is_out_params() == false);
}

void check_ok_r2(const execution_state_impl& st)
{
    BOOST_TEST(st.get_affected_rows() == 5u);
    BOOST_TEST(st.get_last_insert_id() == 6u);
    BOOST_TEST(st.get_warning_count() == 8u);
    BOOST_TEST(st.get_info() == "more_info");
    BOOST_TEST(st.get_is_out_params() == true);
}

void check_ok_r3(const execution_state_impl& st)
{
    BOOST_TEST(st.get_affected_rows() == 10u);
    BOOST_TEST(st.get_last_insert_id() == 11u);
    BOOST_TEST(st.get_warning_count() == 12u);
    BOOST_TEST(st.get_info() == "");
    BOOST_TEST(st.get_is_out_params() == false);
}

BOOST_AUTO_TEST_SUITE(test_execution_state_impl)

struct fixture
{
    std::vector<field_view> fields;
    execution_state_impl st;
    boost::mysql::diagnostics diag;
};

BOOST_FIXTURE_TEST_CASE(one_resultset_data, fixture)
{
    // Initial. Verify that we clear any previous result
    st = exec_builder()
             .reset(resultset_encoding::binary)
             .meta({protocol_field_type::geometry})
             .ok(ok_builder()
                     .affected_rows(1)
                     .last_insert_id(2)
                     .warnings(3)
                     .more_results(true)
                     .info("abc")
                     .build())
             .build();

    // Reset
    st.reset(resultset_encoding::text, metadata_mode::full);
    BOOST_TEST(st.is_reading_first());

    // Head indicates resultset with metadata
    st.on_num_meta(2);
    BOOST_TEST(st.is_reading_meta());

    // First metadata
    auto err = st.on_meta(create_coldef(protocol_field_type::tiny), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_meta());

    // Second metadata, ready to read rows
    err = st.on_meta(create_coldef(protocol_field_type::var_string), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_rows());
    check_meta_r1(st.meta());

    // Rows
    rowbuff r1{10, "abc"}, r2{20, "cdef"};
    err = st.on_row(r1.ctx(), output_ref(fields));
    throw_on_error(err, diag);
    err = st.on_row(r2.ctx(), output_ref(fields));
    throw_on_error(err, diag);
    BOOST_TEST(fields == make_fv_vector(10, "abc", 20, "cdef"));

    // End of resultset
    err = st.on_row_ok_packet(create_ok_r1());
    throw_on_error(err, diag);
    BOOST_TEST(st.is_complete());
    check_meta_r1(st.meta());
    check_ok_r1(st);
}

BOOST_FIXTURE_TEST_CASE(one_resultset_empty, fixture)
{
    // Directly end of resultet, no meta
    auto err = st.on_head_ok_packet(create_ok_r1(), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_complete());
    check_meta_empty(st.meta());
    check_ok_r1(st);
}

BOOST_FIXTURE_TEST_CASE(two_resultsets_data_data, fixture)
{
    // Resultset r1 (rows are not stored anyhow in execution states)
    st = exec_builder().meta(create_meta_r1()).build();

    // OK packet indicates more results
    auto err = st.on_row_ok_packet(create_ok_r1(true));
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_first_subseq());
    check_meta_r1(st.meta());
    check_ok_r1(st);

    // Resultset r2: indicates resultset with meta
    st.on_num_meta(1);
    BOOST_TEST(st.is_reading_meta());

    // First packet
    err = st.on_meta(create_coldef(protocol_field_type::longlong), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_rows());
    check_meta_r2(st.meta());

    // Rows
    rowbuff r1{90u};
    err = st.on_row(r1.ctx(), output_ref(fields));
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_rows());
    BOOST_TEST(fields == make_fv_vector(90u));

    // OK packet, no more resultsets
    err = st.on_row_ok_packet(create_ok_r2());
    throw_on_error(err, diag);
    BOOST_TEST(st.is_complete());
    check_meta_r2(st.meta());
    check_ok_r2(st);
}

BOOST_FIXTURE_TEST_CASE(two_resultsets_empty_data, fixture)
{
    // Resultset r1
    st = exec_builder().ok(create_ok_r1(true)).build();
    BOOST_TEST(st.is_reading_first_subseq());
    check_meta_empty(st.meta());
    check_ok_r1(st);

    // Resultset r2: indicates data
    st.on_num_meta(1);
    BOOST_TEST(st.is_reading_meta());

    // Metadata packet
    auto err = st.on_meta(create_coldef(protocol_field_type::longlong), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_rows());
    check_meta_r2(st.meta());

    // Rows
    rowbuff r1{90u}, r2{100u};
    err = st.on_row(r1.ctx(), output_ref(fields));
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_rows());
    err = st.on_row(r2.ctx(), output_ref(fields));
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_rows());

    // Final OK packet
    err = st.on_row_ok_packet(create_ok_r2());
    throw_on_error(err, diag);
    BOOST_TEST(st.is_complete());
    check_meta_r2(st.meta());
    check_ok_r2(st);
}

BOOST_FIXTURE_TEST_CASE(two_resultsets_data_empty, fixture)
{
    // Resultset r1
    st = exec_builder().meta(create_meta_r1()).build();

    // OK packet indicates more results
    auto err = st.on_row_ok_packet(create_ok_r1(true));
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_first_subseq());
    check_meta_r1(st.meta());
    check_ok_r1(st);

    // OK packet for 2nd result
    err = st.on_head_ok_packet(create_ok_r2(), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_complete());
    check_meta_empty(st.meta());
    check_ok_r2(st);
}

BOOST_FIXTURE_TEST_CASE(two_resultsets_empty_empty, fixture)
{
    // OK packet indicates more results
    auto err = st.on_head_ok_packet(create_ok_r1(true), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_first_subseq());
    check_meta_empty(st.meta());
    check_ok_r1(st);

    // OK packet for 2nd result
    err = st.on_head_ok_packet(create_ok_r2(), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_complete());
    check_meta_empty(st.meta());
    check_ok_r2(st);
}

BOOST_FIXTURE_TEST_CASE(three_resultsets_empty_empty_data, fixture)
{
    // Two first resultsets
    st = exec_builder().ok(create_ok_r1(true)).build();
    auto err = st.on_head_ok_packet(create_ok_r2(true), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_first_subseq());
    check_meta_empty(st.meta());
    check_ok_r2(st);

    // Resultset r3: head indicates resultset with metadata
    st.on_num_meta(3);
    BOOST_TEST(st.is_reading_meta());

    // Metadata
    err = st.on_meta(create_coldef(protocol_field_type::float_), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_meta());

    err = st.on_meta(create_coldef(protocol_field_type::double_), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_meta());

    err = st.on_meta(create_coldef(protocol_field_type::tiny), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_rows());
    check_meta_r3(st.meta());

    // Rows
    rowbuff r1{4.2f, 90.0, 9};
    err = st.on_row(r1.ctx(), output_ref(fields));
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_rows());
    BOOST_TEST(fields == make_fv_vector(4.2f, 90.0, 9));

    // End of resultset
    err = st.on_row_ok_packet(create_ok_r3());
    throw_on_error(err, diag);
    BOOST_TEST(st.is_complete());
    check_meta_r3(st.meta());
    check_ok_r3(st);
}

BOOST_FIXTURE_TEST_CASE(three_resultsets_data_empty_data, fixture)
{
    // Two first resultsets
    st = exec_builder().meta(create_meta_r1()).ok(create_ok_r1(true)).build();
    auto err = st.on_head_ok_packet(create_ok_r2(true), diag);
    BOOST_TEST(st.is_reading_first_subseq());
    check_meta_empty(st.meta());
    check_ok_r2(st);

    // Resultset r3: head indicates resultset with metadata
    st.on_num_meta(3);
    BOOST_TEST(st.is_reading_meta());

    // Metadata
    err = st.on_meta(create_coldef(protocol_field_type::float_), diag);
    throw_on_error(err, diag);
    err = st.on_meta(create_coldef(protocol_field_type::double_), diag);
    throw_on_error(err, diag);
    err = st.on_meta(create_coldef(protocol_field_type::tiny), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_rows());
    check_meta_r3(st.meta());

    // Rows
    rowbuff r1{4.2f, 90.0, 9};
    err = st.on_row(r1.ctx(), output_ref(fields));
    throw_on_error(err, diag);
    BOOST_TEST(fields == make_fv_vector(4.2f, 90.0, 9));

    // End of resultset
    err = st.on_row_ok_packet(create_ok_r3());
    throw_on_error(err, diag);
    BOOST_TEST(st.is_complete());
    check_meta_r3(st.meta());
    check_ok_r3(st);
}

BOOST_FIXTURE_TEST_CASE(info_string_ownserhip, fixture)
{
    // OK packet received, doesn't own the string
    std::string info = "Some info";
    auto err = st.on_head_ok_packet(ok_builder().more_results(true).info(info).build(), diag);
    throw_on_error(err, diag);

    // st does, so changing info doesn't affect
    info = "other info";
    BOOST_TEST(st.get_info() == "Some info");

    // Repeat the process for row OK packet
    st.on_num_meta(1);
    err = st.on_meta(create_coldef(protocol_field_type::longlong), diag);
    throw_on_error(err, diag);
    err = st.on_row_ok_packet(ok_builder().info(info).build());
    throw_on_error(err, diag);
    info = "abcdfefgh";
    BOOST_TEST(st.get_info() == "other info");
}

BOOST_FIXTURE_TEST_CASE(error_deserializing_row, fixture)
{
    st = exec_builder().meta(create_meta_r1()).build();
    auto bad_row = create_text_row_body(42, "abc");
    bad_row.push_back(0xff);

    auto err = st.on_row(
        detail::deserialization_context(
            bad_row.data(),
            bad_row.data() + bad_row.size(),
            detail::capabilities()
        ),
        output_ref(fields)
    );

    BOOST_TEST(err == client_errc::extra_bytes);
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
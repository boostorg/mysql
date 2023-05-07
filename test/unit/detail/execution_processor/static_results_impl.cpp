//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/column_type.hpp>

#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/execution_processor/static_results_impl.hpp>

#include <boost/describe/class.hpp>
#include <boost/describe/operators.hpp>
#include <boost/test/unit_test.hpp>

#include "creation/create_execution_state.hpp"
#include "creation/create_message_struct.hpp"
#include "creation/create_meta.hpp"
#include "printing.hpp"
#include "test_common.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using boost::mysql::detail::output_ref;
using boost::mysql::detail::static_results_erased_impl;
using boost::mysql::detail::static_results_impl;

namespace {

// Row types
// Row types
struct row1
{
    std::string fvarchar;
    std::int16_t ftiny;
};
BOOST_DESCRIBE_STRUCT(row1, (), (fvarchar, ftiny));

struct row2
{
    std::int64_t fbigint;
};
BOOST_DESCRIBE_STRUCT(row2, (), (fbigint));

struct row3
{
    double fdouble;
    std::int8_t ftiny;
    float ffloat;
};
BOOST_DESCRIBE_STRUCT(row3, (), (fdouble, ftiny, ffloat));

struct empty
{
};
BOOST_DESCRIBE_STRUCT(empty, (), ());

using boost::describe::operators::operator==;
using boost::describe::operators::operator<<;

// Metadata
std::vector<metadata> create_meta_r1()
{
    return {
        meta_builder().type(column_type::tinyint).name("ftiny").nullable(false).build(),
        meta_builder().type(column_type::varchar).name("fvarchar").nullable(false).build(),
    };
}
// std::vector<metadata> create_meta_r3()
// {
//     return {
//         meta_builder().type(column_type::float_).name("ffloat").nullable(false).build(),
//         meta_builder().type(column_type::double_).name("fdouble").nullable(false).build(),
//         meta_builder().type(column_type::tinyint).name("ftiny").nullable(false).build(),
//     };
// }

void check_meta_r1(metadata_collection_view meta)
{
    BOOST_TEST_REQUIRE(meta.size() == 2u);
    BOOST_TEST(meta[0].type() == column_type::tinyint);
    BOOST_TEST(meta[1].type() == column_type::varchar);
}

void check_meta_r2(metadata_collection_view meta)
{
    BOOST_TEST_REQUIRE(meta.size() == 1u);
    BOOST_TEST(meta[0].type() == column_type::bigint);
}

// void check_meta_r3(metadata_collection_view meta)
// {
//     BOOST_TEST_REQUIRE(meta.size() == 3u);
//     BOOST_TEST(meta[0].type() == column_type::float_);
//     BOOST_TEST(meta[1].type() == column_type::double_);
//     BOOST_TEST(meta[2].type() == column_type::tinyint);
// }

void check_meta_empty(metadata_collection_view meta) { BOOST_TEST(meta.size() == 0u); }

// Row checking
template <class T>
void check_rows(boost::span<const T> actual, const std::vector<T>& expected)
{
    std::vector<T> actualv(actual.begin(), actual.end());
    BOOST_TEST(actualv == expected);
}

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

// boost::mysql::detail::ok_packet create_ok_r3()
// {
//     return ok_builder().affected_rows(10).last_insert_id(11).warnings(12).info("").build();
// }

void check_ok_r1(const static_results_erased_impl& r, std::size_t idx)
{
    BOOST_TEST(r.get_affected_rows(idx) == 1u);
    BOOST_TEST(r.get_last_insert_id(idx) == 2u);
    BOOST_TEST(r.get_warning_count(idx) == 4u);
    BOOST_TEST(r.get_info(idx) == "Information");
    BOOST_TEST(r.get_is_out_params(idx) == false);
}

void check_ok_r2(const static_results_erased_impl& r, std::size_t idx)
{
    BOOST_TEST(r.get_affected_rows(idx) == 5u);
    BOOST_TEST(r.get_last_insert_id(idx) == 6u);
    BOOST_TEST(r.get_warning_count(idx) == 8u);
    BOOST_TEST(r.get_info(idx) == "more_info");
    BOOST_TEST(r.get_is_out_params(idx) == true);
}

// void check_ok_r3(const static_results_erased_impl& r, std::size_t idx)
// {
//     BOOST_TEST(r.get_affected_rows(idx) == 10u);
//     BOOST_TEST(r.get_last_insert_id(idx) == 11u);
//     BOOST_TEST(r.get_warning_count(idx) == 12u);
//     BOOST_TEST(r.get_info(idx) == "");
//     BOOST_TEST(r.get_is_out_params(idx) == false);
// }

BOOST_AUTO_TEST_SUITE(test_static_results_impl)

struct fixture
{
    diagnostics diag;
};

BOOST_FIXTURE_TEST_CASE(one_resultset_data, fixture)
{
    static_results_impl<row1> rt;
    auto& r = rt.get_interface();

    // Initial
    BOOST_TEST(r.is_reading_first());

    // Head indicates resultset with two columns
    r.on_num_meta(2);
    BOOST_TEST(r.is_reading_meta());

    // First meta
    auto err = r.on_meta(
        meta_builder().type(column_type::tinyint).name("ftiny").nullable(false).build(),
        diag
    );
    throw_on_error(err, diag);
    BOOST_TEST(r.is_reading_meta());

    // Second meta, ready to read rows
    err = r.on_meta(meta_builder().type(column_type::varchar).name("fvarchar").nullable(false).build(), diag);
    throw_on_error(err, diag);
    BOOST_TEST(r.is_reading_rows());

    // Rows
    rowbuff r1{42, "abc"};
    err = r.on_row(r1.ctx(), output_ref());
    throw_on_error(err, diag);
    BOOST_TEST(r.is_reading_rows());

    // End of resultset
    err = r.on_row_ok_packet(create_ok_r1());
    throw_on_error(err, diag);

    // Verify results
    std::vector<row1> expected_r1{
        {"abc", 42}
    };
    BOOST_TEST(r.is_complete());
    check_meta_r1(r.get_meta(0));
    check_ok_r1(r, 0);
    check_rows(rt.get_rows<0>(), expected_r1);
}

BOOST_FIXTURE_TEST_CASE(one_resultset_empty, fixture)
{
    static_results_impl<empty> rt;
    auto& r = rt.get_interface();

    // Initial
    BOOST_TEST(r.is_reading_first());

    // End of resultset
    auto err = r.on_head_ok_packet(create_ok_r1(), diag);
    throw_on_error(err, diag);

    // verify
    BOOST_TEST(r.is_complete());
    check_meta_empty(r.get_meta(0));
    check_ok_r1(r, 0);
    BOOST_TEST(rt.get_rows<0>().size() == 0u);
}

BOOST_FIXTURE_TEST_CASE(two_resultsets_data_data, fixture)
{
    // Resultset r1
    auto rt = static_results_builder<row1, row2>()
                  .meta(create_meta_r1())
                  .row(42, "abc")
                  .row(50, "def")
                  .build();
    auto& r = rt.get_interface();

    // OK packet indicates more results
    auto err = r.on_row_ok_packet(create_ok_r1(true));
    throw_on_error(err, diag);
    BOOST_TEST(r.is_reading_first_subseq());

    // Resultset r2: indicates resultset with meta
    r.on_num_meta(1);
    BOOST_TEST(r.is_reading_meta());

    // Meta
    err = r.on_meta(meta_builder().type(column_type::bigint).name("fbigint").nullable(false).build(), diag);
    BOOST_TEST(r.is_reading_rows());

    // Row
    rowbuff r1{70};
    err = r.on_row(r1.ctx(), output_ref());
    throw_on_error(err, diag);

    // OK packet, no more resultsets
    err = r.on_row_ok_packet(create_ok_r2());
    throw_on_error(err, diag);

    // Verify
    std::vector<row1> expected_r1{
        {"abc", 42},
        {"def", 50}
    };
    std::vector<row2> expected_r2{{70}};
    BOOST_TEST(r.is_complete());
    check_meta_r1(r.get_meta(0));
    check_meta_r2(r.get_meta(1));
    check_ok_r1(r, 0);
    check_ok_r2(r, 1);
    check_rows(rt.get_rows<0>(), expected_r1);
    check_rows(rt.get_rows<1>(), expected_r2);
}

// BOOST_FIXTURE_TEST_CASE(two_resultsets_empty_data, fixture)
// {
//     results_impl r;

//     // Empty resultset r1, indicating more results
//     auto err = r.on_head_ok_packet(create_ok_r1(true), diag);
//     throw_on_error(err, diag);
//     BOOST_TEST(r.is_reading_first_subseq());

//     // Resultset r2: indicates data
//     r.on_num_meta(1);
//     BOOST_TEST(r.is_reading_meta());

//     // Metadata packet
//     err = r.on_meta(create_coldef(protocol_field_type::longlong), diag);
//     throw_on_error(err, diag);
//     BOOST_TEST(r.is_reading_rows());

//     // Rows
//     rowbuff r1{70};
//     r.on_row_batch_start();
//     err = r.on_row(r1.ctx(), output_ref());
//     throw_on_error(err, diag);
//     BOOST_TEST(r.is_reading_rows());

//     // Final OK packet
//     err = r.on_row_ok_packet(create_ok_r2());
//     throw_on_error(err, diag);
//     r.on_row_batch_finish();

//     // Verify
//     BOOST_TEST(r.is_complete());
//     check_meta_empty(r.get_meta(0));
//     check_meta_r2(r.get_meta(1));
//     check_ok_r1(r, 0);
//     check_ok_r2(r, 1);
//     BOOST_TEST(r.num_resultsets() == 2u);
//     BOOST_TEST(r.get_rows(0) == rows_view());
//     BOOST_TEST(r.get_rows(1) == makerows(1, 70));
//     BOOST_TEST(r.get_out_params() == makerow(70));
// }

// // Note: this tests also an edge case where a resultset indicates
// // that contains OUT parameters but is empty
// BOOST_FIXTURE_TEST_CASE(two_resultsets_data_empty, fixture)
// {
//     // Resultset r1
//     auto r = results_builder().meta(create_meta_r1()).row(42, "abc").row(50, "def").build();

//     // OK packet indicates more results
//     auto err = r.on_row_ok_packet(create_ok_r1(true));
//     throw_on_error(err, diag);
//     BOOST_TEST(r.is_reading_first_subseq());

//     // OK packet for 2nd result
//     err = r.on_head_ok_packet(create_ok_r2(), diag);
//     throw_on_error(err, diag);

//     // Verify
//     BOOST_TEST(r.is_complete());
//     check_meta_r1(r.get_meta(0));
//     check_meta_empty(r.get_meta(1));
//     check_ok_r1(r, 0);
//     check_ok_r2(r, 1);
//     BOOST_TEST(r.num_resultsets() == 2u);
//     BOOST_TEST(r.get_rows(0) == makerows(2, 42, "abc", 50, "def"));
//     BOOST_TEST(r.get_rows(1) == rows_view());
//     BOOST_TEST(r.get_out_params() == row_view());
// }

// BOOST_FIXTURE_TEST_CASE(two_resultsets_empty_empty, fixture)
// {
//     results_impl r;

//     // Resultset r1
//     auto err = r.on_head_ok_packet(create_ok_r1(true), diag);
//     throw_on_error(err, diag);
//     BOOST_TEST(r.is_reading_first_subseq());

//     // OK packet for 2nd result
//     err = r.on_head_ok_packet(create_ok_r2(), diag);
//     throw_on_error(err, diag);

//     // Verify
//     BOOST_TEST(r.is_complete());
//     check_meta_empty(r.get_meta(0));
//     check_meta_empty(r.get_meta(1));
//     check_ok_r1(r, 0);
//     check_ok_r2(r, 1);
//     BOOST_TEST(r.num_resultsets() == 2u);
//     BOOST_TEST(r.get_rows(0) == rows_view());
//     BOOST_TEST(r.get_rows(1) == rows_view());
//     BOOST_TEST(r.get_out_params() == row_view());
// }

// BOOST_FIXTURE_TEST_CASE(three_resultsets_empty_empty_data, fixture)
// {
//     // First resultset
//     auto r = results_builder().ok(create_ok_r1(true)).build();

//     // Second resultset: OK packet indicates more results
//     auto err = r.on_head_ok_packet(create_ok_r2(true), diag);
//     BOOST_TEST(r.is_reading_first_subseq());

//     // Resultset r3: head indicates resultset with metadata
//     r.on_num_meta(3);
//     BOOST_TEST(r.is_reading_meta());

//     // Metadata
//     err = r.on_meta(create_coldef(protocol_field_type::float_), diag);
//     err = r.on_meta(create_coldef(protocol_field_type::double_), diag);
//     err = r.on_meta(create_coldef(protocol_field_type::tiny), diag);
//     BOOST_TEST(r.is_reading_rows());

//     // Read rows
//     rowbuff r1{4.2f, 5.0, 8}, r2{42.0f, 50.0, 80};
//     r.on_row_batch_start();
//     err = r.on_row(r1.ctx(), output_ref());
//     throw_on_error(err, diag);
//     err = r.on_row(r2.ctx(), output_ref());
//     throw_on_error(err, diag);

//     // End of resultset
//     err = r.on_row_ok_packet(create_ok_r3());
//     throw_on_error(err, diag);
//     r.on_row_batch_finish();

//     // Verify
//     BOOST_TEST(r.is_complete());
//     check_meta_empty(r.get_meta(0));
//     check_meta_empty(r.get_meta(1));
//     check_meta_r3(r.get_meta(2));
//     check_ok_r1(r, 0);
//     check_ok_r2(r, 1);
//     check_ok_r3(r, 2);
//     BOOST_TEST(r.num_resultsets() == 3u);
//     BOOST_TEST(r.get_rows(0) == rows_view());
//     BOOST_TEST(r.get_rows(1) == rows_view());
//     BOOST_TEST(r.get_rows(2) == makerows(3, 4.2f, 5.0, 8, 42.0f, 50.0, 80));
//     BOOST_TEST(r.get_out_params() == row_view());
// }

// // Verify that we do row slicing correctly
// BOOST_FIXTURE_TEST_CASE(three_resultsets_data_data_data, fixture)
// {
//     // Two first resultets
//     auto r = results_builder()
//                  .meta(create_meta_r1())
//                  .row(42, "abc")
//                  .row(50, "def")
//                  .ok(create_ok_r1(true))
//                  .meta(create_meta_r2())
//                  .row(60)
//                  .build();

//     // OK packet indicates more results
//     auto err = r.on_row_ok_packet(create_ok_r2(true));
//     throw_on_error(err, diag);

//     // Third resultset meta
//     r.on_num_meta(3);
//     err = r.on_meta(create_coldef(protocol_field_type::float_), diag);
//     err = r.on_meta(create_coldef(protocol_field_type::double_), diag);
//     err = r.on_meta(create_coldef(protocol_field_type::tiny), diag);
//     throw_on_error(err, diag);

//     // Rows
//     rowbuff r1{4.2f, 5.0, 8}, r2{42.0f, 50.0, 80};
//     r.on_row_batch_start();
//     err = r.on_row(r1.ctx(), output_ref());
//     throw_on_error(err, diag);
//     err = r.on_row(r2.ctx(), output_ref());
//     throw_on_error(err, diag);
//     r.on_row_batch_finish();

//     // OK packet
//     err = r.on_row_ok_packet(create_ok_r3());
//     throw_on_error(err, diag);

//     // Check results
//     BOOST_TEST(r.is_complete());
//     check_meta_r1(r.get_meta(0));
//     check_meta_r2(r.get_meta(1));
//     check_meta_r3(r.get_meta(2));
//     check_ok_r1(r, 0);
//     check_ok_r2(r, 1);
//     check_ok_r3(r, 2);
//     BOOST_TEST(r.num_resultsets() == 3u);
//     BOOST_TEST(r.get_rows(0) == makerows(2, 42, "abc", 50, "def"));
//     BOOST_TEST(r.get_rows(1) == makerows(1, 60));
//     BOOST_TEST(r.get_rows(2) == makerows(3, 4.2f, 5.0, 8, 42.0f, 50.0, 80));
//     BOOST_TEST(r.get_out_params() == makerow(60));
// }

// BOOST_FIXTURE_TEST_CASE(info_string_ownserhip, fixture)
// {
//     results_impl r;

//     // Head OK packet
//     std::string info = "Some info";
//     auto err = r.on_head_ok_packet(ok_builder().more_results(true).info(info).build(), diag);
//     throw_on_error(err, diag);

//     // Empty OK packet
//     info = "";
//     err = r.on_head_ok_packet(ok_builder().more_results(true).info(info).build(), diag);

//     // Row OK packet
//     info = "other info";
//     add_meta(r, create_meta_r2());
//     err = r.on_row_ok_packet(ok_builder().info(info).build());
//     info = "abcdfefgh";
//     BOOST_TEST(r.get_info(0) == "Some info");
//     BOOST_TEST(r.get_info(1) == "");
//     BOOST_TEST(r.get_info(2) == "other info");
// }

// BOOST_FIXTURE_TEST_CASE(multiple_row_batches, fixture)
// {
//     // Initial
//     auto r = results_builder().meta(create_meta_r1()).build();

//     // Buffers
//     rowbuff r1{42, "abc"}, r2{50, "bdef"}, r3{60, "pov"};

//     // First batch
//     r.on_row_batch_start();
//     auto err = r.on_row(r1.ctx(), output_ref());
//     throw_on_error(err);
//     err = r.on_row(r2.ctx(), output_ref());
//     throw_on_error(err);
//     r.on_row_batch_finish();

//     // Second batch (only one row)
//     r.on_row_batch_start();
//     err = r.on_row(r3.ctx(), output_ref());
//     throw_on_error(err);

//     // End of resultset
//     err = r.on_row_ok_packet(create_ok_r1());
//     throw_on_error(err);
//     r.on_row_batch_finish();

//     // Verify
//     BOOST_TEST(r.is_complete());
//     BOOST_TEST(r.num_resultsets() == 1u);
//     BOOST_TEST(r.get_rows(0) == makerows(2, 42, "abc", 50, "bdef", 60, "pov"));
// }

// BOOST_FIXTURE_TEST_CASE(empty_row_batch, fixture)
// {
//     // Initial
//     auto r = results_builder().meta(create_meta_r1()).build();

//     // No rows, directly eof
//     r.on_row_batch_start();
//     auto err = r.on_row_ok_packet(create_ok_r1());
//     throw_on_error(err);
//     r.on_row_batch_finish();

//     // Verify
//     BOOST_TEST(r.is_complete());
//     BOOST_TEST(r.num_resultsets() == 1u);
//     BOOST_TEST(r.get_rows(0) == makerows(2));  // empty but with 2 cols
// }

// BOOST_FIXTURE_TEST_CASE(error_deserializing_row, fixture)
// {
//     auto st = results_builder().meta(create_meta_r1()).build();
//     rowbuff bad_row{42, "abc"};
//     bad_row.data().push_back(0xff);

//     st.on_row_batch_start();
//     auto err = st.on_row(bad_row.ctx(), output_ref());
//     st.on_row_batch_finish();

//     BOOST_TEST(err == client_errc::extra_bytes);
// }

/**
reset with data-empty-data or similar
meta mismatch (head & rows)
num resultsets mismatch (more & less, head & rows)
row deserialize err
row parse error
ctors/assignments
 */

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
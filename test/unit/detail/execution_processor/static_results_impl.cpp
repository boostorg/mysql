//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/execution_processor/static_results_impl.hpp>

#include <boost/describe/class.hpp>
#include <boost/describe/operators.hpp>
#include <boost/test/unit_test.hpp>

#include "creation/create_meta.hpp"
#include "creation/create_static_results.hpp"
#include "execution_processor_helpers.hpp"
#include "printing.hpp"
#include "test_common.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using boost::mysql::detail::output_ref;
using boost::mysql::detail::static_results_erased_impl;
using boost::mysql::detail::static_results_impl;

namespace {

BOOST_AUTO_TEST_SUITE(test_static_results_impl)

// Row checking
template <class T>
void check_rows(boost::span<const T> actual, const std::vector<T>& expected)
{
    std::vector<T> actualv(actual.begin(), actual.end());
    BOOST_TEST(actualv == expected);
}

// OK packet checking
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

void check_ok_r3(const static_results_erased_impl& r, std::size_t idx)
{
    BOOST_TEST(r.get_affected_rows(idx) == 10u);
    BOOST_TEST(r.get_last_insert_id(idx) == 11u);
    BOOST_TEST(r.get_warning_count(idx) == 12u);
    BOOST_TEST(r.get_info(idx) == "");
    BOOST_TEST(r.get_is_out_params(idx) == false);
}

struct fixture
{
    diagnostics diag;
    std::vector<field_view> fields;
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
    auto err = r.on_meta(create_meta_r1_0(), diag);
    throw_on_error(err, diag);
    BOOST_TEST(r.is_reading_meta());

    // Second meta, ready to read rows
    err = r.on_meta(create_meta_r1_1(), diag);
    throw_on_error(err, diag);
    BOOST_TEST(r.is_reading_rows());

    // Rows
    rowbuff r1{42, "abc"};
    err = r.on_row(r1.ctx(), output_ref(), fields);
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
    err = r.on_meta(create_meta_r2_0(), diag);
    BOOST_TEST(r.is_reading_rows());

    // Row
    rowbuff r1{70};
    err = r.on_row(r1.ctx(), output_ref(), fields);
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

BOOST_FIXTURE_TEST_CASE(two_resultsets_empty_data, fixture)
{
    static_results_impl<empty, row2> rt;
    auto& r = rt.get_interface();

    // Empty resultset r1, indicating more results
    auto err = r.on_head_ok_packet(create_ok_r1(true), diag);
    throw_on_error(err, diag);
    BOOST_TEST(r.is_reading_first_subseq());

    // Resultset r2: indicates data
    r.on_num_meta(1);
    BOOST_TEST(r.is_reading_meta());

    // Metadata packet
    err = r.on_meta(create_meta_r2_0(), diag);
    throw_on_error(err, diag);
    BOOST_TEST(r.is_reading_rows());

    // Rows
    rowbuff r1{70};
    err = r.on_row(r1.ctx(), output_ref(), fields);
    throw_on_error(err, diag);
    BOOST_TEST(r.is_reading_rows());

    // Final OK packet
    err = r.on_row_ok_packet(create_ok_r2());
    throw_on_error(err, diag);

    // Verify
    std::vector<row2> expected_r2{{70}};
    BOOST_TEST(r.is_complete());
    check_meta_empty(r.get_meta(0));
    check_meta_r2(r.get_meta(1));
    check_ok_r1(r, 0);
    check_ok_r2(r, 1);
    BOOST_TEST(rt.get_rows<0>().empty());
    check_rows(rt.get_rows<1>(), expected_r2);
}

BOOST_FIXTURE_TEST_CASE(two_resultsets_data_empty, fixture)
{
    // Resultset r1
    auto rt = static_results_builder<row1, empty>()
                  .meta(create_meta_r1())
                  .row(42, "abc")
                  .row(50, "def")
                  .build();
    auto& r = rt.get_interface();

    // OK packet indicates more results
    auto err = r.on_row_ok_packet(create_ok_r1(true));
    throw_on_error(err, diag);
    BOOST_TEST(r.is_reading_first_subseq());

    // OK packet for 2nd result
    err = r.on_head_ok_packet(create_ok_r2(), diag);
    throw_on_error(err, diag);

    // Verify
    std::vector<row1> expected_r1{
        {"abc", 42},
        {"def", 50}
    };
    BOOST_TEST(r.is_complete());
    check_meta_r1(r.get_meta(0));
    check_meta_empty(r.get_meta(1));
    check_ok_r1(r, 0);
    check_ok_r2(r, 1);
    check_rows(rt.get_rows<0>(), expected_r1);
    BOOST_TEST(rt.get_rows<1>().empty());
}

BOOST_FIXTURE_TEST_CASE(two_resultsets_empty_empty, fixture)
{
    static_results_impl<empty, empty> rt;
    auto& r = rt.get_interface();

    // Resultset r1
    auto err = r.on_head_ok_packet(create_ok_r1(true), diag);
    throw_on_error(err, diag);
    BOOST_TEST(r.is_reading_first_subseq());

    // OK packet for 2nd result
    err = r.on_head_ok_packet(create_ok_r2(), diag);
    throw_on_error(err, diag);

    // Verify
    BOOST_TEST(r.is_complete());
    check_meta_empty(r.get_meta(0));
    check_meta_empty(r.get_meta(1));
    check_ok_r1(r, 0);
    check_ok_r2(r, 1);
    BOOST_TEST(rt.get_rows<0>().empty());
    BOOST_TEST(rt.get_rows<1>().empty());
}

BOOST_FIXTURE_TEST_CASE(three_resultsets_empty_empty_data, fixture)
{
    // First resultset
    auto rt = static_results_builder<empty, empty, row3>().ok(create_ok_r1(true)).build();
    auto& r = rt.get_interface();

    // Second resultset: OK packet indicates more results
    auto err = r.on_head_ok_packet(create_ok_r2(true), diag);
    BOOST_TEST(r.is_reading_first_subseq());

    // Resultset r3: head indicates resultset with metadata
    r.on_num_meta(3);
    BOOST_TEST(r.is_reading_meta());

    // Metadata
    err = r.on_meta(create_meta_r3_0(), diag);
    throw_on_error(err, diag);
    err = r.on_meta(create_meta_r3_1(), diag);
    throw_on_error(err, diag);
    err = r.on_meta(create_meta_r3_2(), diag);
    throw_on_error(err, diag);
    BOOST_TEST(r.is_reading_rows());

    // Read rows
    rowbuff r1{4.2f, 5.0, 8}, r2{42.0f, 50.0, 80};
    err = r.on_row(r1.ctx(), output_ref(), fields);
    throw_on_error(err, diag);
    err = r.on_row(r2.ctx(), output_ref(), fields);
    throw_on_error(err, diag);

    // End of resultset
    err = r.on_row_ok_packet(create_ok_r3());
    throw_on_error(err, diag);

    // Verify
    std::vector<row3> expected_r3{
        {5.0,  8,  4.2f },
        {50.0, 80, 42.0f}
    };
    BOOST_TEST(r.is_complete());
    check_meta_empty(r.get_meta(0));
    check_meta_empty(r.get_meta(1));
    check_meta_r3(r.get_meta(2));
    check_ok_r1(r, 0);
    check_ok_r2(r, 1);
    check_ok_r3(r, 2);
    BOOST_TEST(rt.get_rows<0>().empty());
    BOOST_TEST(rt.get_rows<1>().empty());
    check_rows(rt.get_rows<2>(), expected_r3);
}

BOOST_FIXTURE_TEST_CASE(three_resultsets_data_data_data, fixture)
{
    // Two first resultets
    auto rt = static_results_builder<row1, row2, row3>()
                  .meta(create_meta_r1())
                  .row(42, "abc")
                  .row(50, "def")
                  .ok(create_ok_r1(true))
                  .meta(create_meta_r2())
                  .row(60)
                  .build();
    auto& r = rt.get_interface();

    // OK packet indicates more results
    auto err = r.on_row_ok_packet(create_ok_r2(true));
    throw_on_error(err, diag);

    // Third resultset meta
    r.on_num_meta(3);
    err = r.on_meta(create_meta_r3_0(), diag);
    throw_on_error(err, diag);
    err = r.on_meta(create_meta_r3_1(), diag);
    throw_on_error(err, diag);
    err = r.on_meta(create_meta_r3_2(), diag);
    throw_on_error(err, diag);

    // Rows
    rowbuff r1{4.2f, 5.0, 8}, r2{42.0f, 50.0, 80};
    err = r.on_row(r1.ctx(), output_ref(), fields);
    throw_on_error(err, diag);
    err = r.on_row(r2.ctx(), output_ref(), fields);
    throw_on_error(err, diag);

    // OK packet
    err = r.on_row_ok_packet(create_ok_r3());
    throw_on_error(err, diag);

    // Verify
    std::vector<row1> expected_r1{
        {"abc", 42},
        {"def", 50}
    };
    std::vector<row2> expected_r2{{60}};
    std::vector<row3> expected_r3{
        {5.0,  8,  4.2f },
        {50.0, 80, 42.0f}
    };
    BOOST_TEST(r.is_complete());
    check_meta_r1(r.get_meta(0));
    check_meta_r2(r.get_meta(1));
    check_meta_r3(r.get_meta(2));
    check_ok_r1(r, 0);
    check_ok_r2(r, 1);
    check_ok_r3(r, 2);
    check_rows(rt.get_rows<0>(), expected_r1);
    check_rows(rt.get_rows<1>(), expected_r2);
    check_rows(rt.get_rows<2>(), expected_r3);
}

// Verify that reset clears all previous state
BOOST_FIXTURE_TEST_CASE(reset, fixture)
{
    // Previous state
    auto rt = static_results_builder<row1, row2, empty>()
                  .meta({
                      meta_builder().type(column_type::tinyint).name("ftiny").nullable(false).build(),
                      meta_builder().type(column_type::varchar).name("fvarchar").nullable(false).build(),
                  })
                  .row(21, "a string")
                  .row(90, "another string")
                  .ok(create_ok_r1(true))
                  .meta({
                      meta_builder().type(column_type::bigint).name("fbigint").nullable(false).build(),
                      meta_builder().type(column_type::char_).name("unrelated_field").nullable(false).build(),
                  })
                  .row(10, "aaa")
                  .row(2000, "bbb")
                  .ok(create_ok_r2(true))
                  .build();
    auto& r = rt.get_interface();

    r.on_num_meta(3);
    auto err = r.on_meta(
        meta_builder().type(column_type::float_).name("other").nullable(false).build(),
        diag
    );
    throw_on_error(err, diag);

    // Reset
    r.reset(detail::resultset_encoding::text, metadata_mode::minimal);
    BOOST_TEST(r.is_reading_first());

    // Use the object
    add_meta(r, create_meta_r1());
    add_row(r, 42, "abc");
    add_row(r, 50, "def");
    add_ok(r, create_ok_r1(true));

    add_meta(r, create_meta_r2());
    add_row(r, 100);
    add_ok(r, create_ok_r2(true));

    add_ok(r, create_ok_r3());

    // Verify
    std::vector<row1> expected_r1{
        {"abc", 42},
        {"def", 50}
    };
    std::vector<row2> expected_r2{{100}};
    BOOST_TEST(r.is_complete());
    check_meta_r1(r.get_meta(0));
    check_meta_r2(r.get_meta(1));
    check_meta_empty(r.get_meta(2));
    check_ok_r1(r, 0);
    check_ok_r2(r, 1);
    check_ok_r3(r, 2);
    check_rows(rt.get_rows<0>(), expected_r1);
    check_rows(rt.get_rows<1>(), expected_r2);
    BOOST_TEST(rt.get_rows<2>().empty());
}

BOOST_FIXTURE_TEST_CASE(info_string_ownserhip, fixture)
{
    static_results_impl<empty, empty, row2> rt;
    auto& r = rt.get_interface();

    // Head OK packet
    std::string info = "Some info";
    auto err = r.on_head_ok_packet(ok_builder().more_results(true).info(info).build(), diag);
    throw_on_error(err, diag);

    // Empty OK packet
    info = "";
    err = r.on_head_ok_packet(ok_builder().more_results(true).info(info).build(), diag);

    // Row OK packet
    info = "other info";
    add_meta(r, create_meta_r2());
    err = r.on_row_ok_packet(ok_builder().info(info).build());
    info = "abcdfefgh";
    BOOST_TEST(r.get_info(0) == "Some info");
    BOOST_TEST(r.get_info(1) == "");
    BOOST_TEST(r.get_info(2) == "other info");
}

// Verify that we clear the fields before adding new ones
BOOST_FIXTURE_TEST_CASE(storage_reuse, fixture)
{
    auto rt = static_results_builder<row1>().meta(create_meta_r1()).build();
    auto& r = rt.get_interface();

    // Rows
    rowbuff r1{42, "abc"}, r2{43, "def"};
    auto err = r.on_row(r1.ctx(), output_ref(), fields);
    throw_on_error(err, diag);
    err = r.on_row(r2.ctx(), output_ref(), fields);
    throw_on_error(err, diag);

    // End of resultset
    add_ok(r, create_ok_r1());

    // Verify results
    std::vector<row1> expected_r1{
        {"abc", 42},
        {"def", 43},
    };
    BOOST_TEST(fields.size() == 2u);
    check_rows(rt.get_rows<0>(), expected_r1);
}

BOOST_FIXTURE_TEST_CASE(error_meta_mismatch, fixture)
{
    static_results_impl<row1> rt;
    auto& r = rt.get_interface();

    r.on_num_meta(1);
    auto err = r.on_meta(
        meta_builder().type(column_type::bigint).name("fvarchar").nullable(false).build(),
        diag
    );

    const char* expected_msg =
        "Incompatible types for field 'fvarchar': C++ type 'string' is not compatible with DB type 'BIGINT'\n"
        "Field 'ftiny' is not present in the data returned by the server";
    BOOST_TEST(err == client_errc::metadata_check_failed);
    BOOST_TEST(diag.client_message() == expected_msg);
}

BOOST_FIXTURE_TEST_CASE(error_meta_mismatch_head, fixture)
{
    static_results_impl<row1> rt;
    auto& r = rt.get_interface();

    auto err = r.on_head_ok_packet(create_ok_r1(), diag);
    const char* expected_msg =
        "Field 'fvarchar' is not present in the data returned by the server\n"
        "Field 'ftiny' is not present in the data returned by the server";
    BOOST_TEST(err == client_errc::metadata_check_failed);
    BOOST_TEST(diag.client_message() == expected_msg);
}

BOOST_FIXTURE_TEST_CASE(error_deserializing_row, fixture)
{
    auto rt = static_results_builder<row1>().meta(create_meta_r1()).build();
    auto& r = rt.get_interface();
    rowbuff bad_row{42, "abc"};
    bad_row.data().push_back(0xff);

    auto err = r.on_row(bad_row.ctx(), output_ref(), fields);

    BOOST_TEST(err == client_errc::extra_bytes);
}

BOOST_FIXTURE_TEST_CASE(error_parsing_row, fixture)
{
    auto rt = static_results_builder<row1>().meta(create_meta_r1()).build();
    auto& r = rt.get_interface();
    rowbuff bad_row{nullptr, "abc"};  // should not be NULL - non_null used incorrectly, for instance

    auto err = r.on_row(bad_row.ctx(), output_ref(), fields);
    BOOST_TEST(err == client_errc::is_null);
}

BOOST_FIXTURE_TEST_CASE(error_too_few_resultsets_empty, fixture)
{
    static_results_impl<empty, row2> rt;
    auto& r = rt.get_interface();

    auto err = r.on_head_ok_packet(create_ok_r1(), diag);
    BOOST_TEST(err == client_errc::num_resultsets_mismatch);
}

BOOST_FIXTURE_TEST_CASE(error_too_many_resultsets_empty, fixture)
{
    static_results_impl<empty> rt;
    auto& r = rt.get_interface();

    auto err = r.on_head_ok_packet(create_ok_r1(true), diag);
    BOOST_TEST(err == client_errc::num_resultsets_mismatch);
}

BOOST_FIXTURE_TEST_CASE(error_too_few_resultsets_data, fixture)
{
    auto rt = static_results_builder<row1, row2>().meta(create_meta_r1()).build();
    auto& r = rt.get_interface();

    auto err = r.on_row_ok_packet(create_ok_r1());
    BOOST_TEST(err == client_errc::num_resultsets_mismatch);
}

BOOST_FIXTURE_TEST_CASE(error_too_many_resultsets_data, fixture)
{
    auto rt = static_results_builder<row1>().meta(create_meta_r1()).build();
    auto& r = rt.get_interface();

    auto err = r.on_row_ok_packet(create_ok_r1(true));
    BOOST_TEST(err == client_errc::num_resultsets_mismatch);
}

struct ctor_assign_fixture
{
    using results_t = static_results_impl<row1, row2>;

    std::unique_ptr<results_t> rt_old{new results_t{}};

    ctor_assign_fixture()
    {
        // Create and populate an object. Having it in the heap should make it easier to detect dangling
        // pointers
        add_meta(rt_old->get_interface(), create_meta_r1());
        add_row(rt_old->get_interface(), 42, "abc");
        add_row(rt_old->get_interface(), 50, "def");
        add_ok(rt_old->get_interface(), create_ok_r1(true));
        add_meta(rt_old->get_interface(), create_meta_r2());
        add_row(rt_old->get_interface(), 400);
        add_ok(rt_old->get_interface(), create_ok_r2());
    }

    // Checks that we correctly performed the copy/move, and that the object works
    // without dangling parts
    static void check_object(results_t& rt)
    {
        auto& r = rt.get_interface();

        // Data has been copied, and external data (like rows) doesn't dangle
        std::vector<row1> expected_r1{
            {"abc", 42},
            {"def", 50}
        };
        std::vector<row2> expected_r2{{400}};
        BOOST_TEST(r.is_complete());
        check_meta_r1(r.get_meta(0));
        check_meta_r2(r.get_meta(1));
        check_rows(rt.get_rows<0>(), expected_r1);
        check_rows(rt.get_rows<1>(), expected_r2);
        check_ok_r1(r, 0);
        check_ok_r2(r, 1);
    }
};

BOOST_FIXTURE_TEST_CASE(copy_ctor, ctor_assign_fixture)
{
    // Copy construct
    results_t rt{*rt_old};
    rt_old.reset();

    // Check
    check_object(rt);
}

BOOST_FIXTURE_TEST_CASE(move_ctor, ctor_assign_fixture)
{
    // Move construct
    results_t rt{std::move(*rt_old)};
    rt_old.reset();

    // Check
    check_object(rt);
}

BOOST_FIXTURE_TEST_CASE(copy_assignment, ctor_assign_fixture)
{
    // Create and populate the object we'll assign to
    results_t rt;
    add_meta(
        rt.get_interface(),
        {
            meta_builder().type(column_type::smallint).name("ftiny").nullable(false).build(),
            meta_builder().type(column_type::text).name("fvarchar").nullable(false).build(),
        }
    );

    // Assign
    rt = *rt_old;
    rt_old.reset();

    // Check
    check_object(rt);
}

BOOST_FIXTURE_TEST_CASE(move_assignment, ctor_assign_fixture)
{
    // Create and populate the object we'll assign to
    results_t rt;
    add_meta(
        rt.get_interface(),
        {
            meta_builder().type(column_type::smallint).name("ftiny").nullable(false).build(),
            meta_builder().type(column_type::text).name("fvarchar").nullable(false).build(),
        }
    );

    // Assign
    rt = std::move(*rt_old);
    rt_old.reset();

    // Check
    check_object(rt);
}

// Regression check: using tuples crashed. Having more fields in the query than the C++ type crashed
BOOST_AUTO_TEST_CASE(tuples)
{
    std::vector<field_view> fields;
    static_results_impl<row1_tuple, empty, row3_tuple> stp;
    auto& st = stp.get_interface();

    // Meta r1
    add_meta(st, create_meta_r1());

    // Rows r1
    rowbuff r1{10, "abc"};
    auto err = st.on_row(r1.ctx(), output_ref(), fields);
    throw_on_error(err);

    // EOF r1
    add_ok(st, create_ok_r1(true));

    // EOF r2
    add_ok(st, create_ok_r2(true));

    // Meta r3
    add_meta(st, create_meta_r3());

    // Rows r3
    rowbuff r3{4.2f, 90.0, 9};
    err = st.on_row(r3.ctx(), output_ref(), fields);
    throw_on_error(err);

    // OK r3
    add_ok(st, create_ok_r3());

    // Verify
    std::vector<row1_tuple> expected_r1{
        {10, "abc"}
    };
    std::vector<row3_tuple> expected_r3{
        {4.2f, 90.0}
    };
    BOOST_TEST(st.is_complete());
    check_meta_r1(st.get_meta(0));
    check_meta_empty(st.get_meta(1));
    check_meta_r3(st.get_meta(2));
    check_rows(stp.get_rows<0>(), expected_r1);
    BOOST_TEST(stp.get_rows<1>().empty());
    check_rows(stp.get_rows<2>(), expected_r3);
    check_ok_r1(st, 0);
    check_ok_r2(st, 1);
    check_ok_r3(st, 2);
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
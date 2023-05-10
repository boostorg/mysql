//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/detail/config.hpp>

#ifdef BOOST_MYSQL_CXX14

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/column_type.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/execution_processor/static_execution_state_impl.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>
#include <boost/mysql/detail/typing/get_type_index.hpp>

#include <boost/core/span.hpp>
#include <boost/describe/class.hpp>
#include <boost/describe/operators.hpp>
#include <boost/test/unit_test.hpp>

#include <cstddef>

#include "check_meta.hpp"
#include "creation/create_message_struct.hpp"
#include "creation/create_meta.hpp"
#include "creation/create_row_message.hpp"
#include "creation/create_static_execution_state.hpp"
#include "execution_processor_helpers.hpp"
#include "printing.hpp"
#include "test_common.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using boost::span;
using boost::mysql::detail::get_type_index;
using boost::mysql::detail::output_ref;
using boost::mysql::detail::resultset_encoding;
using boost::mysql::detail::static_execution_state_erased_impl;
using boost::mysql::detail::static_execution_state_impl;

namespace {

BOOST_AUTO_TEST_SUITE(test_static_execution_state_impl)

// OK packet data checking
void check_ok_r1(const static_execution_state_erased_impl& st)
{
    BOOST_TEST(st.get_affected_rows() == 1u);
    BOOST_TEST(st.get_last_insert_id() == 2u);
    BOOST_TEST(st.get_warning_count() == 4u);
    BOOST_TEST(st.get_info() == "Information");
    BOOST_TEST(st.get_is_out_params() == false);
}

void check_ok_r2(const static_execution_state_erased_impl& st)
{
    BOOST_TEST(st.get_affected_rows() == 5u);
    BOOST_TEST(st.get_last_insert_id() == 6u);
    BOOST_TEST(st.get_warning_count() == 8u);
    BOOST_TEST(st.get_info() == "more_info");
    BOOST_TEST(st.get_is_out_params() == true);
}

void check_ok_r3(const static_execution_state_erased_impl& st)
{
    BOOST_TEST(st.get_affected_rows() == 10u);
    BOOST_TEST(st.get_last_insert_id() == 11u);
    BOOST_TEST(st.get_warning_count() == 12u);
    BOOST_TEST(st.get_info() == "");
    BOOST_TEST(st.get_is_out_params() == false);
}

// Helper to create output_ref's
template <class... TList, class T>
output_ref create_ref(span<T> sp, std::size_t offset)
{
    return output_ref(sp, get_type_index<T, TList...>(), offset);
}

struct fixture
{
    diagnostics diag;
    std::vector<field_view> fields;
};

BOOST_FIXTURE_TEST_CASE(one_resultset_data, fixture)
{
    // Initial. Verify that we clear any previous result
    auto stp = static_exec_builder<row1>()
                   .reset(resultset_encoding::binary)
                   .meta({
                       meta_builder().type(column_type::smallint).name("ftiny").nullable(false).build(),
                       meta_builder().type(column_type::text).name("fvarchar").nullable(false).build(),
                   })
                   .ok(ok_builder().affected_rows(1).last_insert_id(2).warnings(3).info("abc").build())
                   .build();
    auto& st = stp.get_interface();

    // Reset
    st.reset(resultset_encoding::text, metadata_mode::full);
    BOOST_TEST(st.is_reading_first());

    // Head indicates resultset with metadata
    st.on_num_meta(2);
    BOOST_TEST(st.is_reading_meta());

    // First metadata
    auto err = st.on_meta(create_meta_r1_0(), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_meta());

    // Second metadata, ready to read rows
    err = st.on_meta(create_meta_r1_1(), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_rows());
    check_meta_r1(st.meta());

    // Rows
    row1 storage[2]{};
    rowbuff r1{10, "abc"}, r2{20, "cdef"};

    err = st.on_row(r1.ctx(), create_ref<row1, row1>(span<row1>(storage), 0), fields);
    BOOST_TEST(err == error_code());
    BOOST_TEST((storage[0] == row1{"abc", 10}));
    BOOST_TEST(storage[1] == row1{});

    err = st.on_row(r2.ctx(), create_ref<row1, row1>(span<row1>(storage), 1), fields);
    BOOST_TEST(err == error_code());
    BOOST_TEST((storage[0] == row1{"abc", 10}));
    BOOST_TEST((storage[1] == row1{"cdef", 20}));

    // End of resultset
    err = st.on_row_ok_packet(create_ok_r1());
    BOOST_TEST(err == error_code());
    BOOST_TEST(st.is_complete());
    check_meta_r1(st.meta());
    check_ok_r1(st);
}

BOOST_FIXTURE_TEST_CASE(one_resultset_empty, fixture)
{
    static_execution_state_impl<empty> stp;
    auto& st = stp.get_interface();

    auto err = st.on_head_ok_packet(create_ok_r1(), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_complete());
    check_meta_empty(st.meta());
    check_ok_r1(st);
}

BOOST_FIXTURE_TEST_CASE(two_resultsets_data_data, fixture)
{
    // Resultset r1 (rows are not stored anyhow in execution states)
    auto stp = static_exec_builder<row1, row2>()
                   .reset(resultset_encoding::text)
                   .meta(create_meta_r1())
                   .build();
    auto& st = stp.get_interface();

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
    err = st.on_meta(create_meta_r2_0(), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_rows());
    check_meta_r2(st.meta());

    // Rows
    rowbuff r1{90u};
    row2 storage[2]{};
    err = st.on_row(r1.ctx(), create_ref<row1, row2>(span<row2>(storage), 0), fields);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_rows());
    BOOST_TEST(storage[0] == row2{90u});
    BOOST_TEST(storage[1] == row2{});

    // OK packet, no more resultsets
    err = st.on_row_ok_packet(create_ok_r2());
    throw_on_error(err, diag);
    BOOST_TEST(st.is_complete());
    check_meta_r2(st.meta());
    check_ok_r2(st);
}

BOOST_FIXTURE_TEST_CASE(two_resultsets_empty_data, fixture)
{
    static_execution_state_impl<empty, row2> stp;
    auto& st = stp.get_interface();

    // Resultset r1
    auto err = st.on_head_ok_packet(create_ok_r1(true), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_first_subseq());
    check_meta_empty(st.meta());
    check_ok_r1(st);

    // Resultset r2: indicates data
    st.on_num_meta(1);
    BOOST_TEST(st.is_reading_meta());

    // Metadata packet
    err = st.on_meta(create_meta_r2_0(), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_rows());
    check_meta_r2(st.meta());

    // Rows
    rowbuff r1{90u}, r2{100u};
    row2 storage[2]{};
    err = st.on_row(r1.ctx(), create_ref<empty, row2>(span<row2>(storage), 0), fields);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_rows());
    BOOST_TEST(storage[0] == row2{90u});
    BOOST_TEST(storage[1] == row2{});

    err = st.on_row(r2.ctx(), create_ref<empty, row2>(span<row2>(storage), 1), fields);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_rows());
    BOOST_TEST(storage[0] == row2{90u});
    BOOST_TEST(storage[1] == row2{100u});

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
    auto stp = static_exec_builder<row1, empty>()
                   .reset(resultset_encoding::text)
                   .meta(create_meta_r1())
                   .build();
    auto& st = stp.get_interface();

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
    static_execution_state_impl<empty, empty> stp;
    auto& st = stp.get_interface();

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
    // First resultset
    auto stp = static_exec_builder<empty, empty, row3>().ok(create_ok_r1(true)).build();
    auto& st = stp.get_interface();

    // OK packet for second resultset indicates more results
    auto err = st.on_head_ok_packet(create_ok_r2(true), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_first_subseq());
    check_meta_empty(st.meta());
    check_ok_r2(st);

    // Resultset r3: head indicates resultset with metadata
    st.on_num_meta(3);
    BOOST_TEST(st.is_reading_meta());

    // Metadata
    err = st.on_meta(create_meta_r3_0(), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_meta());

    err = st.on_meta(create_meta_r3_1(), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_meta());

    err = st.on_meta(create_meta_r3_2(), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_rows());
    check_meta_r3(st.meta());

    // Rows
    rowbuff r1{4.2f, 90.0, 9};
    row3 storage[1]{};
    err = st.on_row(r1.ctx(), create_ref<empty, empty, row3>(span<row3>(storage), 0), fields);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_rows());
    BOOST_TEST((storage[0] == row3{90.0, 9, 4.2f}));

    // End of resultset
    err = st.on_row_ok_packet(create_ok_r3());
    BOOST_TEST(err == error_code());
    BOOST_TEST(st.is_complete());
    check_meta_r3(st.meta());
    check_ok_r3(st);
}

BOOST_FIXTURE_TEST_CASE(three_resultsets_data_empty_data, fixture)
{
    // First resultset
    auto stp = static_exec_builder<row1, empty, row3>().meta(create_meta_r1()).ok(create_ok_r1(true)).build();
    auto& st = stp.get_interface();

    // OK packet indicates more results
    auto err = st.on_head_ok_packet(create_ok_r2(true), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_first_subseq());
    check_meta_empty(st.meta());
    check_ok_r2(st);

    // Resultset r3: head indicates resultset with metadata
    st.on_num_meta(3);
    BOOST_TEST(st.is_reading_meta());

    // Metadata
    err = st.on_meta(create_meta_r3_0(), diag);
    throw_on_error(err, diag);

    err = st.on_meta(create_meta_r3_1(), diag);
    throw_on_error(err, diag);

    err = st.on_meta(create_meta_r3_2(), diag);
    throw_on_error(err, diag);
    BOOST_TEST(st.is_reading_rows());
    check_meta_r3(st.meta());

    // Rows
    rowbuff r1{4.2f, 90.0, 9};
    row3 storage[1]{};
    err = st.on_row(r1.ctx(), create_ref<row1, empty, row3>(span<row3>(storage), 0), fields);
    throw_on_error(err, diag);
    BOOST_TEST((storage[0] == row3{90.0, 9, 4.2f}));

    // End of resultset
    err = st.on_row_ok_packet(create_ok_r3());
    throw_on_error(err, diag);
    BOOST_TEST(st.is_complete());
    check_meta_r3(st.meta());
    check_ok_r3(st);
}

BOOST_FIXTURE_TEST_CASE(info_string_ownserhip_head_ok, fixture)
{
    static_execution_state_impl<empty> stp;
    auto& st = stp.get_interface();

    // OK packet received, doesn't own the string
    std::string info = "Some info";
    auto err = st.on_head_ok_packet(ok_builder().info(info).build(), diag);
    throw_on_error(err, diag);

    // st does, so changing info doesn't affect
    info = "other info";
    BOOST_TEST(st.get_info() == "Some info");
}

BOOST_FIXTURE_TEST_CASE(info_string_ownserhip_row_ok, fixture)
{
    auto stp = static_exec_builder<row1>().meta(create_meta_r1()).build();
    auto& st = stp.get_interface();

    // OK packet received, doesn't own the string
    std::string info = "Some info";
    auto err = st.on_row_ok_packet(ok_builder().info(info).build());
    throw_on_error(err, diag);

    // st does, so changing info doesn't affect
    info = "abcdfefgh";
    BOOST_TEST(st.get_info() == "Some info");
}

BOOST_FIXTURE_TEST_CASE(repeated_row_types, fixture)
{
    // Ready to read rows
    auto stp = static_exec_builder<row1, row1>()
                   .meta(create_meta_r1())
                   .ok(create_ok_r1(true))
                   .meta(create_meta_r1())
                   .build();
    auto& st = stp.get_interface();

    // Rows use type index 0, since they're the same type as resultset one's rows
    rowbuff r1{10, "abc"};
    row1 storage[1]{};
    auto err = st.on_row(r1.ctx(), create_ref<row1, row1>(span<row1>(storage), 0), fields);
    throw_on_error(err, diag);
    BOOST_TEST((storage[0] == row1{"abc", 10}));
}

// Verify that we clear the fields before adding new ones
BOOST_FIXTURE_TEST_CASE(storage_reuse, fixture)
{
    auto stp = static_exec_builder<row1>().meta(create_meta_r1()).build();
    auto& st = stp.get_interface();

    // Rows
    rowbuff r1{42, "abc"}, r2{43, "def"};
    row1 storage[2]{};
    auto err = st.on_row(r1.ctx(), create_ref<row1>(span<row1>(storage), 0), fields);
    throw_on_error(err, diag);
    err = st.on_row(r2.ctx(), create_ref<row1>(span<row1>(storage), 1), fields);
    throw_on_error(err, diag);

    // Verify results
    BOOST_TEST((storage[0] == row1{"abc", 42}));
    BOOST_TEST((storage[1] == row1{"def", 43}));
    BOOST_TEST(fields.size() == 2u);
}

BOOST_FIXTURE_TEST_CASE(error_meta_mismatch, fixture)
{
    static_execution_state_impl<row1> stp;
    auto& st = stp.get_interface();

    st.on_num_meta(1);
    auto err = st.on_meta(
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
    static_execution_state_impl<row1> stp;
    auto& st = stp.get_interface();

    auto err = st.on_head_ok_packet(create_ok_r1(), diag);
    const char* expected_msg =
        "Field 'fvarchar' is not present in the data returned by the server\n"
        "Field 'ftiny' is not present in the data returned by the server";
    BOOST_TEST(err == client_errc::metadata_check_failed);
    BOOST_TEST(diag.client_message() == expected_msg);
}

BOOST_FIXTURE_TEST_CASE(error_deserializing_row, fixture)
{
    auto stp = static_exec_builder<row1>().meta(create_meta_r1()).build();
    auto& st = stp.get_interface();
    rowbuff bad_row{42, "abc"};
    bad_row.data().push_back(0xff);

    row1 storage[1]{};
    auto err = st.on_row(bad_row.ctx(), create_ref<row1>(span<row1>(storage), 0), fields);
    BOOST_TEST(err == client_errc::extra_bytes);
}

BOOST_FIXTURE_TEST_CASE(error_parsing_row, fixture)
{
    auto stp = static_exec_builder<row1>().meta(create_meta_r1()).build();
    auto& st = stp.get_interface();
    rowbuff bad_row{nullptr, "abc"};  // should not be NULL - non_null used incorrectly, for instance

    row1 storage[1]{};
    auto err = st.on_row(bad_row.ctx(), create_ref<row1>(span<row1>(storage), 0), fields);
    BOOST_TEST(err == client_errc::is_null);
}

BOOST_FIXTURE_TEST_CASE(error_type_index_mismatch, fixture)
{
    auto stp = static_exec_builder<row1, row2>().meta(create_meta_r1()).build();
    auto& st = stp.get_interface();
    rowbuff r1{42, "abc"};

    row2 storage[1]{};
    auto err = st.on_row(r1.ctx(), create_ref<row1, row2>(span<row2>(storage), 0), fields);
    BOOST_TEST(err == client_errc::row_type_mismatch);
}

BOOST_FIXTURE_TEST_CASE(error_too_few_resultsets_empty, fixture)
{
    static_execution_state_impl<empty, row2> stp;
    auto& st = stp.get_interface();

    auto err = st.on_head_ok_packet(create_ok_r1(), diag);
    BOOST_TEST(err == client_errc::num_resultsets_mismatch);
}

BOOST_FIXTURE_TEST_CASE(error_too_many_resultsets_empty, fixture)
{
    static_execution_state_impl<empty> stp;
    auto& st = stp.get_interface();

    auto err = st.on_head_ok_packet(create_ok_r1(true), diag);
    BOOST_TEST(err == client_errc::num_resultsets_mismatch);
}

BOOST_FIXTURE_TEST_CASE(error_too_few_resultsets_data, fixture)
{
    auto stp = static_exec_builder<row1, row2>().meta(create_meta_r1()).build();
    auto& st = stp.get_interface();

    auto err = st.on_row_ok_packet(create_ok_r1());
    BOOST_TEST(err == client_errc::num_resultsets_mismatch);
}

BOOST_FIXTURE_TEST_CASE(error_too_many_resultsets_data, fixture)
{
    auto stp = static_exec_builder<row1>().meta(create_meta_r1()).build();
    auto& st = stp.get_interface();

    auto err = st.on_row_ok_packet(create_ok_r1(true));
    BOOST_TEST(err == client_errc::num_resultsets_mismatch);
}

struct ctor_assign_fixture
{
    // Using row3 because it has more fields, to verify pos_map
    using st_t = static_execution_state_impl<row1, row3>;

    std::unique_ptr<st_t> stp_old{new st_t{}};

    ctor_assign_fixture()
    {
        // Create and populate an object. Having it in the heap should make it easier to detect dangling
        // pointers
        add_meta(stp_old->get_interface(), create_meta_r1());
        add_ok(stp_old->get_interface(), create_ok_r1(true));
    }

    // Checks that we correctly performed the copy/move, and that the object works
    // without dangling parts
    static void check_object(static_execution_state_erased_impl& st)
    {
        // Data has been copied
        BOOST_TEST(st.is_reading_first_subseq());
        check_meta_r1(st.meta());
        check_ok_r1(st);

        // External data (pos_map and fields) does not dangle
        add_meta(st, create_meta_r3());
        check_meta_r3(st.meta());

        rowbuff r1{4.2f, 90.0, 9};
        row3 storage[1]{};
        std::vector<field_view> fields;
        auto err = st.on_row(r1.ctx(), create_ref<row1, row3>(span<row3>(storage), 0), fields);
        BOOST_TEST(err == error_code());
        BOOST_TEST((storage[0] == row3{90.0, 9, 4.2f}));
    }
};

BOOST_FIXTURE_TEST_CASE(copy_ctor, ctor_assign_fixture)
{
    // Copy construct
    st_t stp{*stp_old};
    stp_old.reset();

    // Check
    check_object(stp.get_interface());
}

BOOST_FIXTURE_TEST_CASE(move_ctor, ctor_assign_fixture)
{
    // Move construct
    st_t stp{std::move(*stp_old)};
    stp_old.reset();

    // Check
    check_object(stp.get_interface());
}

BOOST_FIXTURE_TEST_CASE(copy_assignment, ctor_assign_fixture)
{
    // Create and populate the object we'll assign to
    st_t stp;
    add_meta(
        stp.get_interface(),
        {
            meta_builder().type(column_type::smallint).name("ftiny").nullable(false).build(),
            meta_builder().type(column_type::text).name("fvarchar").nullable(false).build(),
        }
    );

    // Assign
    stp = *stp_old;
    stp_old.reset();

    // Check
    check_object(stp.get_interface());
}

BOOST_FIXTURE_TEST_CASE(move_assignment, ctor_assign_fixture)
{
    // Create and populate the object we'll assign to
    st_t stp;
    add_meta(
        stp.get_interface(),
        {
            meta_builder().type(column_type::smallint).name("ftiny").nullable(false).build(),
            meta_builder().type(column_type::text).name("fvarchar").nullable(false).build(),
        }
    );

    // Assign
    stp = std::move(*stp_old);
    stp_old.reset();

    // Check
    check_object(stp.get_interface());
}

// Regression check: using tuples crashed
BOOST_AUTO_TEST_CASE(tuples)
{
    std::vector<field_view> fields;
    static_execution_state_impl<row1_tuple, std::tuple<>, row3_tuple> stp;
    auto& st = stp.get_interface();

    // Meta r1
    add_meta(st, create_meta_r1());

    // Rows r1
    row1_tuple storage_1[2]{};
    rowbuff r1{10, "abc"};
    auto err = st.on_row(
        r1.ctx(),
        create_ref<row1_tuple, std::tuple<>, row3_tuple>(span<row1_tuple>(storage_1), 0),
        fields
    );
    throw_on_error(err);
    BOOST_TEST((storage_1[0] == row1_tuple{10, "abc"}));

    // EOF r1
    add_ok(st, create_ok_r1(true));
    check_ok_r1(st);

    // EOF r2
    add_ok(st, create_ok_r2(true));
    check_ok_r2(st);

    // Meta r3
    add_meta(st, create_meta_r3());

    // Rows r3
    row3_tuple storage_3[2]{};
    rowbuff r3{4.2f, 90.0, 9};
    err = st.on_row(
        r3.ctx(),
        create_ref<row1_tuple, std::tuple<>, row3_tuple>(span<row3_tuple>(storage_3), 0),
        fields
    );
    throw_on_error(err);
    BOOST_TEST((storage_3[0] == row3_tuple{4.2f, 90.0}));

    // OK r3
    add_ok(st, create_ok_r3());
    check_ok_r3(st);
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

#endif

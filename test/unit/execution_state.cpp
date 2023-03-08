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

#include <boost/test/unit_test.hpp>

#include "creation/create_message_struct.hpp"

using namespace boost::mysql::test;
using boost::mysql::column_type;
using boost::mysql::execution_state;
using boost::mysql::metadata_mode;
using boost::mysql::detail::protocol_field_type;

namespace {
BOOST_AUTO_TEST_SUITE(test_execution_state)

// The functionality has been tested in execution_state_impl already.
// Just spotchecks here
BOOST_AUTO_TEST_CASE(spotchecks)
{
    execution_state st;
    auto& impl = boost::mysql::detail::execution_state_access::get_impl(st);

    // Initial
    BOOST_TEST(st.should_read_head());
    BOOST_TEST(!st.should_read_rows());
    BOOST_TEST(!st.complete());

    // Reading meta
    impl.on_num_meta(1);
    BOOST_TEST(st.should_read_head());
    BOOST_TEST(!st.should_read_rows());
    BOOST_TEST(!st.complete());

    // Reading rows
    impl.on_meta(create_coldef(protocol_field_type::bit), metadata_mode::full);
    BOOST_TEST(!st.should_read_head());
    BOOST_TEST(st.should_read_rows());
    BOOST_TEST(!st.complete());
    BOOST_TEST(st.meta()[0].type() == column_type::bit);

    // Complete
    impl.on_row_ok_packet(create_ok_packet(1, 2, 0, 4, "abc"));
    BOOST_TEST(!st.should_read_head());
    BOOST_TEST(!st.should_read_rows());
    BOOST_TEST(st.complete());
    BOOST_TEST(st.meta()[0].type() == column_type::bit);
    BOOST_TEST(st.affected_rows() == 1);
    BOOST_TEST(st.last_insert_id() == 2);
    BOOST_TEST(st.warning_count() == 4);
    BOOST_TEST(st.info() == "abc");
    BOOST_TEST(!st.is_out_params());
}

// Verify that the lifetime guarantees we make are correct
void populate(execution_state& st)
{
    auto& impl = boost::mysql::detail::execution_state_access::get_impl(st);
    impl.on_num_meta(1);
    impl.on_meta(create_coldef(protocol_field_type::var_string), metadata_mode::full);
    impl.on_row_ok_packet(create_ok_packet(0, 0, 0, 0, "small"));
}

BOOST_AUTO_TEST_CASE(move_constructor)
{
    // Construction
    execution_state st;
    populate(st);

    // Obtain references
    auto meta = st.meta();
    auto info = st.info();

    // Move construct
    execution_state st2(std::move(st));
    st = execution_state();  // Regression check

    // Make sure that views are still valid
    BOOST_TEST_REQUIRE(meta.size() == 1u);
    BOOST_TEST(meta[0].type() == column_type::varchar);
    BOOST_TEST(info == "small");

    // The new object holds the same data
    BOOST_TEST_REQUIRE(st2.complete());
    BOOST_TEST_REQUIRE(st2.meta().size() == 1u);
    BOOST_TEST(st2.meta()[0].type() == column_type::varchar);
    BOOST_TEST(st2.info() == "small");
}

BOOST_AUTO_TEST_CASE(move_assignment)
{
    // Construct an execution_state with value
    execution_state st;
    populate(st);

    // Obtain references
    auto meta = st.meta();
    auto info = st.info();

    // Move assign
    execution_state st2;
    st2 = std::move(st);
    st = execution_state();  // Regression check - std::string impl SBO buffer

    // Make sure that views are still valid
    BOOST_TEST_REQUIRE(meta.size() == 1u);
    BOOST_TEST(meta[0].type() == column_type::varchar);
    BOOST_TEST(info == "small");

    // The new object holds the same data
    BOOST_TEST_REQUIRE(st2.complete());
    BOOST_TEST_REQUIRE(st2.meta().size() == 1u);
    BOOST_TEST(st2.meta()[0].type() == column_type::varchar);
    BOOST_TEST(st2.info() == "small");
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

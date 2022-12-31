//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/execution_state.hpp>

#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/protocol_types.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/test/unit_test.hpp>

#include "create_execution_state.hpp"
#include "create_message.hpp"

using namespace boost::mysql::test;
using boost::mysql::column_type;
using boost::mysql::execution_state;
using boost::mysql::detail::column_definition_packet;
using boost::mysql::detail::int_lenenc;
using boost::mysql::detail::ok_packet;
using boost::mysql::detail::protocol_field_type;
using boost::mysql::detail::resultset_encoding;
using boost::mysql::detail::string_lenenc;

namespace {

BOOST_AUTO_TEST_SUITE(test_execution_state)

BOOST_AUTO_TEST_CASE(member_fns)
{
    // Construction
    execution_state st;
    BOOST_TEST(!st.complete());
    BOOST_TEST(st.meta().size() == 0u);

    // Reset
    st.reset(resultset_encoding::binary);
    BOOST_TEST(!st.complete());
    BOOST_TEST(st.meta().size() == 0u);

    // Add meta
    column_definition_packet pack{};
    pack.type = protocol_field_type::var_string;
    st.add_meta(pack);
    pack.type = protocol_field_type::bit;
    st.add_meta(pack);

    BOOST_TEST(!st.complete());
    BOOST_TEST(st.meta().size() == 2u);
    BOOST_TEST(st.meta()[0].type() == column_type::varchar);
    BOOST_TEST(st.meta()[1].type() == column_type::bit);

    // Complete the resultset
    st.complete(create_ok_packet(4, 1, 0, 3, "info"));
    BOOST_TEST(st.complete());
    BOOST_TEST(st.affected_rows() == 4u);
    BOOST_TEST(st.last_insert_id() == 1u);
    BOOST_TEST(st.warning_count() == 3u);
    BOOST_TEST(st.info() == "info");

    // Reset
    st.reset(resultset_encoding::text);
    BOOST_TEST(!st.complete());
    BOOST_TEST(st.meta().size() == 0u);
}

BOOST_AUTO_TEST_CASE(move_constructor)
{
    // Construct a resultset with value
    auto st = create_execution_state(resultset_encoding::text, {protocol_field_type::var_string});
    st.complete(create_ok_packet(2, 3, 0, 4, "small"));

    // Obtain references
    auto meta = st.meta();
    auto info = st.info();

    // Move construct
    execution_state st2(std::move(st));
    st = execution_state();  // Regression check - std::string impl SBO buffer

    // Make sure that views are still valid
    BOOST_TEST(meta.at(0).type() == column_type::varchar);
    BOOST_TEST(info == "small");

    // The new object holds the same data
    BOOST_TEST_REQUIRE(st2.complete());
    BOOST_TEST(st2.meta().at(0).type() == column_type::varchar);
    BOOST_TEST(st2.info() == "small");
}

BOOST_AUTO_TEST_CASE(move_assignment)
{
    // Construct a resultset with value
    auto st = create_execution_state(resultset_encoding::text, {protocol_field_type::var_string});
    st.complete(create_ok_packet(2, 3, 0, 4, "small"));

    // Obtain references
    auto meta = st.meta();
    auto info = st.info();

    // Move assign
    execution_state st2;
    st2 = std::move(st);
    st = execution_state();  // Regression check - std::string impl SBO buffer

    // Make sure that views are still valid
    BOOST_TEST(meta.at(0).type() == column_type::varchar);
    BOOST_TEST(info == "small");

    // The new object holds the same data
    BOOST_TEST_REQUIRE(st2.complete());
    BOOST_TEST(st2.meta().at(0).type() == column_type::varchar);
    BOOST_TEST(st2.info() == "small");
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

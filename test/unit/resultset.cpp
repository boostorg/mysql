//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/resultset.hpp>

#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/protocol_types.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/test/unit_test.hpp>

#include "create_execution_state.hpp"
#include "create_message.hpp"
#include "test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::column_type;
using boost::mysql::resultset;
using boost::mysql::detail::int_lenenc;
using boost::mysql::detail::ok_packet;
using boost::mysql::detail::protocol_field_type;
using boost::mysql::detail::resultset_encoding;
using boost::mysql::detail::string_lenenc;

namespace {

BOOST_AUTO_TEST_SUITE(test_resultset)

BOOST_AUTO_TEST_CASE(has_value)
{
    // Default construction
    resultset result;
    BOOST_TEST_REQUIRE(!result.has_value());

    // Populate it
    result.state().complete(create_ok_packet(4, 1, 0, 3, "info"));

    // It's now valid
    BOOST_TEST_REQUIRE(result.has_value());
    BOOST_TEST(result.meta().size() == 0u);
    BOOST_TEST(result.rows().size() == 0u);
    BOOST_TEST(result.affected_rows() == 4u);
    BOOST_TEST(result.last_insert_id() == 1u);
    BOOST_TEST(result.warning_count() == 3u);
    BOOST_TEST(result.info() == "info");
}

BOOST_AUTO_TEST_CASE(move_constructor)
{
    // Construct a resultset with value
    resultset result;
    result.mutable_rows() = makerows(1, "abc", nullptr);
    result.state(
    ) = create_execution_state(resultset_encoding::text, {protocol_field_type::var_string});
    result.state().complete(create_ok_packet(2, 3, 0, 4, "small"));

    // Obtain references
    auto rws = result.rows();
    auto meta = result.meta();
    auto info = result.info();

    // Move construct
    resultset result2(std::move(result));
    result = resultset();  // Regression check - std::string impl SBO buffer

    // Make sure that views are still valid
    BOOST_TEST(rws == makerows(1, "abc", nullptr));
    BOOST_TEST(meta.at(0).type() == column_type::varchar);
    BOOST_TEST(info == "small");

    // The new object holds the same data
    BOOST_TEST_REQUIRE(result2.has_value());
    BOOST_TEST(result2.rows() == makerows(1, "abc", nullptr));
    BOOST_TEST(result2.meta().at(0).type() == column_type::varchar);
    BOOST_TEST(result2.info() == "small");
}

BOOST_AUTO_TEST_CASE(move_assignment)
{
    // Construct a resultset with value
    resultset result;
    result.mutable_rows() = makerows(1, "abc", nullptr);
    result.state(
    ) = create_execution_state(resultset_encoding::text, {protocol_field_type::var_string});
    result.state().complete(create_ok_packet(2, 3, 0, 4, "small"));

    // Obtain references
    auto rws = result.rows();
    auto meta = result.meta();
    auto info = result.info();

    // Move construct
    resultset result2;
    result2 = std::move(result);
    result = resultset();  // Regression check - std::string impl SBO buffer

    // Make sure that views are still valid
    BOOST_TEST(rws == makerows(1, "abc", nullptr));
    BOOST_TEST(meta.at(0).type() == column_type::varchar);
    BOOST_TEST(info == "small");

    // The new object holds the same data
    BOOST_TEST_REQUIRE(result2.has_value());
    BOOST_TEST(result2.rows() == makerows(1, "abc", nullptr));
    BOOST_TEST(result2.meta().at(0).type() == column_type::varchar);
    BOOST_TEST(result2.info() == "small");
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

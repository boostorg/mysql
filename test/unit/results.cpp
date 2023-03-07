//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/results.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/protocol_types.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>
#include <boost/mysql/impl/results.hpp>

#include <boost/test/unit_test.hpp>

#include "create_execution_state.hpp"
#include "create_message.hpp"
#include "test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::column_type;
using boost::mysql::field_view;
using boost::mysql::metadata_mode;
using boost::mysql::results;
using boost::mysql::detail::execution_state_access;
using boost::mysql::detail::protocol_field_type;
using boost::mysql::detail::results_access;
using boost::mysql::detail::resultset_encoding;

namespace {

BOOST_AUTO_TEST_SUITE(test_results)

void populate(results& r)
{
    auto& impl = boost::mysql::detail::results_access::get_impl(r);
    impl.on_num_meta(1);
    impl.on_meta(create_coldef(protocol_field_type::var_string), metadata_mode::minimal);
    auto fields = make_fv_arr("abc", nullptr);
    impl.rows().assign(fields.data(), fields.size());
    impl.on_row();
    impl.on_row();
    impl.on_row_ok_packet(create_ok_packet(0, 0, 0, 0, "small"));
}

BOOST_AUTO_TEST_CASE(has_value)
{
    // Default construction
    results result;
    BOOST_TEST_REQUIRE(!result.has_value());

    // Populate it
    populate(result);
    BOOST_TEST_REQUIRE(result.has_value());
}

// Verify view validity
BOOST_AUTO_TEST_CASE(move_constructor)
{
    // Construct a valid object
    results result;
    populate(result);

    // Obtain references. Note that iterators and resultset_view's don't remain valid.
    auto rws = result.rows();
    auto meta = result.meta();
    auto info = result.info();

    // Move construct
    results result2(std::move(result));
    result = results();  // Regression check - std::string impl SBO buffer

    // Make sure that views are still valid
    BOOST_TEST(rws == makerows(1, "abc", nullptr));
    BOOST_TEST_REQUIRE(meta.size() == 1u);
    BOOST_TEST(meta[0].type() == column_type::varchar);
    BOOST_TEST(info == "small");

    // The new object holds the same data
    BOOST_TEST_REQUIRE(result2.has_value());
    BOOST_TEST(result2.rows() == makerows(1, "abc", nullptr));
    BOOST_TEST_REQUIRE(result2.meta().size() == 1u);
    BOOST_TEST(result2.meta()[0].type() == column_type::varchar);
    BOOST_TEST(result2.info() == "small");
}

BOOST_AUTO_TEST_CASE(move_assignment)
{
    // Construct a results object
    results result;
    populate(result);

    // Obtain references
    auto rws = result.rows();
    auto meta = result.meta();
    auto info = result.info();

    // Move construct
    results result2;
    result2 = std::move(result);
    result = results();  // Regression check - std::string impl SBO buffer

    // Make sure that views are still valid
    BOOST_TEST(rws == makerows(1, "abc", nullptr));
    BOOST_TEST_REQUIRE(meta.size() == 1u);
    BOOST_TEST(meta[0].type() == column_type::varchar);
    BOOST_TEST(info == "small");

    // The new object holds the same data
    BOOST_TEST_REQUIRE(result2.has_value());
    BOOST_TEST(result2.rows() == makerows(1, "abc", nullptr));
    BOOST_TEST_REQUIRE(result2.meta().size() == 1u);
    BOOST_TEST(result2.meta()[0].type() == column_type::varchar);
    BOOST_TEST(result2.info() == "small");
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

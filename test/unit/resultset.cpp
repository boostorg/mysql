//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/resultset_view.hpp>

#include <boost/mysql/detail/protocol/constants.hpp>

#include <boost/test/unit_test.hpp>

#include "check_meta.hpp"
#include "creation/create_execution_state.hpp"
#include "creation/create_message_struct.hpp"
#include "printing.hpp"
#include "test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::column_type;
using boost::mysql::results;
using boost::mysql::resultset;
using boost::mysql::resultset_view;
using boost::mysql::detail::protocol_field_type;

namespace {

BOOST_AUTO_TEST_SUITE(test_resultset)

results create_initial_results()
{
    return create_results({
        {{protocol_field_type::var_string},
         makerows(1, "abc", nullptr),
         ok_builder().affected_rows(1).last_insert_id(2).warnings(3).info("1st").build()},

        {{protocol_field_type::tiny},
         makerows(1, 42),
         ok_builder().affected_rows(4).last_insert_id(5).warnings(6).info("2nd").out_params(true).build()},
    });
}

BOOST_AUTO_TEST_CASE(default_ctor)
{
    resultset r;
    BOOST_TEST(!r.has_value());
}

BOOST_AUTO_TEST_CASE(ctor_from_view_empty)
{
    resultset r{resultset_view{}};
    BOOST_TEST(!r.has_value());
}

BOOST_AUTO_TEST_CASE(ctor_from_view)
{
    results result = create_initial_results();
    resultset r{result.at(0)};
    result = results();

    BOOST_TEST_REQUIRE(r.has_value());
    BOOST_TEST(r.rows() == makerows(1, "abc", nullptr));
    check_meta(r.meta(), {column_type::varchar});
    BOOST_TEST(r.affected_rows() == 1u);
    BOOST_TEST(r.last_insert_id() == 2u);
    BOOST_TEST(r.warning_count() == 3u);
    BOOST_TEST(r.info() == "1st");
    BOOST_TEST(!r.is_out_params());
}

BOOST_AUTO_TEST_CASE(assignment_from_view_empty)
{
    results result = create_initial_results();
    resultset r{result.at(0)};
    r = resultset();
    result = results();

    BOOST_TEST(!r.has_value());
}

BOOST_AUTO_TEST_CASE(assignment_from_view)
{
    results result = create_initial_results();
    resultset r{result.at(0)};
    r = result.at(1);
    result = results();

    BOOST_TEST_REQUIRE(r.has_value());
    BOOST_TEST(r.rows() == makerows(1, 42));
    check_meta(r.meta(), {column_type::tinyint});
    BOOST_TEST(r.affected_rows() == 4u);
    BOOST_TEST(r.last_insert_id() == 5u);
    BOOST_TEST(r.warning_count() == 6u);
    BOOST_TEST(r.info() == "2nd");
    BOOST_TEST(r.is_out_params());
}

// View validity
BOOST_AUTO_TEST_CASE(move_constructor)
{
    // Construct object
    results result = create_initial_results();
    resultset r1(result.at(0));

    // Obtain references
    auto rws = r1.rows();
    auto meta = r1.meta();
    auto info = r1.info();

    // Move construct
    resultset r2(std::move(r1));
    r1 = resultset();

    // Make sure that views are still valid
    BOOST_TEST(rws == makerows(1, "abc", nullptr));
    check_meta(meta, {column_type::varchar});
    BOOST_TEST(info == "1st");

    // The new object holds the same data
    BOOST_TEST_REQUIRE(r2.has_value());
    BOOST_TEST(r2.rows() == makerows(1, "abc", nullptr));
    check_meta(r2.meta(), {column_type::varchar});
    BOOST_TEST(r2.info() == "1st");
}

BOOST_AUTO_TEST_CASE(move_assignment)
{
    // Construct object
    results result = create_initial_results();
    resultset r1(result.at(0));

    // Obtain references
    auto rws = r1.rows();
    auto meta = r1.meta();
    auto info = r1.info();

    // Move construct
    resultset r2;
    r2 = std::move(r1);
    r1 = resultset();

    // Make sure that views are still valid
    BOOST_TEST(rws == makerows(1, "abc", nullptr));
    BOOST_TEST_REQUIRE(meta.size() == 1u);
    BOOST_TEST(meta[0].type() == column_type::varchar);
    BOOST_TEST(info == "1st");

    // The new object holds the same data
    BOOST_TEST_REQUIRE(r2.has_value());
    BOOST_TEST(r2.rows() == makerows(1, "abc", nullptr));
    check_meta(r2.meta(), {column_type::varchar});
    BOOST_TEST(r2.info() == "1st");
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

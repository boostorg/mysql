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
#include <boost/mysql/resultset_view.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/protocol_types.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>
#include <boost/mysql/impl/results.hpp>

#include <boost/test/tools/old/interface.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>

#include <stdexcept>

#include "create_execution_state.hpp"
#include "create_message.hpp"
#include "printing.hpp"
#include "test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::column_type;
using boost::mysql::field_view;
using boost::mysql::metadata_mode;
using boost::mysql::results;
using boost::mysql::resultset_view;
using boost::mysql::detail::execution_state_access;
using boost::mysql::detail::protocol_field_type;
using boost::mysql::detail::results_access;
using boost::mysql::detail::resultset_encoding;
using boost::mysql::detail::SERVER_MORE_RESULTS_EXISTS;
using boost::mysql::detail::SERVER_PS_OUT_PARAMS;

namespace {

BOOST_AUTO_TEST_SUITE(test_results)

void populate(results& r)
{
    auto& impl = boost::mysql::detail::results_access::get_impl(r);

    // First resultset
    impl.on_num_meta(1);
    impl.on_meta(create_coldef(protocol_field_type::var_string), metadata_mode::minimal);
    auto fields = make_fv_arr("abc", nullptr);
    impl.rows().assign(fields.data(), fields.size());
    impl.on_row();
    impl.on_row();
    impl.on_row_ok_packet(create_ok_packet(0, 0, SERVER_MORE_RESULTS_EXISTS, 0, "1st"));

    // Second resultset
    auto flags = SERVER_MORE_RESULTS_EXISTS | SERVER_PS_OUT_PARAMS;
    impl.on_num_meta(1);
    impl.on_meta(create_coldef(protocol_field_type::tiny), metadata_mode::minimal);
    *impl.rows().add_fields(1) = field_view(42);
    impl.on_row();
    impl.on_row_ok_packet(create_ok_packet(4, 5, flags, 6, "2nd"));

    // Third resultset
    impl.on_head_ok_packet(create_ok_packet(0, 0, 0, 0, "3rd"));
}

struct fixture
{
    results result;
    fixture() { populate(result); }
};

BOOST_AUTO_TEST_CASE(has_value)
{
    // Default construction
    results result;
    BOOST_TEST_REQUIRE(!result.has_value());

    // Populate it
    populate(result);
    BOOST_TEST_REQUIRE(result.has_value());
}

BOOST_AUTO_TEST_SUITE(iterators)

BOOST_FIXTURE_TEST_CASE(basic, fixture)
{
    // Obtain iterators
    auto it = result.begin();   // should point to resultset 0
    auto itend = result.end();  // should point to resultset 3 (1 past end)

    // Check dereference
    BOOST_TEST((*it).info() == "1st");
    BOOST_TEST(it->info() == "1st");

    // Check ==
    BOOST_TEST(!(it == itend));
    BOOST_TEST(!(itend == it));
    BOOST_TEST(it == result.begin());
    BOOST_TEST(it == it);
    BOOST_TEST(itend == result.end());
    BOOST_TEST(itend == itend);

    // Check !=
    BOOST_TEST(it != itend);
    BOOST_TEST(itend != it);
    BOOST_TEST(!(it != result.begin()));
    BOOST_TEST(!(it != it));
    BOOST_TEST(!(itend != result.end()));
    BOOST_TEST(!(itend != itend));
}

BOOST_FIXTURE_TEST_CASE(prefix_increment, fixture)
{
    auto it = result.begin();
    auto& ref = (++it);
    BOOST_TEST(&ref == &it);
    BOOST_TEST(it->info() == "2nd");
    BOOST_TEST(it == result.begin() + 1);
}

BOOST_FIXTURE_TEST_CASE(postfix_increment, fixture)
{
    auto it = result.begin();
    auto it2 = it++;
    BOOST_TEST(it2 == result.begin());
    BOOST_TEST(it == result.begin() + 1);
    BOOST_TEST(it->info() == "2nd");
}

BOOST_FIXTURE_TEST_CASE(prefix_decrement, fixture)
{
    auto it = result.end();
    auto& ref = (--it);
    BOOST_TEST(&ref == &it);
    BOOST_TEST(it->info() == "3rd");
    BOOST_TEST(it == result.begin() + 2);
}

BOOST_FIXTURE_TEST_CASE(postfix_decrement, fixture)
{
    auto it = result.end();
    auto it2 = it--;
    BOOST_TEST(it2 == result.end());
    BOOST_TEST(it == result.begin() + 2);
    BOOST_TEST(it->info() == "3rd");
}

BOOST_FIXTURE_TEST_CASE(operator_square_brackets, fixture)
{
    auto it = result.begin();
    BOOST_TEST(it[0].info() == "1st");
    BOOST_TEST(it[1].info() == "2nd");
    BOOST_TEST(it[2].info() == "3rd");
}

BOOST_FIXTURE_TEST_CASE(operator_plus, fixture)
{
    auto it = result.begin();

    // Increment by 1
    auto it2 = it + 1;
    BOOST_TEST(it2->info() == "2nd");

    // Reversed operands
    it2 = 1 + it2;
    BOOST_TEST(it2->info() == "3rd");

    // Increment by more than 1
    BOOST_TEST(result.begin() + 3 == result.end());

    // Increment by 0
    BOOST_TEST(result.begin() + 0 == result.begin());

    // Negative increment
    BOOST_TEST(result.end() + (-2) == result.begin() + 1);
}

BOOST_FIXTURE_TEST_CASE(operator_plus_equals, fixture)
{
    auto it = result.begin();

    // Increment by 1
    it += 1;
    BOOST_TEST(it->info() == "2nd");

    // Increment by more than
    it += 2;
    BOOST_TEST(it == result.end());

    // Increment by 0
    it += 0;
    BOOST_TEST(it == result.end());

    // Negative increment
    it += (-2);
    BOOST_TEST(it == result.begin() + 1);
}

BOOST_FIXTURE_TEST_CASE(operator_minus, fixture)
{
    auto it = result.end();

    // Decrement by 1
    auto it2 = it - 1;
    BOOST_TEST(it2->info() == "3rd");

    // Decrement by more than 1
    BOOST_TEST(result.end() - 3 == result.begin());

    // Decrement by 0
    BOOST_TEST(result.end() - 0 == result.end());

    // Negative decrement
    BOOST_TEST(result.begin() - (-2) == result.begin() + 2);
}

BOOST_FIXTURE_TEST_CASE(operator_minus_equals, fixture)
{
    auto it = result.end();

    // Decrement by 1
    it -= 1;
    BOOST_TEST(it->info() == "3rd");

    // Decrement by more than 1
    it -= 2;
    BOOST_TEST(it == result.begin());

    // Decrement by 0
    it -= 0;
    BOOST_TEST(it == result.begin());

    // Negative decrement
    it -= (-2);
    BOOST_TEST(it == result.begin() + 2);
}

BOOST_FIXTURE_TEST_CASE(difference, fixture)
{
    auto first = result.begin();
    auto second = result.begin() + 1;
    auto last = result.end();

    BOOST_TEST(last - first == 3);
    BOOST_TEST(last - second == 2);
    BOOST_TEST(last - last == 0);
    BOOST_TEST(first - last == -3);
    BOOST_TEST(second - last == -2);
    BOOST_TEST(last - last == 0);
    BOOST_TEST(first - first == 0);
}

BOOST_FIXTURE_TEST_CASE(relational, fixture)
{
    auto first = result.begin();
    auto second = result.begin() + 1;
    auto third = result.begin() + 2;

    // Less than
    BOOST_TEST(first < second);
    BOOST_TEST(first <= second);
    BOOST_TEST(!(first > second));
    BOOST_TEST(!(first >= second));

    // Equal
    BOOST_TEST(!(second < second));
    BOOST_TEST(second <= second);
    BOOST_TEST(!(second > second));
    BOOST_TEST(second >= second);

    // Greater than
    BOOST_TEST(!(third < second));
    BOOST_TEST(!(third <= second));
    BOOST_TEST(third > second);
    BOOST_TEST(third >= second);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_CASE(collection_fns, fixture)
{
    // at
    BOOST_TEST(result.at(0).info() == "1st");
    BOOST_TEST(result.at(1).info() == "2nd");
    BOOST_TEST(result.at(2).info() == "3rd");
    BOOST_CHECK_THROW(result.at(3), std::out_of_range);

    // operator[]
    BOOST_TEST(result[0].info() == "1st");
    BOOST_TEST(result[1].info() == "2nd");
    BOOST_TEST(result[2].info() == "3rd");

    // front & back
    BOOST_TEST(result.front().info() == "1st");
    BOOST_TEST(result.back().info() == "3rd");

    // size & empty
    BOOST_TEST(result.size() == 3);
    BOOST_TEST(!result.empty());
}

BOOST_AUTO_TEST_SUITE(resultset_view_)

BOOST_AUTO_TEST_CASE(null_view)
{
    resultset_view v;
    BOOST_TEST(!v.has_value());
}

BOOST_FIXTURE_TEST_CASE(valid_view, fixture)
{
    auto v = result.at(1);
    BOOST_TEST_REQUIRE(v.has_value());
    BOOST_TEST(v.rows() == makerows(1, 42));
    BOOST_TEST_REQUIRE(v.meta().size() == 1);
    BOOST_TEST(v.meta()[0].type() == column_type::tinyint);
    BOOST_TEST(v.affected_rows() == 4);
    BOOST_TEST(v.last_insert_id() == 5);
    BOOST_TEST(v.warning_count() == 6);
    BOOST_TEST(v.info() == "2nd");
    BOOST_TEST(v.is_out_params());
}

BOOST_AUTO_TEST_SUITE_END()

// Verify view validity
BOOST_FIXTURE_TEST_CASE(move_constructor, fixture)
{
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
    BOOST_TEST(info == "1st");

    // The new object holds the same data
    BOOST_TEST_REQUIRE(result2.has_value());
    BOOST_TEST(result2.rows() == makerows(1, "abc", nullptr));
    BOOST_TEST_REQUIRE(result2.meta().size() == 1u);
    BOOST_TEST(result2.meta()[0].type() == column_type::varchar);
    BOOST_TEST(result2.info() == "1st");
}

BOOST_FIXTURE_TEST_CASE(move_assignment, fixture)
{
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
    BOOST_TEST(info == "1st");

    // The new object holds the same data
    BOOST_TEST_REQUIRE(result2.has_value());
    BOOST_TEST(result2.rows() == makerows(1, "abc", nullptr));
    BOOST_TEST_REQUIRE(result2.meta().size() == 1u);
    BOOST_TEST(result2.meta()[0].type() == column_type::varchar);
    BOOST_TEST(result2.info() == "1st");
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

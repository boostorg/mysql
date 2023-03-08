//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/resultset_view.hpp>

#include <boost/test/unit_test.hpp>

#include "check_meta.hpp"
#include "creation/create_execution_state.hpp"
#include "creation/create_message_struct.hpp"
#include "test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::column_type;
using boost::mysql::resultset_view;
using boost::mysql::detail::protocol_field_type;

namespace {

BOOST_AUTO_TEST_SUITE(test_resultset_view)

BOOST_AUTO_TEST_CASE(null_view)
{
    resultset_view v;
    BOOST_TEST(!v.has_value());
}

BOOST_AUTO_TEST_CASE(valid_view)
{
    auto result = create_results({
        {{protocol_field_type::tiny},
         makerows(1, 42),
         ok_builder().affected_rows(4).last_insert_id(5).warnings(6).info("2nd").out_params(true).build()}
    });

    auto v = result.at(0);
    BOOST_TEST_REQUIRE(v.has_value());
    BOOST_TEST(v.rows() == makerows(1, 42));
    check_meta(v.meta(), {column_type::tinyint});
    BOOST_TEST(v.affected_rows() == 4u);
    BOOST_TEST(v.last_insert_id() == 5u);
    BOOST_TEST(v.warning_count() == 6u);
    BOOST_TEST(v.info() == "2nd");
    BOOST_TEST(v.is_out_params());
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

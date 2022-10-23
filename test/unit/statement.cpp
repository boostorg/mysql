//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/detail/protocol/prepared_statement_messages.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/test/unit_test.hpp>

#include "test_channel.hpp"
#include "test_common.hpp"
#include "test_stream.hpp"

using namespace boost::mysql::test;
using boost::mysql::detail::com_stmt_prepare_ok_packet;
using statement_t = boost::mysql::statement<test_stream>;

namespace {

BOOST_AUTO_TEST_SUITE(test_statement)

test_channel chan = create_channel();

BOOST_AUTO_TEST_CASE(default_ctor)
{
    statement_t stmt;
    BOOST_TEST(!stmt.valid());
}

BOOST_AUTO_TEST_CASE(member_fns)
{
    statement_t stmt;
    stmt.reset(&chan, com_stmt_prepare_ok_packet{1, 2, 3, 4});

    BOOST_TEST(stmt.valid());
    BOOST_TEST(stmt.id() == 1u);
    BOOST_TEST(stmt.num_params() == 3u);
}

BOOST_AUTO_TEST_CASE(move_ctor_from_invalid)
{
    statement_t stmt1;
    statement_t stmt2(std::move(stmt1));

    BOOST_TEST(!stmt2.valid());
}

BOOST_AUTO_TEST_CASE(move_ctor_from_valid)
{
    statement_t stmt1;
    stmt1.reset(&chan, com_stmt_prepare_ok_packet{1, 2, 3, 4});
    statement_t stmt2(std::move(stmt1));

    BOOST_TEST(stmt2.valid());
    BOOST_TEST(stmt2.id() == 1u);
    BOOST_TEST(stmt2.num_params() == 3u);
}

BOOST_AUTO_TEST_CASE(move_assign_from_invalid)
{
    statement_t stmt1;
    stmt1.reset(&chan, com_stmt_prepare_ok_packet{1, 2, 3, 4});
    statement_t stmt2;

    stmt1 = std::move(stmt2);
    BOOST_TEST(!stmt1.valid());
}

BOOST_AUTO_TEST_CASE(move_assign_from_valid)
{
    statement_t stmt1;
    stmt1.reset(&chan, com_stmt_prepare_ok_packet{4, 8, 3, 9});
    statement_t stmt2;
    stmt2.reset(&chan, com_stmt_prepare_ok_packet{1, 2, 3, 4});

    stmt1 = std::move(stmt2);
    BOOST_TEST(stmt1.valid());
    BOOST_TEST(stmt1.id() == 1u);
    BOOST_TEST(stmt1.num_params() == 3u);
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

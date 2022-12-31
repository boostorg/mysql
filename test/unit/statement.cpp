//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/statement.hpp>

#include <boost/test/unit_test.hpp>

#include "test_common.hpp"
#include "test_connection.hpp"
#include "test_statement.hpp"

using namespace boost::mysql::test;

namespace {

BOOST_AUTO_TEST_SUITE(test_statement_)

test_connection conn;

BOOST_AUTO_TEST_CASE(default_ctor)
{
    test_statement stmt;
    BOOST_TEST(!stmt.valid());
}

BOOST_AUTO_TEST_CASE(member_fns)
{
    auto stmt = create_statement(conn, 3, 1);

    BOOST_TEST(stmt.valid());
    BOOST_TEST(stmt.num_params() == 3u);
    BOOST_TEST(stmt.id() == 1u);
}

BOOST_AUTO_TEST_CASE(move_ctor_from_invalid)
{
    test_statement stmt1;
    test_statement stmt2(std::move(stmt1));

    BOOST_TEST(!stmt2.valid());
}

BOOST_AUTO_TEST_CASE(move_ctor_from_valid)
{
    auto stmt1 = create_statement(conn, 3, 1);
    test_statement stmt2(std::move(stmt1));

    BOOST_TEST(stmt2.valid());
    BOOST_TEST(stmt2.num_params() == 3u);
    BOOST_TEST(stmt2.id() == 1u);
}

BOOST_AUTO_TEST_CASE(move_assign_from_invalid)
{
    auto stmt1 = create_statement(conn, 3, 1);
    test_statement stmt2;
    stmt1 = std::move(stmt2);

    BOOST_TEST(!stmt1.valid());
}

BOOST_AUTO_TEST_CASE(move_assign_from_valid)
{
    auto stmt1 = create_statement(conn, 8, 10);
    auto stmt2 = create_statement(conn, 3, 1);

    stmt1 = std::move(stmt2);
    BOOST_TEST(stmt1.valid());
    BOOST_TEST(stmt1.num_params() == 3u);
    BOOST_TEST(stmt1.id() == 1u);
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

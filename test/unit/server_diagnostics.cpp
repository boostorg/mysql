//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/server_diagnostics.hpp>

#include <boost/test/unit_test.hpp>

#include "printing.hpp"

using boost::mysql::server_diagnostics;

namespace {

BOOST_AUTO_TEST_SUITE(test_server_diagnostics)

BOOST_AUTO_TEST_CASE(operator_equals)
{
    BOOST_TEST(server_diagnostics() == server_diagnostics());
    BOOST_TEST(server_diagnostics("abc") == server_diagnostics("abc"));
    BOOST_TEST(!(server_diagnostics() == server_diagnostics("abc")));
    BOOST_TEST(!(server_diagnostics("def") == server_diagnostics("abc")));
}

BOOST_AUTO_TEST_CASE(operator_not_equals)
{
    BOOST_TEST(!(server_diagnostics() != server_diagnostics()));
    BOOST_TEST(!(server_diagnostics("abc") != server_diagnostics("abc")));
    BOOST_TEST(server_diagnostics() != server_diagnostics("abc"));
    BOOST_TEST(server_diagnostics("def") != server_diagnostics("abc"));
}

BOOST_AUTO_TEST_CASE(accessors)
{
    server_diagnostics diag("abc");
    BOOST_TEST(diag.message() == "abc");
}

BOOST_AUTO_TEST_SUITE_END()  // test_server_diagnostics

}  // namespace

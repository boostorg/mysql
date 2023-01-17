//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/server_diagnostics.hpp>
#include <boost/mysql/server_errc.hpp>
#include <boost/mysql/server_error.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/system/system_error.hpp>
#include <boost/test/tools/old/interface.hpp>
#include <boost/test/unit_test.hpp>

using boost::mysql::error_code;
using boost::mysql::server_diagnostics;
using boost::mysql::server_error;
using boost::mysql::throw_on_error;
using boost::system::system_error;

namespace {

BOOST_AUTO_TEST_SUITE(test_throw_on_error)

BOOST_AUTO_TEST_CASE(success)
{
    error_code ec;
    server_diagnostics diag("abc");
    BOOST_CHECK_NO_THROW(throw_on_error(ec, diag));
}

BOOST_AUTO_TEST_CASE(client_error)
{
    error_code ec(boost::mysql::client_errc::incomplete_message);
    server_diagnostics diag("abc");
    BOOST_CHECK_EXCEPTION(throw_on_error(ec, diag), system_error, [ec](const system_error& err) {
        return dynamic_cast<const server_error*>(&err) == nullptr && err.code() == ec;
    });
}

BOOST_AUTO_TEST_CASE(server_error_)
{
    error_code ec(boost::mysql::server_errc::no_such_db);
    server_diagnostics diag("abc");
    BOOST_CHECK_EXCEPTION(throw_on_error(ec, diag), server_error, [&](const server_error& err) {
        return err.code() == ec && err.diagnostics() == diag;
    });
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
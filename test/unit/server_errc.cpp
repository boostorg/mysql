//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/server_errc.hpp>

#include <boost/mysql/detail/auxiliar/stringize.hpp>

#include <boost/test/unit_test.hpp>

using boost::mysql::server_errc;
using boost::mysql::detail::error_to_string;
using boost::mysql::detail::stringize;

namespace {

BOOST_AUTO_TEST_SUITE(test_server_errc)

BOOST_AUTO_TEST_SUITE(error_to_string_)

BOOST_AUTO_TEST_CASE(regular)
{
    BOOST_TEST(error_to_string(server_errc::bad_db_error) == "bad_db_error");
}

BOOST_AUTO_TEST_CASE(unknown_error)
{
    BOOST_TEST(
        error_to_string(static_cast<server_errc>(0xfffefdfc)) == "<unknown MySQL server error>"
    );
}

BOOST_AUTO_TEST_CASE(coverage)
{
    // Check that no value causes problems.
    // Ensure that all branches of the switch/case are covered
    // Valid error ranges are 1000-2000 and 3000-5000
    for (int i = 1000; i <= 2000; ++i)
    {
        BOOST_CHECK_NO_THROW(error_to_string(static_cast<server_errc>(i)));
    }
    for (int i = 3000; i <= 5000; ++i)
    {
        BOOST_CHECK_NO_THROW(error_to_string(static_cast<server_errc>(i)));
    }
}
BOOST_AUTO_TEST_SUITE_END()  // error_to_string_

BOOST_AUTO_TEST_CASE(operator_stream) { BOOST_TEST(stringize(server_errc::no) == "no"); }

BOOST_AUTO_TEST_CASE(error_code_from_errc)
{
    boost::mysql::error_code code(server_errc::no_such_db);
    BOOST_TEST(code.value() == static_cast<int>(server_errc::no_such_db));
}

BOOST_AUTO_TEST_SUITE_END()  // test_client_errc

}  // namespace

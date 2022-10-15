//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "integration_test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::errc;
using boost::mysql::error_code;

namespace {

BOOST_AUTO_TEST_SUITE(test_connect)

// The OK case is already being tested by all other integ tests
// that require the connection to be connected

BOOST_MYSQL_NETWORK_TEST(physical_error, network_fixture)
{
    setup(sample.net);

    // Connect to a bad endpoint
    // depending on system and stream type, error code will be different
    conn->connect(er_endpoint::inexistent, params).validate_any_error({"physical connect failed"});
    BOOST_TEST(!conn->is_open());
}

BOOST_MYSQL_NETWORK_TEST(physical_ok_handshake_error, network_fixture)
{
    setup(sample.net);
    set_credentials("integ_user", "bad_password");
    conn->connect(er_endpoint::valid, params)
        .validate_error(errc::access_denied_error, {"access denied", "integ_user"});
    BOOST_TEST(!conn->is_open());
}

BOOST_AUTO_TEST_SUITE_END()  // test_connect

}  // namespace

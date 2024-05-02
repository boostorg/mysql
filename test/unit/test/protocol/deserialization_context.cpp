//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>

#include <boost/mysql/impl/internal/protocol/impl/deserialization_context.hpp>

#include <boost/test/unit_test.hpp>

using namespace boost::mysql::detail;
using boost::mysql::client_errc;
using boost::mysql::error_code;

BOOST_AUTO_TEST_SUITE(test_deserialization_context)

// TODO: do we want to test anything else here?

// going from deserialize_errc to error code
BOOST_AUTO_TEST_CASE(to_error_code_)
{
    BOOST_TEST(to_error_code(deserialize_errc::ok) == error_code());
    BOOST_TEST(
        to_error_code(deserialize_errc::incomplete_message) == error_code(client_errc::incomplete_message)
    );
    BOOST_TEST(
        to_error_code(deserialize_errc::protocol_value_error) == error_code(client_errc::protocol_value_error)
    );
    BOOST_TEST(
        to_error_code(deserialize_errc::server_unsupported) == error_code(client_errc::server_unsupported)
    );
}

BOOST_AUTO_TEST_SUITE_END()

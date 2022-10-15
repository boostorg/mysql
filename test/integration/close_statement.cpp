//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "integration_test_common.hpp"

using namespace boost::mysql::test;

namespace {

BOOST_AUTO_TEST_SUITE(test_close_statement)

BOOST_MYSQL_NETWORK_TEST(success, network_fixture)
{
    setup_and_connect(sample.net);

    // Prepare a statement
    conn->prepare_statement("SELECT * FROM empty_table", *stmt).validate_no_error();

    // Verify it works fine
    stmt->execute_collection({}, *result).validate_no_error();
    result->read_all().validate_no_error();  // clean all packets sent for this execution

    // Close the statement
    stmt->close().validate_no_error();

    // The statement is no longer valid
    BOOST_TEST(!stmt->base().valid());
}

BOOST_AUTO_TEST_SUITE_END()  // test_close_statement

}  // namespace

//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "integration_test_common.hpp"

using namespace boost::mysql::test;

BOOST_AUTO_TEST_SUITE(test_close_statement)

BOOST_MYSQL_NETWORK_TEST(existing_or_closed_statement, network_fixture)
{
    setup_and_connect(sample.net);

    // Prepare a statement
    auto stmt = conn->prepare_statement("SELECT * FROM empty_table").get();

    // Verify it works fine
    auto exec_result = stmt->execute_container({}).get();
    exec_result->read_all().validate_no_error(); // clean all packets sent for this execution

    // Close the statement
    stmt->close().validate_no_error();

    // Close it again
    stmt->close().validate_no_error();

    // Verify close took effect
    stmt->execute_container({}).validate_error(
        boost::mysql::errc::unknown_stmt_handler, {"unknown prepared statement"});
}

BOOST_AUTO_TEST_SUITE_END() // test_close_statement

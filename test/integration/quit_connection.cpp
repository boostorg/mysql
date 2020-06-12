//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "integration_test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::error_code;

namespace
{

template <typename Stream>
struct QuitConnectionTest : public NetworkTest<Stream>
{
    QuitConnectionTest()
    {
        this->connect(this->GetParam().ssl);
    }

    void ActiveConnection_ClosesIt()
    {
        network_functions<Stream>* net = this->GetParam().net;

        // Quit
        auto result = net->quit(this->conn);
        result.validate_no_error();

        // We are no longer able to query
        auto query_result = net->query(this->conn, "SELECT 1");
        query_result.validate_any_error();
    }
};

BOOST_MYSQL_NETWORK_TEST_SUITE(QuitConnectionTest)

BOOST_MYSQL_NETWORK_TEST(QuitConnectionTest, ActiveConnection_ClosesIt)

} // anon namespace


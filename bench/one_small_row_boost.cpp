//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/ssl_mode.hpp>

#include <boost/asio/io_context.hpp>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace asio = boost::asio;
namespace mysql = boost::mysql;

int main()
{
    // Setup
    asio::io_context ctx;
    mysql::any_connection conn(ctx);
    mysql::results r;

    // Connect
    mysql::connect_params params;
    params.server_address.emplace_unix_path("/var/run/mysqld/mysqld.sock");
    params.username = "root";
    params.password = "";
    params.database = "boost_mysql_bench";
    params.ssl = mysql::ssl_mode::disable;
    conn.connect(params);

    // Prepare the statement. Exclude the big TEXT/BLOB fields.
    auto stmt = conn.prepare_statement(
        "SELECT s8, u8, s16, u16, s32, u32, s64, u64, s1, b1, flt, dbl, dt, dtime, t "
        "FROM test_data WHERE id = 1"
    );

    // Ensure that nothing gets optimized away
    unsigned num_rows = 0;

    // Benchmark starts here
    auto tbegin = std::chrono::steady_clock::now();

    for (int i = 0; i < 10000; ++i)
    {
        // Since the rows are small, using execute is recommended
        conn.execute(stmt.bind(), r);
        num_rows += r.rows().size();
    }

    // Benchmark ends here
    auto tend = std::chrono::steady_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(tend - tbegin).count() << std::endl;

    // We expect one row per iteration
    return num_rows == 10000 ? EXIT_SUCCESS : EXIT_FAILURE;
}
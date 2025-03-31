//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/blob_view.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/date.hpp>
#include <boost/mysql/datetime.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/time.hpp>

#include <boost/asio/io_context.hpp>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

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

    // Prepare the statement. It should have many parameters and be a lightweight query.
    // This SELECT is lighter than an INSERT.
    auto stmt = conn.prepare_statement(
        "SELECT id FROM test_data WHERE id = 1 AND s8 = ? AND u8 = ? AND s16 = ? AND u16 = ? AND "
        "s32 = ? AND u32 = ? AND s64 = ? AND u64 = ? AND s1 = ? AND s2 = ? AND b1 = ? AND "
        "b2 = ? AND flt = ? AND dbl = ? AND dt = ? AND dtime = ? AND t = ?"
    );

    // Statement params
    signed char s8 = 64;
    unsigned char u8 = 172;
    short s16 = -129;
    unsigned short u16 = 0xfe21;
    int s32 = 42;
    unsigned u32 = 0xfe8173;
    long long s64 = -1;
    unsigned long long u64 = 98302402;
    std::string s1(200, 'a');
    std::string s2(36000, 'b');
    std::vector<unsigned char> b1(200, 5);
    std::vector<unsigned char> b2(35000, 7);
    float flt = 3.14e10;
    double dbl = 7.1e-150;
    mysql::date dt(2010, 6, 20);
    mysql::datetime dtime(2020, 3, 21, 10, 40, 10, 123456);
    mysql::time t = std::chrono::hours(126) + std::chrono::minutes(18) + std::chrono::seconds(40) +
                    std::chrono::microseconds(123456);

    // Ensure that nothing gets optimized away
    unsigned num_rows = 0;

    // Benchmark starts here
    auto tbegin = std::chrono::steady_clock::now();

    for (int i = 0; i < 1000; ++i)
    {
        // No rows will be matched, so execute() works
        conn.execute(
            stmt.bind(
                s8,
                u8,
                s16,
                u16,
                s32,
                u32,
                s64,
                u64,
                mysql::string_view(s1),
                mysql::string_view(s2),
                mysql::blob_view(b1),
                mysql::blob_view(b2),
                flt,
                dbl,
                dt,
                dtime,
                t
            ),
            r
        );
        num_rows += r.rows().size();
    }

    // Benchmark ends here
    auto tend = std::chrono::steady_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(tend - tbegin).count() << std::endl;

    // We don't expect any row to be matched
    return num_rows == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
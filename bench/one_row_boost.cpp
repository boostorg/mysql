#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/ssl_mode.hpp>

#include <boost/asio/io_context.hpp>

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

using namespace std;

namespace asio = boost::asio;
namespace mysql = boost::mysql;

int main()
{
    asio::io_context ctx;
    mysql::any_connection conn(ctx);
    mysql::connect_params params;
    params.server_address.emplace_unix_path("/var/run/mysqld/mysqld.sock");
    params.username = "root";
    params.password = "";
    params.database = "boost_mysql_bench";
    params.ssl = mysql::ssl_mode::disable;
    conn.connect(params);

    // Prepare stmt
    auto stmt = conn.prepare_statement("SELECT * FROM test_data WHERE id = 1");

    // Output results
    mysql::execution_state st;

    auto tbegin = std::chrono::steady_clock::now();
    for (int i = 0; i < 10000; ++i)
    {
        conn.start_execution(stmt.bind(), st);
        while (!st.complete())
            conn.read_some_rows(st);
    }
    auto tend = std::chrono::steady_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(tend - tbegin).count() << std::endl;
}
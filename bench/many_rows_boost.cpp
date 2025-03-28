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
#include <stdlib.h>
#include <string>

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

    unsigned res = 0;

    // Prepare stmt
    auto stmt = conn.prepare_statement("SELECT * FROM test_data");

    mysql::execution_state st;

    auto tbegin = std::chrono::steady_clock::now();
    conn.start_execution(stmt.bind(), st);
    while (!st.complete())
        res += conn.read_some_rows(st).size();
    auto tend = std::chrono::steady_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(tend - tbegin).count() << std::endl;

    return res != 0;
}
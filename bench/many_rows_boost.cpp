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

constexpr const char* create_table = R"SQL(
CREATE TEMPORARY TABLE myt(
    id INT NOT NULL PRIMARY KEY AUTO_INCREMENT,
    s8_0 TINYINT NOT NULL,
    u8_0 TINYINT UNSIGNED NOT NULL,
    s16_0 SMALLINT NOT NULL,
    u16_0 SMALLINT UNSIGNED NOT NULL,
    s32_0 INT NOT NULL,
    u32_0 INT UNSIGNED NOT NULL,
    s64_0 BIGINT NOT NULL,
    u64_0 BIGINT UNSIGNED NOT NULL,
    s1_0 VARCHAR(256),
    s2_0 TEXT,
    b1_0 VARBINARY(256),
    b2_0 BLOB,
    flt_0 FLOAT,
    dbl_0 DOUBLE,
    dt_0 DATE,
    dtime_0 DATETIME,
    t_0 TIME
)
)SQL";

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
    params.database = "mytest";
    params.ssl = mysql::ssl_mode::disable;
    conn.connect(params);

    // Run setup data
    std::ostringstream oss;
    oss << "INSERT INTO myt(s8_0, u8_0, s16_0, u16_0, s32_0, u32_0, s64_0, u64_0, s1_0, s2_0, b1_0, b2_0, "
           "flt_0, dbl_0, dt_0, dtime_0, t_0) VALUES ";
    bool is_first = true;
    for (int i = 0; i < 10000; ++i)
    {
        if (!is_first)
            oss << ", ";
        is_first = false;
        oss << "("
               "FLOOR(RAND()*(0x7f+0x80+1)-0x80),"
               "FLOOR(RAND()*(0xff+1)),"
               "FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),"
               "FLOOR(RAND()*(0xffff+1)),"
               "FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),"
               "FLOOR(RAND()*(0xffffffff+1)),"
               "FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),"
               "FLOOR(RAND()*(0xffffffffffffffff)),"
               "REPEAT(UUID(), 5),"
               "REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),"
               "REPEAT(UUID(), 5),"
               "REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),"
               "RAND(),"
               "RAND(),"
               "CURDATE(),"
               "CURTIME(),"
               "SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600))"
               ")";
    }
    auto insert_data = oss.str();

    mysql::results r1;
    conn.execute(create_table, r1);
    conn.execute(insert_data, r1);

    // Prepare stmt
    auto stmt = conn.prepare_statement("SELECT * FROM myt");

    // Output results
    mysql::execution_state st;
    for (int i = 0; i < 10; ++i)
    {
        // Bench begins here
        auto tbegin = std::chrono::steady_clock::now();
        conn.start_execution(stmt.bind(), st);
        while (!st.complete())
            conn.read_some_rows(st);
        auto tend = std::chrono::steady_clock::now();
        // Bench ends here
        std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(tend - tbegin).count()
                  << std::endl;
    }
}
//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mariadb/mysql.h>
#include <string>
#include <vector>

int main()
{
    // Initialize
    if (mysql_library_init(0, NULL, NULL))
    {
        fprintf(stderr, "could not initialize MySQL client library\n");
        exit(1);
    }
    MYSQL* con = mysql_init(NULL);
    if (con == NULL)
    {
        fprintf(stderr, "Error initializing connection: %s\n", mysql_error(con));
        exit(1);
    }

    // Connect
    if (mysql_real_connect(con, NULL, "root", "", "boost_mysql_bench", 0, "/var/run/mysqld/mysqld.sock", 0) ==
        NULL)
    {
        fprintf(stderr, "%s\n", mysql_error(con));
        mysql_close(con);
        exit(1);
    }

    // Prepare the statement. It should have many parameters and be a lightweight query.
    // This SELECT is lighter than an INSERT.
    MYSQL_STMT* stmt;
    stmt = mysql_stmt_init(con);
    if (!stmt)
    {
        printf("Could not initialize statement\n");
        exit(1);
    }
    constexpr const char* stmt_str =
        "SELECT id FROM test_data WHERE id = 1 AND s8 = ? AND u8 = ? AND s16 = ? AND u16 = ? AND "
        "s32 = ? AND u32 = ? AND s64 = ? AND u64 = ? AND s1 = ? AND s2 = ? AND b1 = ? AND "
        "b2 = ? AND flt = ? AND dbl = ? AND dt = ? AND dtime = ? AND t = ?";
    if (mysql_stmt_prepare(stmt, stmt_str, strlen(stmt_str)))
    {
        fprintf(stderr, "Error preparing statement: %s\n", mysql_stmt_error(stmt));
        exit(1);
    }

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
    MYSQL_TIME dt{}, dtime{}, t{};

    dt.year = 2010;
    dt.month = 6;
    dt.day = 20;

    dtime.year = 2020;
    dtime.month = 3;
    dtime.day = 21;
    dtime.hour = 10;
    dtime.minute = 40;
    dtime.second = 10;
    dtime.second_part = 123456;

    t.hour = 126;
    t.minute = 18;
    t.second = 40;
    t.second_part = 123456;

    // Prepare the bind objects
    MYSQL_BIND in_binds[17]{};
    in_binds[0].buffer_type = MYSQL_TYPE_TINY;
    in_binds[0].buffer = &s8;
    in_binds[0].buffer_length = 1;
    in_binds[0].is_unsigned = 0;

    in_binds[1].buffer_type = MYSQL_TYPE_TINY;
    in_binds[1].buffer = &u8;
    in_binds[1].buffer_length = 1;
    in_binds[1].is_unsigned = 1;

    in_binds[2].buffer_type = MYSQL_TYPE_SHORT;
    in_binds[2].buffer = &s16;
    in_binds[2].buffer_length = 2;
    in_binds[2].is_unsigned = 0;

    in_binds[3].buffer_type = MYSQL_TYPE_SHORT;
    in_binds[3].buffer = &u16;
    in_binds[3].buffer_length = 2;
    in_binds[3].is_unsigned = 1;

    in_binds[4].buffer_type = MYSQL_TYPE_LONG;
    in_binds[4].buffer = &s32;
    in_binds[4].buffer_length = 4;
    in_binds[4].is_unsigned = 0;

    in_binds[5].buffer_type = MYSQL_TYPE_LONG;
    in_binds[5].buffer = &u32;
    in_binds[5].buffer_length = 4;
    in_binds[5].is_unsigned = 1;

    in_binds[6].buffer_type = MYSQL_TYPE_LONGLONG;
    in_binds[6].buffer = &s64;
    in_binds[6].buffer_length = 8;
    in_binds[6].is_unsigned = 0;

    in_binds[7].buffer_type = MYSQL_TYPE_LONGLONG;
    in_binds[7].buffer = &u64;
    in_binds[7].buffer_length = 8;
    in_binds[7].is_unsigned = 1;

    in_binds[8].buffer_type = MYSQL_TYPE_STRING;
    in_binds[8].buffer = s1.data();
    in_binds[8].buffer_length = s1.size();

    in_binds[9].buffer_type = MYSQL_TYPE_STRING;
    in_binds[9].buffer = s2.data();
    in_binds[9].buffer_length = s2.size();

    in_binds[10].buffer_type = MYSQL_TYPE_BLOB;
    in_binds[10].buffer = b1.data();
    in_binds[10].buffer_length = b1.size();

    in_binds[11].buffer_type = MYSQL_TYPE_BLOB;
    in_binds[11].buffer = b2.data();
    in_binds[11].buffer_length = b2.size();

    in_binds[12].buffer_type = MYSQL_TYPE_FLOAT;
    in_binds[12].buffer = &flt;
    in_binds[12].buffer_length = 4;

    in_binds[13].buffer_type = MYSQL_TYPE_DOUBLE;
    in_binds[13].buffer = &dbl;
    in_binds[13].buffer_length = 8;

    in_binds[14].buffer_type = MYSQL_TYPE_DATE;
    in_binds[14].buffer = &dt;
    in_binds[14].buffer_length = sizeof(dt);

    in_binds[15].buffer_type = MYSQL_TYPE_DATETIME;
    in_binds[15].buffer = &dtime;
    in_binds[15].buffer_length = sizeof(dtime);

    in_binds[16].buffer_type = MYSQL_TYPE_TIME;
    in_binds[16].buffer = &t;
    in_binds[16].buffer_length = sizeof(t);

    // Prepare the output bind objects (only one parameter)
    long long int out_id = 0;
    MYSQL_BIND out_binds[1]{};
    out_binds[0].buffer_type = MYSQL_TYPE_LONGLONG;
    out_binds[0].buffer = &out_id;
    out_binds[0].buffer_length = 8;

    // Ensure that nothing gets optimized away
    unsigned num_rows = 0;

    // Benchmark starts here
    auto tbegin = std::chrono::steady_clock::now();

    for (int i = 0; i < 1000; ++i)
    {
        // Bind the params
        if (mysql_stmt_bind_param(stmt, in_binds))
        {
            fprintf(stderr, "Error binding params: %s\n", mysql_stmt_error(stmt));
            exit(1);
        }

        // Execute the statement
        if (mysql_stmt_execute(stmt))
        {
            fprintf(stderr, "Error executing statement: %s\n", mysql_stmt_error(stmt));
            exit(1);
        }

        // Bind output
        if (mysql_stmt_bind_result(stmt, out_binds))
        {
            fprintf(stderr, "Error binding result: %s\n", mysql_stmt_error(stmt));
            exit(1);
        }

        // Read the rows
        while (true)
        {
            auto status = mysql_stmt_fetch(stmt);
            if (status == MYSQL_DATA_TRUNCATED)
            {
                // No truncation is expected here
                fprintf(stderr, "Data truncation error\n");
                exit(1);
            }
            else if (status == MYSQL_NO_DATA)
            {
                break;
            }
            else if (status == 1)
            {
                fprintf(stderr, "Error fetching result: %s\n", mysql_stmt_error(stmt));
                exit(1);
            }
            else
            {
                ++num_rows;
            }
        }
    }

    // Benchmark ends here
    auto tend = std::chrono::steady_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(tend - tbegin).count() << std::endl;

    // Cleanup
    mysql_stmt_close(stmt);
    mysql_close(con);

    // We don't expect any rows to be matched
    return num_rows == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <mysql/field_types.h>
#include <mysql/mysql.h>
#include <mysql/mysql_com.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

using namespace std;

int main()
{
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

    unsigned mode = SSL_MODE_DISABLED;
    if (mysql_options(con, MYSQL_OPT_SSL_MODE, &mode))
    {
        fprintf(stderr, "Error in mysql_options: %s\n", mysql_error(con));
        exit(1);
    }

    if (mysql_real_connect(con, NULL, "root", "", "mytest", 0, "/var/run/mysqld/mysqld.sock", 0) == NULL)
    {
        fprintf(stderr, "%s\n", mysql_error(con));
        mysql_close(con);
        exit(1);
    }

    // Prepare stmt
    MYSQL_STMT* stmt;
    stmt = mysql_stmt_init(con);
    if (!stmt)
    {
        printf("Could not initialize statement\n");
        exit(1);
    }
    constexpr const char* stmt_str = "SELECT * FROM test_data";
    if (mysql_stmt_prepare(stmt, stmt_str, strlen(stmt_str)))
    {
        fprintf(stderr, "Error preparing statement: %s\n", mysql_stmt_error(stmt));
        exit(1);
    }

    // Prepare the bind objects
    long long int out_id = 0;
    MYSQL_BIND binds[21]{};
    binds[0].buffer_type = MYSQL_TYPE_LONGLONG;
    binds[0].buffer = &out_id;
    binds[0].buffer_length = 8;

    signed char s8{};
    binds[1].buffer_type = MYSQL_TYPE_TINY;
    binds[1].buffer = &s8;
    binds[1].buffer_length = 1;
    binds[1].is_unsigned = 0;

    unsigned char u8{};
    binds[2].buffer_type = MYSQL_TYPE_TINY;
    binds[2].buffer = &u8;
    binds[2].buffer_length = 1;
    binds[2].is_unsigned = 1;

    short s16{};
    binds[3].buffer_type = MYSQL_TYPE_SHORT;
    binds[3].buffer = &s16;
    binds[3].buffer_length = 2;
    binds[3].is_unsigned = 0;

    unsigned short u16{};
    binds[4].buffer_type = MYSQL_TYPE_SHORT;
    binds[4].buffer = &u16;
    binds[4].buffer_length = 2;
    binds[4].is_unsigned = 1;

    int s32{};
    binds[5].buffer_type = MYSQL_TYPE_LONG;
    binds[5].buffer = &s32;
    binds[5].buffer_length = 4;
    binds[5].is_unsigned = 0;

    unsigned u32{};
    binds[6].buffer_type = MYSQL_TYPE_LONG;
    binds[6].buffer = &u32;
    binds[6].buffer_length = 4;
    binds[6].is_unsigned = 1;

    long long s64{};
    binds[7].buffer_type = MYSQL_TYPE_LONGLONG;
    binds[7].buffer = &s64;
    binds[7].buffer_length = 8;
    binds[7].is_unsigned = 0;

    unsigned long long u64{};
    binds[8].buffer_type = MYSQL_TYPE_LONGLONG;
    binds[8].buffer = &u64;
    binds[8].buffer_length = 8;
    binds[8].is_unsigned = 1;

    char s1[255]{};
    binds[9].buffer_type = MYSQL_TYPE_STRING;
    binds[9].buffer = s1;
    binds[9].buffer_length = sizeof(s1);

    std::string s2;
    unsigned long s2_length = 0u;
    bool s2_truncated{};
    binds[10].buffer_type = MYSQL_TYPE_STRING;
    binds[10].buffer = s2.data();
    binds[10].buffer_length = s2_length;
    binds[10].length = &s2_length;
    binds[10].error = &s2_truncated;

    char b1[255]{};
    binds[11].buffer_type = MYSQL_TYPE_BLOB;
    binds[11].buffer = b1;
    binds[11].buffer_length = sizeof(b1);

    std::string b2;
    unsigned long b2_length = 0u;
    bool b2_truncated{};
    binds[12].buffer_type = MYSQL_TYPE_BLOB;
    binds[12].buffer = b2.data();
    binds[12].buffer_length = b2_length;
    binds[12].length = &b2_length;
    binds[12].error = &b2_truncated;

    float flt{};
    binds[12].buffer_type = MYSQL_TYPE_FLOAT;
    binds[12].buffer = &flt;
    binds[12].buffer_length = 4;

    double dbl{};
    binds[13].buffer_type = MYSQL_TYPE_DOUBLE;
    binds[13].buffer = &dbl;
    binds[13].buffer_length = 8;

    MYSQL_TIME dt{};
    binds[14].buffer_type = MYSQL_TYPE_DATE;
    binds[14].buffer = &dt;
    binds[14].buffer_length = sizeof(dt);

    MYSQL_TIME dtime{};
    binds[15].buffer_type = MYSQL_TYPE_DATETIME;
    binds[15].buffer = &dtime;
    binds[15].buffer_length = sizeof(dtime);

    MYSQL_TIME t{};
    binds[16].buffer_type = MYSQL_TYPE_TIME;
    binds[16].buffer = &t;
    binds[16].buffer_length = sizeof(t);

    auto tbegin = std::chrono::steady_clock::now();
    if (mysql_stmt_execute(stmt))
    {
        fprintf(stderr, "Error executing statement: %s\n", mysql_stmt_error(stmt));
        exit(1);
    }

    if (mysql_stmt_bind_result(stmt, binds))
    {
        fprintf(stderr, "Error binding result: %s\n", mysql_stmt_error(stmt));
        exit(1);
    }

    while (true)
    {
        auto status = mysql_stmt_fetch(stmt);

        if (status == MYSQL_DATA_TRUNCATED)
        {
            if (s2_length > s2.size())
            {
                s2.resize(s2_length);
                binds[10].buffer = s2.data();
                binds[10].buffer_length = s2_length;
                if (mysql_stmt_fetch_column(stmt, &binds[10], 10, 0))
                {
                    fprintf(stderr, "Error fetching s2: %s\n", mysql_stmt_error(stmt));
                    exit(1);
                }
            }

            if (b2_length > b2.size())
            {
                b2.resize(b2_length);
                binds[12].buffer = b2.data();
                binds[12].buffer_length = b2_length;
                if (mysql_stmt_fetch_column(stmt, &binds[12], 12, 0))
                {
                    fprintf(stderr, "Error fetching b2: %s\n", mysql_stmt_error(stmt));
                    exit(1);
                }
            }
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
    }

    auto tend = std::chrono::steady_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(tend - tbegin).count() << std::endl;

    mysql_stmt_close(stmt);
    mysql_close(con);
    exit(0);
}
//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_async_callbacks

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/connection.hpp>
#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/pool_params.hpp>
#include <boost/mysql/pooled_connection.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>

#include <asm-generic/errno.h>
#include <chrono>
#include <cstddef>
#include <iostream>

using boost::mysql::error_code;
namespace mysql = boost::mysql;
namespace asio = boost::asio;

static constexpr std::size_t num_parallel = 100;
static constexpr std::size_t total = num_parallel * 100;

using myclock = std::chrono::steady_clock;

class coordinator
{
    bool finished_{};
    std::size_t remaining_{total};
    std::size_t outstanding{num_parallel};
    myclock::time_point tp_start;
    myclock::time_point tp_finish;
    mysql::connection_pool* pool{};

public:
    coordinator(mysql::connection_pool* pool = nullptr) : pool(pool) {}
    std::chrono::milliseconds ellapsed() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(tp_finish - tp_start);
    }
    void record_start() { tp_start = myclock::now(); }
    void on_finish()
    {
        if (--outstanding == 0)
        {
            tp_finish = myclock::now();
            if (pool)
                pool->cancel();
        }
    }
    bool on_loop_finish()
    {
        if (--remaining_ == 0)
            finished_ = true;
        return !finished_;
    }

    bool check_ec(error_code ec, const mysql::diagnostics& diag)
    {
        if (ec)
        {
            finished_ = true;
            std::cout << ec << ", " << diag.server_message() << std::endl;
            // exit(1);
        }
        return static_cast<bool>(ec);
    }
};

struct per_connection
{
    mysql::any_connection conn;
    mysql::results r;
    mysql::diagnostics diag;
    coordinator* coord{};
    const mysql::connect_params* params;

    void start()
    {
        conn.async_connect(params, diag, [this](error_code ec) {
            if (!coord->check_ec(ec, diag))
            {
                send_query();
            }
            else
            {
                coord->on_finish();
            }
        });
    }

    void send_query()
    {
        conn.async_execute("SELECT 1", r, diag, [this](error_code ec) {
            if (!coord->check_ec(ec, diag))
            {
                close();
            }
            else
            {
                coord->on_finish();
            }
        });
    }

    void close()
    {
        conn.async_close(diag, [this](error_code ec) {
            if (!coord->check_ec(ec, diag) && coord->on_loop_finish())
            {
                start();
            }
            else
            {
                coord->on_finish();
            }
        });
    }
};

struct per_connection_2
{
    mysql::connection_pool* pool;
    mysql::results r;
    mysql::diagnostics diag;
    coordinator* coord{};
    mysql::pooled_connection conn;

    void start()
    {
        pool->async_get_connection(diag, [this](error_code ec, mysql::pooled_connection c) {
            if (!coord->check_ec(ec, diag))
            {
                conn = std::move(c);
                stmt();
            }
            else
            {
                coord->on_finish();
            }
        });
    }

    void stmt()
    {
        conn->async_prepare_statement(
            "SELECT tax_id FROM company WHERE id = ?",
            diag,
            [this](error_code ec, boost::mysql::statement s) {
                if (!coord->check_ec(ec, diag))
                {
                    send_query(s);
                }
                else
                {
                    coord->on_finish();
                }
            }
        );
    }

    void send_query(boost::mysql::statement s)
    {
        conn->async_execute(s.bind("HGS"), r, diag, [this, s](error_code ec) {
            if (!coord->check_ec(ec, diag))
            {
                close_stmt(s);
            }
            else
            {
                coord->on_finish();
            }
        });
    }

    void close_stmt(boost::mysql::statement s)
    {
        conn->async_close_statement(s, diag, [this](error_code ec) {
            conn.return_to_pool();
            if (!coord->check_ec(ec, diag) && coord->on_loop_finish())
            {
                start();
            }
            else
            {
                coord->on_finish();
            }
        });
    }
};

void without_pool()
{
    // Setup
    asio::io_context ctx;
    mysql::connect_params params{
        mysql::any_address::make_tcp("localhost"),
        // mysql::any_address::make_unix("/var/run/mysqld/mysqld.sock"),
        "example_user",
        "example_password",
        "boost_mysql_examples",
    };
    // params.ssl = mysql::ssl_mode::disable;
    std::vector<per_connection> conns;
    coordinator coord;

    // Create connections
    for (std::size_t i = 0; i < num_parallel; ++i)
        conns.push_back(per_connection{ctx, {}, {}, &coord, &params});

    // Launch
    coord.record_start();
    for (auto& conn : conns)
        conn.start();

    // Run
    ctx.run();

    // Print ellapsed time
    std::cout << "Ellapsed: " << coord.ellapsed().count() << std::endl;
}

void with_pool()
{
    // Setup
    asio::io_context ctx;
    mysql::pool_params params{
        mysql::any_address::make_tcp("localhost"),
        // mysql::any_address::make_unix("/var/run/mysqld/mysqld.sock"),
        "example_user",
        "example_password",
        "boost_mysql_examples",
    };
    params.max_size = num_parallel;
    // params.enable_thread_safety = false;
    // params.ssl = mysql::ssl_mode::disable;

    mysql::connection_pool pool(ctx, std::move(params));
    pool.async_run(asio::detached);

    std::vector<per_connection_2> conns;
    coordinator coord(&pool);

    // Create connections
    for (std::size_t i = 0; i < num_parallel; ++i)
        conns.push_back(per_connection_2{&pool, {}, {}, &coord, {}});

    // Launch
    coord.record_start();
    for (auto& conn : conns)
        conn.start();

    // Run
    ctx.run();

    // Print ellapsed time
    std::cout << "Ellapsed: " << coord.ellapsed().count() << std::endl;
}

int main(int, char**) { with_pool(); }

//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/connection.hpp>
#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/pool_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/thread_pool.hpp>

#include <atomic>
#include <cstddef>
#include <iostream>

using boost::mysql::error_code;
namespace mysql = boost::mysql;
namespace asio = boost::asio;

namespace {

static constexpr int num_parallel = 100;
static constexpr int total = num_parallel * 20;

static void check_ec(error_code ec, mysql::diagnostics& diag)
{
    if (ec)
    {
        std::cerr << ec << ", " << diag.server_message() << std::endl;
        exit(1);
    }
}

class coordinator
{
    std::atomic_int remaining_queries_{total};
    std::atomic_int outstanding_tasks_{num_parallel};
    mysql::connection_pool* pool_{};

public:
    coordinator(mysql::connection_pool& pool) : pool_(&pool) {}
    void on_finish()
    {
        if (--outstanding_tasks_ == 0)
            pool_->cancel();
    }
    bool on_loop_finish() { return --remaining_queries_ > 0; }
};

class task
{
    enum class state_t
    {
        initial,
        after_get_connection,
        after_execute
    };

    mysql::connection_pool* pool_;
    mysql::results r_;
    mysql::diagnostics diag_;
    coordinator* coord_{};
    mysql::pooled_connection conn_;
    state_t state_{state_t::initial};
    asio::any_io_executor base_ex_;

public:
    task(mysql::connection_pool& pool, coordinator& coord, asio::any_io_executor base_ex)
        : pool_(&pool), coord_(&coord), base_ex_(std::move(base_ex))
    {
    }

    void resume(error_code ec = {})
    {
        // Error checking
        check_ec(ec, diag_);

        switch (state_)
        {
            while (true)
            {
            case state_t::initial:
                state_ = state_t::after_get_connection;
                // Verify that we achieve thread-safety even if the token
                // has a bound executor that is not a strand (regression check).
                pool_->async_get_connection(
                    diag_,
                    asio::bind_executor(
                        base_ex_,
                        [this](error_code ec, mysql::pooled_connection c) {
                            conn_ = std::move(c);
                            resume(ec);
                        }
                    )
                );
                return;

            case state_t::after_get_connection:

                state_ = state_t::after_execute;
                conn_->async_execute("SELECT 1", r_, diag_, [this](error_code ec) { resume(ec); });
                return;

            case state_t::after_execute:

                conn_ = boost::mysql::pooled_connection();

                if (!coord_->on_loop_finish())
                {
                    conn_ = mysql::pooled_connection();
                    coord_->on_finish();
                    return;
                }
            }
        }
    }
};

void run(const char* hostname)
{
    // Setup
    asio::thread_pool ctx(8);
    mysql::pool_params params;
    params.server_address.emplace_host_and_port(hostname);
    params.username = "integ_user";
    params.password = "integ_password";
    params.max_size = num_parallel - 10;    // create some contention
    params.ssl = mysql::ssl_mode::require;  // double check sharing SSL contexts is OK

    mysql::connection_pool pool(
        mysql::pool_executor_params::thread_safe(ctx.get_executor()),
        std::move(params)
    );

    // The pool should be thread-safe even if we pass a token with a custom
    // executor to async_run
    pool.async_run(asio::bind_executor(ctx.get_executor(), asio::detached));

    std::vector<task> conns;
    coordinator coord(pool);

    // Create connections
    for (std::size_t i = 0; i < num_parallel; ++i)
        conns.emplace_back(pool, coord, ctx.get_executor());

    // Launch
    for (auto& conn : conns)
        conn.resume(error_code());

    // Run
    ctx.join();
}

void usage(const char* progname)
{
    std::cerr << "Usage: " << progname << " <hostname>\n";
    exit(1);
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        usage(argv[0]);
    }

    run(argv[1]);
}

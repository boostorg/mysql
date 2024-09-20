//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/pool_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <iostream>

using boost::mysql::error_code;
namespace mysql = boost::mysql;
namespace asio = boost::asio;

namespace {

static constexpr int num_parallel = 100;
static constexpr int total = num_parallel * 4;

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
        after_enter_strand,
        after_get_connection,
        after_execute
    };

    mysql::connection_pool* pool_;
    mysql::results r_;
    mysql::diagnostics diag_;
    coordinator* coord_{};
    mysql::pooled_connection conn_;
    state_t state_{state_t::initial};
    asio::strand<asio::any_io_executor> strand_;
    std::chrono::milliseconds timeout_{1};  // make it likely to get some cancellations

public:
    task(mysql::connection_pool& pool, coordinator& coord, asio::any_io_executor base_ex)
        : pool_(&pool), coord_(&coord), strand_(asio::make_strand(std::move(base_ex)))
    {
    }

    void resume(error_code ec = {})
    {
        while (true)
        {
            switch (state_)
            {
            case state_t::initial:
                state_ = state_t::after_enter_strand;
                asio::dispatch(asio::bind_executor(strand_, [this]() { resume(error_code()); }));
                return;

            case state_t::after_enter_strand:
                state_ = state_t::after_get_connection;
                pool_->async_get_connection(
                    diag_,
                    asio::cancel_after(
                        timeout_,
                        asio::bind_executor(
                            strand_,
                            [this](error_code ec, mysql::pooled_connection c) {
                                conn_ = std::move(c);
                                resume(ec);
                            }
                        )
                    )
                );
                return;

            case state_t::after_get_connection:
                if (ec == boost::mysql::client_errc::cancelled)
                {
                    state_ = state_t::initial;
                    timeout_ *= 2;  // Make sure the test doesn't get stuck
                    break;
                }
                check_ec(ec, diag_);

                state_ = state_t::after_execute;
                conn_->async_execute(
                    "SELECT 1",
                    r_,
                    diag_,
                    asio::bind_executor(strand_, [this](error_code ec) { resume(ec); })
                );
                return;

            case state_t::after_execute:

                check_ec(ec, diag_);
                conn_ = boost::mysql::pooled_connection();

                if (coord_->on_loop_finish())
                {
                    state_ = state_t::after_enter_strand;
                }
                else
                {
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
    params.thread_safe = true;

    mysql::connection_pool pool(ctx, std::move(params));

    pool.async_run(asio::detached);

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

//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/pool_params.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

#include <iostream>
#include <memory>

using boost::mysql::error_code;
namespace mysql = boost::mysql;
namespace asio = boost::asio;

namespace {

void run(const char* hostname)
{
    // Setup
    asio::thread_pool ctx(8);
    mysql::pool_params params;
    params.server_address.emplace_host_and_port(hostname);
    params.username = "integ_user";
    params.password = "integ_password";
    params.initial_size = 20;
    params.thread_safe = true;

    for (int i = 0; i < 1000; ++i)
    {
        // Create a pool
        auto pool = std::make_shared<mysql::connection_pool>(ctx, std::move(params));

        // Run the pool within the thread pool
        asio::post(asio::bind_executor(ctx.get_executor(), [pool]() { pool->async_run(asio::detached); }));

        // Issue a cancellation
        asio::post(asio::bind_executor(ctx.get_executor(), [pool]() { pool->cancel(); }));
    }

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

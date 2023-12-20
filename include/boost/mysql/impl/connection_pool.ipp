//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_CONNECTION_POOL_IPP
#define BOOST_MYSQL_IMPL_CONNECTION_POOL_IPP

#pragma once

#include <boost/mysql/connection_pool.hpp>

#include <boost/mysql/detail/connection_pool_fwd.hpp>

#include <boost/mysql/impl/internal/connection_pool/connection_pool_impl.hpp>

void boost::mysql::detail::mark_as_collectable(
    boost::mysql::detail::connection_node& node,
    bool should_reset
) noexcept
{
    node.mark_as_collectable(should_reset);
}

boost::mysql::any_connection& boost::mysql::detail::get_connection(boost::mysql::detail::connection_node& node
) noexcept
{
    return node.connection();
}

boost::mysql::connection_pool::connection_pool(const pool_executor_params& ex_params, pool_params params)
    : impl_(std::make_shared<detail::pool_impl>(ex_params, std::move(params)))
{
}

boost::mysql::connection_pool::executor_type boost::mysql::connection_pool::get_executor() noexcept
{
    return impl_->get_executor();
}

void boost::mysql::connection_pool::async_run_erased(
    std::shared_ptr<detail::pool_impl> pool,
    asio::any_completion_handler<void(error_code)> handler
)
{
    pool->async_run(std::move(handler));
}

void boost::mysql::connection_pool::async_get_connection_erased(
    std::shared_ptr<detail::pool_impl> pool,
    std::chrono::steady_clock::duration timeout,
    diagnostics* diag,
    asio::any_completion_handler<void(error_code, pooled_connection)> handler
)
{
    pool->async_get_connection(timeout, diag, std::move(handler));
}

void boost::mysql::connection_pool::cancel()
{
    BOOST_ASSERT(valid());
    impl_->cancel();
}

#endif

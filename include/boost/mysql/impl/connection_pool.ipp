//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_CONNECTION_POOL_IPP
#define BOOST_MYSQL_IMPL_CONNECTION_POOL_IPP

#pragma once

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/connection_pool.hpp>

#include <boost/mysql/detail/any_resettable.hpp>
#include <boost/mysql/detail/connection_pool_fwd.hpp>

#include <boost/mysql/impl/internal/connection_pool/connection_pool_impl.hpp>

#include <boost/asio/any_io_executor.hpp>

#include <memory>

void boost::mysql::detail::return_connection(
    pool_impl& pool,
    connection_node& node,
    bool should_reset
) noexcept
{
    pool.return_connection(node, should_reset);
}

boost::mysql::any_connection& boost::mysql::detail::get_connection(
    boost::mysql::detail::connection_node& node
) noexcept
{
    return node.connection();
}

void* boost::mysql::detail::get_user_node(boost::mysql::detail::connection_node& node)
{
    return node.user_state().get();
}

std::shared_ptr<boost::mysql::detail::pool_impl> boost::mysql::detail::make_pool_impl(
    asio::any_io_executor ex,
    pool_params&& params,
    any_resettable (*state_factory)(any_connection&)
)
{
    return std::make_shared<detail::pool_impl>(std::move(ex), std::move(params), state_factory);
}

boost::asio::any_io_executor boost::mysql::detail::get_executor(boost::mysql::detail::pool_impl& pool)
{
    return pool.get_executor();
}

void boost::mysql::detail::async_run_erased(
    std::shared_ptr<pool_impl> pool,
    asio::any_completion_handler<void(error_code)> handler
)
{
    pool->async_run(std::move(handler));
}

void boost::mysql::detail::async_get_connection_erased(
    std::shared_ptr<detail::pool_impl> pool,
    diagnostics* diag,
    asio::any_completion_handler<void(error_code, connection_node*, std::shared_ptr<pool_impl>)> handler
)
{
    pool->async_get_connection(diag, std::move(handler));
}

void boost::mysql::detail::cancel(boost::mysql::detail::pool_impl& pool) { pool.cancel(); }

#endif

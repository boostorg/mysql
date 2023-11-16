//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_POOL_PARAMS_HPP
#define BOOST_MYSQL_POOL_PARAMS_HPP

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/defaults.hpp>
#include <boost/mysql/ssl_mode.hpp>

#include <boost/mysql/detail/access.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/optional/optional.hpp>

#include <chrono>
#include <cstddef>
#include <string>

namespace boost {
namespace mysql {

class pool_executor_params
{
    struct
    {
        asio::any_io_executor pool_ex;
        asio::any_io_executor conn_ex;
    } impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    pool_executor_params(asio::any_io_executor pool_ex, asio::any_io_executor conn_ex = {})
        : impl_{pool_ex, conn_ex ? std::move(conn_ex) : pool_ex}
    {
    }

    template <
        class ExecutionContext
#ifndef BOOST_MYSQL_DOXYGEN
        ,
        class = typename std::enable_if<std::is_convertible<
            decltype(std::declval<ExecutionContext&>().get_executor()),
            asio::any_io_executor>::value>::type
#endif
        >
    pool_executor_params(ExecutionContext& ctx) : pool_executor_params(ctx.get_executor())
    {
    }

    static pool_executor_params thread_safe(asio::any_io_executor ex)
    {
        return pool_executor_params(asio::make_strand(ex), ex);
    }
};

/**
 * \brief (EXPERIMENTAL) Configuration parameters for \ref connection_pool.
 * \details
 * This is an owning type.
 */
struct pool_params
{
    /**
     * \brief Determines how to establish a physical connection to the MySQL server.
     * \details
     * Connections created by the pool will use this address to connect to the
     * server. This can be either a host and port or a UNIX socket path.
     * Defaults to (localhost, 3306).
     */
    any_address server_address;

    /// User name that connections created by the pool should use to authenticate as.
    std::string username;

    /// Password that connections created by the pool should use.
    std::string password;

    /**
     * \brief Database name that connections created by the pool will use when connecting.
     * \details Leave it empty to select no database (this is the default).
     */
    std::string database;

    /**
     * \brief Controls whether connections created by the pool will use TLS or not.
     * \details
     * See \ref ssl_mode for more information about the possible modes.
     * This option is only relevant when `server_address.type() == address_type::host_and_port`.
     * UNIX socket connections will never use TLS, regardless of this value.
     */
    ssl_mode ssl{ssl_mode::require};

    /**
     * \brief Whether to enable support for semicolon-separated text queries for connections created by the
     * pool. \details Disabled by default.
     */
    bool multi_queries{false};

    /// Initial size (in bytes) of the internal read buffer for the connections created by the pool.
    std::size_t initial_read_buffer_size{default_initial_read_buffer_size};

    /**
     * \brief Initial number of connections to create.
     * \details
     * When \ref connection_pool::async_run starts running, this number of connections
     * will be created and connected.
     */
    std::size_t initial_size{1};

    /**
     * \brief Max number of connections to create.
     * \details
     * When a connection is requested, but all connections are in use, new connections
     * will be created and connected up to this size.
     * \n
     * Defaults to the maximum number of concurrent connections that MySQL
     * servers allow by default. If you increase this value, increase the server's
     * max number of connections, too (by setting the `max_connections` variable).
     * \n
     * This value must be `> 0` and `>= initial_size`.
     */
    std::size_t max_size{151};

    /**
     * \brief The SSL context to use for connections using TLS.
     * \details
     * If a non-empty value is provided, all connections created by the pool
     * will use the passed context when using TLS. This allows setting TLS options
     * to pool-created connections.
     * \n
     * If an empty value is passed (the default) and the connections require TLS,
     * an internal SSL context with suitable options will be created by the pool.
     */
    boost::optional<asio::ssl::context> ssl_ctx{};

    /**
     * \brief The timeout to use when connecting.
     * \details
     * Connections will be connected by the pool before being handed to the user
     * (as per \ref any_connection::async_connect).
     * If the operation takes longer than this timeout,
     * the operation will be interrupted and be considered as failed.
     * \n
     * Set this timeout to zero to disable it.
     * \n
     * This value must not be negative.
     */
    std::chrono::steady_clock::duration connect_timeout{std::chrono::seconds(20)};

    /**
     * \brief The interval between connect attempts.
     * \details
     * When session establishment fails, the operation will be retried until
     * success. This value determines the interval between consecutive connection
     * attempts.
     * \n
     * This value must be greater than zero.
     */
    std::chrono::steady_clock::duration retry_interval{std::chrono::seconds(30)};

    /**
     * \brief The health-check interval.
     * \details
     * If a connection becomes iddle and hasn't been handed to the user for
     * `ping_interval`, a health-check will be performed (as per \ref any_connection::async_ping).
     * Pings will be sent with a periodicity of `ping_interval` until the connection
     * is handed to the user, or a ping fails.
     * \n
     * Set this interval to zero to disable pings.
     * \n
     * This value must not be negative.
     */
    std::chrono::steady_clock::duration ping_interval{std::chrono::hours(1)};

    /**
     * \brief The timeout to use for pings and session resets.
     * \details
     * If pings (as per \ref any_connection::async_ping) or session resets
     * (as per \ref any_connection::async_reset_session) take longer than this
     * timeout, they will be cancelled, and the operation will be considered failed.
     * \n
     * Set this timeout to zero to disable it.
     * \n
     * This value must not be negative.
     */
    std::chrono::steady_clock::duration ping_timeout{std::chrono::seconds(10)};
};

}  // namespace mysql
}  // namespace boost

#endif

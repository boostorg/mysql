//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CONNECTION_POOL_HPP
#define BOOST_MYSQL_CONNECTION_POOL_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/pool_params.hpp>
#include <boost/mysql/pooled_connection.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/connection_pool/connection_pool_impl.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>

#include <chrono>
#include <memory>

namespace boost {
namespace mysql {

/**
 * \brief (EXPERIMENTAL) A pool of connections of variable size.
 * \details
 * A connection pool creates and manages \ref any_connection objects.
 * Using a pool allows to reuse sessions, avoiding part of the overhead associated
 * to session establishment. It also features built-in error handling and reconnection.
 * See the discussion and examples for more details on when to use this class.
 * \n
 * Connections are retrieved by \ref async_get_connection, which yields a
 * \ref pooled_connection object. They are returned to the pool when the
 * `pooled_connection` is destroyed, or by calling \ref pooled_connection::return_without_reset.
 * \n
 * A pool needs to be run before it can return any connection. Use \ref async_run for this.
 * Pools can only be run once.
 * \n
 * Connections are created, connected and managed internally by the pool, following
 * a well-defined state model. Please refer to the discussion for details.
 * \n
 * Due to oddities in Boost.Asio's universal async model, this class only
 * exposes async functions. You can use `asio::use_future` to transform them
 * into sync functions (see the examples for details).
 * \n
 * This is a move-only type.
 *
 * \par Thread-safety
 * By default, connection pools are *not* thread-safe, but most functions can
 * be made thread-safe by passing an adequate \ref pool_executor_params objects
 * to the constructor. See \ref pool_executor_params::thread_safe and the discussion
 * for details.
 * \n
 * Distinct objects: safe. \n
 * Shared objects: unsafe, unless passing adequate values to the constructor.
 *
 * \par Object lifetimes
 * Connection pool objects create an internal state object that is referenced
 * by other objects and operations (like \ref pooled_connection). This object
 * will be kept alive using shared ownership semantics even after the `connection_pool`
 * object is destroyed. This results in intuitive lifetime rules.
 *
 * \par Experimental
 * This part of the API is experimental, and may change in successive
 * releases without previous notice.
 */
class connection_pool
{
    std::shared_ptr<detail::connection_pool_impl> impl_;

    static constexpr std::chrono::steady_clock::duration get_default_timeout() noexcept
    {
        return std::chrono::seconds(30);
    }

public:
    /**
     * \brief Constructs a connection pool from an executor.
     * \details
     * Internal I/O objects (like timers and channels) are constructed using
     * \ref pool_executor_params::pool_executor on `ex_params`. Connections are constructed using
     * \ref pool_executor_params::connection_executor. This can be used to create
     * thread-safe pools.
     * \n
     * The pool is created in a "not-running" state. Call \ref async_run to transition to the
     * "running" state. Calling \ref async_get_connection in the "not-running" state will fail
     * with \ref client_errc::cancelled.
     * \n
     * The constructed pool is always valid (`this->valid() == true`).
     *
     * \par Exception safety
     * Strong guarantee. Exceptions may be thrown by memory allocations.
     * \throws `std::invalid_argument` If `params` contains values that violate the rules described in \ref
     *         pool_params.
     */
    connection_pool(const pool_executor_params& ex_params, pool_params params)
        : impl_(std::make_shared<detail::connection_pool_impl>(ex_params, std::move(params)))
    {
    }

#ifndef BOOST_MYSQL_DOXYGEN
    connection_pool(const connection_pool&) = delete;
    connection_pool& operator=(const connection_pool&) = delete;
#endif

    /**
     * \brief Move-constructor.
     * \details
     * Constructs a connection pool by taking ownership of `other`.
     * \n
     * After this function returns, if `other.valid() == true`, `this->valid() == true`.
     * In any case, `other` will become invalid (`other.valid() == false`).
     * \n
     * Moving a connection pool with outstanding async operations
     * is safe.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Thead-safety
     * This function is never thread-safe, regardless of the executor
     * configuration passed to the constructor. Calling this function
     * concurrently with any other function introduces data races.
     */
    connection_pool(connection_pool&& other) = default;

    /**
     * \brief Move assignment.
     * \details
     * Assigns `other` to `*this`, transferring ownership.
     * \n
     * After this function returns, if `other.valid() == true`, `this->valid() == true`.
     * In any case, `other` will become invalid (`other.valid() == false`).
     * \n
     * Moving a connection pool with outstanding async operations
     * is safe.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Thead-safety
     * This function is never thread-safe, regardless of the executor
     * configuration passed to the constructor. Calling this function
     * concurrently with any other function introduces data races.
     */
    connection_pool& operator=(connection_pool&& other) = default;

    /**
     * \brief Destructor.
     * \details
     * If `this->valid() == true`, the pool will be cancelled as per \ref cancel.
     */
    ~connection_pool() noexcept
    {
        if (valid())
            cancel();
    }

    /**
     * \brief Returns whether the object is in a moved-from state.
     * \details
     * This function returns always `true` except for pools that have been
     * moved-from. Moved-from objects don't represent valid pools. They can only
     * be assigned to or destroyed.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Thead-safety
     * This function is never thread-safe, regardless of the executor
     * configuration passed to the constructor. Calling this function
     * concurrently with any other function introduces data races.
     */
    bool valid() const noexcept { return impl_.get() != nullptr; }

    /// The executor type associated to this object.
    using executor_type = asio::any_io_executor;

    /**
     * \brief Retrieves the executor associated to this object.
     * \details
     * Returns the pool executor passed to the constructor, as per
     * \ref pool_executor_params::pool_executor.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Thead-safety
     * This function is never thread-safe, regardless of the executor
     * configuration passed to the constructor. Calling this function
     * concurrently with any other function introduces data races.
     */
    executor_type get_executor() noexcept { return impl_->get_executor(); }

    /**
     * \brief Runs the pool task in charge of managing connections.
     * \details
     * This function creates and connects new connections, and resets and pings
     * already created ones. You need to call this function for \ref async_get_connection
     * to succeed.
     * \n
     * The async operation will run indefinitely, until the pool is cancelled
     * (by being destroyed or calling \ref cancel). The operation completes once
     * all internal connection operations (including connects, pings and resets)
     * complete.
     * \n
     * It is safe to call this function after calling \ref cancel.
     *
     * \par Preconditions
     * This function can be called at most once for a single pool.
     * Formal precondition: `async_run` hasn't been called before on `*this` or any object
     * used to move-construct or move-assign `*this`.
     * \n
     * Additionally, `this->valid() == true`.
     *
     * \par Object lifetimes
     * While the operation is outstanding, the pool's internal data will be kept alive.
     * It is safe to destroy `*this` while the operation is outstanding.
     *
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`
     *
     * \par Errors
     * This function always complete successfully. The handler signature ensures
     * maximum compatibility with Boost.Asio infrastructure.
     *
     * \par Executor
     * All intermediate completion handlers are dispatched through the pool's
     * executor (as given by `this->get_executor()`).
     *
     * \par Thead-safety
     * When the pool is constructed with adequate executor configuration, this function
     * is safe to be called concurrently with \ref async_get_connection, \ref cancel,
     * `~pooled_connection` and \ref pooled_connection::return_without_reset.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_run(CompletionToken&& token)
    {
        BOOST_ASSERT(valid());
        return impl_->async_run(std::forward<CompletionToken>(token));
    }

    /**
     * \copydoc async_get_connection(std::chrono::steady_clock::duration,diagnostics&,CompletionToken&&)
     * \details
     * A timeout of 30 seconds will be used.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::pooled_connection))
            CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, pooled_connection))
    async_get_connection(CompletionToken&& token)
    {
        BOOST_ASSERT(valid());
        return impl_
            ->async_get_connection(get_default_timeout(), nullptr, std::forward<CompletionToken>(token));
    }

    /**
     * \copydoc async_get_connection(std::chrono::steady_clock::duration,diagnostics&,CompletionToken&&)
     * \details
     * A timeout of 30 seconds will be used.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::pooled_connection))
            CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, pooled_connection))
    async_get_connection(diagnostics& diag, CompletionToken&& token)
    {
        BOOST_ASSERT(valid());
        return impl_
            ->async_get_connection(get_default_timeout(), &diag, std::forward<CompletionToken>(token));
    }

    /// \copydoc async_get_connection(std::chrono::steady_clock::duration,diagnostics&,CompletionToken&&)
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::pooled_connection))
            CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, pooled_connection))
    async_get_connection(std::chrono::steady_clock::duration timeout, CompletionToken&& token)
    {
        BOOST_ASSERT(valid());
        return impl_->async_get_connection(timeout, nullptr, std::forward<CompletionToken>(token));
    }

    /**
     * \brief Retrieves a connection from the pool.
     * \details
     * Retrieves an iddle connection from the pool to be used.
     * \n
     * If this function completes successfully (empty error code), the return \ref pooled_connection
     * will have `valid() == true` and will be usable. If it completes with a non-empty error code,
     * it will have `valid() == false`.
     * \n
     * The returned connection is *not* thread-safe, even if the pool has been configured
     * with thread-safety enabled.
     * \n
     * If a connection is iddle when the operation is started, it will complete immediately
     * with such connection. Otherwise, it will wait for a connection to become iddle
     * (possibly creating one in the process, if pool configuration allows it), up to
     * a duration of `timeout`. A zero timeout disables it.
     * \n
     * If a timeout happens because connection establishment has failed, appropriate
     * diagnostics will be returned.
     *
     * \par Preconditions
     * `this->valid() == true` \n
     * `timeout.count() >= 0` (timeout values must be positive).
     *
     * \par Object lifetimes
     * While the operation is outstanding, the pool's internal data will be kept alive.
     * It is safe to destroy `*this` while the operation is outstanding.
     *
     * \par Handler signature
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::pooled_connection)`
     *
     * \par Errors
     * \li Any error returned by \ref any_connection::async_connect, if a timeout
     *     happens because connection establishment failed.
     * \li \ref client_errc::timeout, if a timeout happens for any other reason
     *     (e.g. all connections are in use and limits forbid creating more).
     * \li \ref client_errc::cancelled if \ref cancel was called before or while
     *     the operation is outstanding, or if the pool is not running.
     *
     * \par Executor
     * All intermediate completion handlers are dispatched through the pool's
     * executor (as given by `this->get_executor()`).
     *
     * \par Thead-safety
     * When the pool is constructed with adequate executor configuration, this function
     * is safe to be called concurrently with \ref async_run, \ref cancel,
     * `~pooled_connection` and \ref pooled_connection::return_without_reset.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::pooled_connection))
            CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, pooled_connection))
    async_get_connection(
        std::chrono::steady_clock::duration timeout,
        diagnostics& diag,
        CompletionToken&& token
    )
    {
        BOOST_ASSERT(valid());
        return impl_->async_get_connection(timeout, &diag, std::forward<CompletionToken>(token));
    }

    /**
     * \brief Stops any current outstanding operation and marks the pool as cancelled.
     * \details
     * This function has the following effects:
     * \n
     * \li Stops the currently outstanding \ref async_run operation, if any, which will complete
     *     with a success error code.
     * \li Cancels any outstanding \ref async_get_connection operations, which will complete with
     *     \ref client_errc::cancelled.
     * \li Marks the pool as cancelled. Successive `async_get_connection` calls will complete
     *     immediately with \ref client_errc::cancelled.
     * \n
     * This function will return immediately, without waiting for the cancelled operations to complete.
     * \n
     * You may call this function any number of times. Successive calls will have no effect.
     *
     * \par Preconditions
     * `this->valid() == true`
     *
     * \par Exception safety
     * Basic guarantee. Memory allocations and acquiring mutexes may throw.
     *
     * \par Thead-safety
     * When the pool is constructed with adequate executor configuration, this function
     * is safe to be called concurrently with \ref async_run, \ref async_get_connection,
     * `~pooled_connection` and \ref pooled_connection::return_without_reset.
     */
    void cancel()
    {
        BOOST_ASSERT(valid());
        impl_->cancel();
    }
};

}  // namespace mysql
}  // namespace boost

#endif

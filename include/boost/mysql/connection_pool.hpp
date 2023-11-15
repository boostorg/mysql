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
 * Thead-safety is determined at runtime by the value of
 * \ref pool_params::enable_thread_safety. By default, thread-safety is enabled.
 * \n
 * Distinct objects: always safe, regardless of \ref pool_params::enable_thread_safety. \n
 * Shared objects: if constructed from a `params` object with `params.enable_thread_safety == true`,
 *    safe. Otherwise, unsafe.
 *
 * \par Object lifetimes
 * Connection pool objects create an internal state object that is referenced
 * by other objects and operations (like \ref pooled_connection). This object
 * will be kept alive using shared ownership semantics even after the `connection_pool`
 * object is destroyed. This results in intuitive lifetime rules.
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
     * The passed executor is used to create all \ref any_connection objects,
     * and any other internally required I/O object. If `params.enable_thread_safety == true`,
     * an internal `asio::strand` object will be created, wrapping `ex`, for ensuring thread
     * safety. The strand is only used internally, and *not* by the returned connection ojects.
     * \n
     * The pool is created in a "not-running" state. Call \ref async_run to transition to the
     * "running" state. Calling \ref async_get_connection in the "not-running" state will fail
     * with `asio::error::operation_aborted`.
     * \n
     * The constructed pool is always valid (`this->valid() == true`).
     *
     * \par Exception safety
     * Strong guarantee. Exceptions may be thrown by memory allocations.
     * \throws `std::invalid_argument` If `params` contains values that violate the rules described in \ref
     *         pool_params.
     */
    connection_pool(asio::any_io_executor ex, pool_params params)
        : impl_(std::make_shared<detail::connection_pool_impl>(std::move(params), std::move(ex)))
    {
    }

    /**
     * \brief Constructs a connection pool from an execution context.
     * \details
     * Equivalent to `connection_pool(ctx.get_executor(), std::move(params))`.
     * \n
     * This function participates in overload resolution only if `ExecutionContext`
     * satisfies the `ExecutionContext` requirements imposed by Boost.Asio.
     *
     * \copydetail connection_pool::connection_pool
     */
    template <
        class ExecutionContext
#ifndef BOOST_MYSQL_DOXYGEN
        ,
        class = typename std::enable_if<std::is_convertible<
            decltype(std::declval<ExecutionContext&>().get_executor()),
            asio::any_io_executor>::value>::type
#endif
        >
    connection_pool(ExecutionContext& ctx, pool_params params)
        : connection_pool(ctx.get_executor(), std::move(params))
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
     */
    bool valid() const noexcept { return impl_.get() != nullptr; }

    /// The executor type associated to this object.
    using executor_type = asio::any_io_executor;

    /**
     * \brief Retrieves the executor associated to this object.
     * \details
     * If the pool has been configured with thread-safety enabled, this function
     * returns the original executor passed to the object, instead of the internal
     * strand.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    executor_type get_executor() noexcept { return impl_->get_executor(); }

    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_run(CompletionToken&& token)
    {
        BOOST_ASSERT(valid());
        return impl_->async_run(std::forward<CompletionToken>(token));
    }

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

    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::pooled_connection))
            CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, pooled_connection))
    async_get_connection(std::chrono::steady_clock::duration timeout, CompletionToken&& token)
    {
        BOOST_ASSERT(valid());
        return impl_->async_get_connection(timeout, nullptr, std::forward<CompletionToken>(token));
    }

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

    void cancel()
    {
        BOOST_ASSERT(valid());
        impl_->cancel();
    }
};

}  // namespace mysql
}  // namespace boost

#endif

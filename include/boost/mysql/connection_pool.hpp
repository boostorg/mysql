//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CONNECTION_POOL_HPP
#define BOOST_MYSQL_CONNECTION_POOL_HPP

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/handshake_params.hpp>

#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/connection_pool_helpers.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>

#include <chrono>
#include <memory>

namespace boost {
namespace mysql {

class connection_pool;

// TODO: review this
struct pool_params
{
    any_address_view server_address;
    handshake_params handshake_params;
    std::size_t initial_size{1};
    std::size_t max_size{150};  // TODO: is this MySQL's max by default?
    bool enable_thread_safety{true};

    std::chrono::steady_clock::duration connect_timeout{std::chrono::seconds(20)};
    std::chrono::steady_clock::duration ping_timeout{std::chrono::seconds(5)};
    std::chrono::steady_clock::duration reset_timeout{std::chrono::seconds(10)};
    std::chrono::steady_clock::duration retry_interval{std::chrono::seconds(10)};
    std::chrono::steady_clock::duration ping_interval{std::chrono::hours(1)};
    // TODO: SSL
};

class pooled_connection
{
    friend class connection_pool;

    BOOST_MYSQL_DECL
    const any_connection* const_ptr() const noexcept;

    any_connection* ptr() noexcept { return const_cast<any_connection*>(const_ptr()); }

    pooled_connection(detail::connection_node& node) noexcept : impl_(&node) {}

    std::unique_ptr<detail::connection_node, detail::connection_node_deleter> impl_;

public:
    pooled_connection() noexcept = default;
    pooled_connection(const pooled_connection&) = delete;
    pooled_connection(pooled_connection&&) = default;
    pooled_connection& operator=(const pooled_connection&) = delete;
    pooled_connection& operator=(pooled_connection&&) = default;
    ~pooled_connection() = default;

    bool valid() const noexcept { return impl_.get() != nullptr; }

    any_connection& get() noexcept { return *ptr(); }
    const any_connection& get() const noexcept { return *const_ptr(); }
    any_connection* operator->() noexcept { return ptr(); }
    const any_connection* operator->() const noexcept { return const_ptr(); }
};

class connection_pool
{
    std::unique_ptr<detail::connection_pool_impl> impl_;

    BOOST_MYSQL_DECL
    static void async_run_impl(
        detail::connection_pool_impl& impl,
        asio::any_completion_handler<void(error_code)> handler
    );

    struct initiate_run
    {
        template <class Handler>
        void operator()(Handler&& handler, detail::connection_pool_impl* impl) const
        {
            async_run_impl(*impl, std::forward<Handler>(handler));
        }
    };

    BOOST_MYSQL_DECL
    static void async_get_connection_impl(
        detail::connection_pool_impl& impl,
        std::chrono::steady_clock::duration timeout,
        diagnostics* diag,
        asio::any_completion_handler<void(error_code, pooled_connection)> handler
    );

    struct initiate_get_connection
    {
        template <class Handler>
        void operator()(
            Handler&& handler,
            detail::connection_pool_impl* impl,
            diagnostics* diag,
            std::chrono::steady_clock::duration timeout
        ) const
        {
            async_get_connection_impl(*impl, timeout, diag, std::forward<Handler>(handler));
        }
    };

    template <class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, pooled_connection))
    async_get_connection(
        std::chrono::steady_clock::duration timeout,
        diagnostics* diag,
        CompletionToken&& token
    )
    {
        BOOST_ASSERT(valid());
        return asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_get_connection(),
            token,
            this,
            diag,
            timeout
        );
    }

    static constexpr std::chrono::steady_clock::duration get_default_timeout() noexcept
    {
        return std::chrono::seconds(30);
    }

public:
    BOOST_MYSQL_DECL
    connection_pool(asio::any_io_executor ex, const pool_params& params);

    BOOST_MYSQL_DECL
    connection_pool(connection_pool&& rhs) noexcept;

    BOOST_MYSQL_DECL
    connection_pool& operator=(connection_pool&&) noexcept;

#ifndef BOOST_MYSQL_DOXYGEN
    connection_pool(const connection_pool&) = delete;
    connection_pool& operator=(const connection_pool&) = delete;
#endif

    BOOST_MYSQL_DECL
    ~connection_pool();

    bool valid() const noexcept { return impl_.get() != nullptr; }

    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_run(CompletionToken&& token)
    {
        BOOST_ASSERT(valid());
        return asio::async_initiate<CompletionToken, void(error_code)>(initiate_run(), token, this);
    }

    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::pooled_connection))
            CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, pooled_connection))
    async_get_connection(CompletionToken&& token)
    {
        return async_get_connection(get_default_timeout(), nullptr, std::forward<CompletionToken>(token));
    }

    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::pooled_connection))
            CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, pooled_connection))
    async_get_connection(diagnostics& diag, CompletionToken&& token)
    {
        return async_get_connection(get_default_timeout(), &diag, std::forward<CompletionToken>(token));
    }

    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::pooled_connection))
            CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, pooled_connection))
    async_get_connection(std::chrono::steady_clock::duration timeout, CompletionToken&& token)
    {
        return async_get_connection(timeout, nullptr, std::forward<CompletionToken>(token));
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
        return async_get_connection(timeout, &diag, std::forward<CompletionToken>(token));
    }

    void return_connection(pooled_connection&& conn, bool should_reset = true) noexcept
    {
        BOOST_ASSERT(valid());
        if (conn.valid())
            detail::return_connection(*conn.impl_.release(), should_reset);
    }

    BOOST_MYSQL_DECL
    void cancel();
};

}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/connection_pool.ipp>
#endif

#endif

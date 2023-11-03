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

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/connection_pool/connection_pool_impl.hpp>

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
    handshake_params hparams;
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

class connection_pool
{
    std::unique_ptr<detail::connection_pool_impl> impl_;

    static constexpr std::chrono::steady_clock::duration get_default_timeout() noexcept
    {
        return std::chrono::seconds(30);
    }

public:
    BOOST_MYSQL_DECL
    connection_pool(asio::any_io_executor ex, const pool_params& params);

#ifndef BOOST_MYSQL_DOXYGEN
    connection_pool(const connection_pool&) = delete;
    connection_pool& operator=(const connection_pool&) = delete;
#endif

    connection_pool(connection_pool&& rhs) = default;
    connection_pool& operator=(connection_pool&&) = default;
    ~connection_pool() = default;

    bool valid() const noexcept { return impl_.get() != nullptr; }

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

    void return_connection(pooled_connection&& conn, bool should_reset = true) noexcept
    {
        BOOST_ASSERT(valid());
        if (conn.valid())
            detail::return_connection(*conn.impl_.release(), should_reset);
    }

    void cancel()
    {
        BOOST_ASSERT(valid());
        impl_->cancel();
    }
};

}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/connection_pool.ipp>
#endif

#endif

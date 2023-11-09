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

class connection_pool
{
    std::shared_ptr<detail::connection_pool_impl> impl_;

    static constexpr std::chrono::steady_clock::duration get_default_timeout() noexcept
    {
        return std::chrono::seconds(30);
    }

public:
    connection_pool(asio::any_io_executor ex, pool_params params)
        : impl_(std::make_shared<detail::connection_pool_impl>(std::move(params), std::move(ex)))
    {
    }

    template <
        class ExecutionContext,
        class = typename std::enable_if<std::is_constructible<
            asio::any_io_executor,
            decltype(std::declval<ExecutionContext&>().get_executor())>::value>::type>
    connection_pool(ExecutionContext& ctx, pool_params params)
        : connection_pool(ctx.get_executor(), std::move(params))
    {
    }

#ifndef BOOST_MYSQL_DOXYGEN
    connection_pool(const connection_pool&) = delete;
    connection_pool& operator=(const connection_pool&) = delete;
#endif

    connection_pool(connection_pool&& rhs) = default;
    connection_pool& operator=(connection_pool&&) = default;
    ~connection_pool() noexcept
    {
        if (valid())
            cancel();
    }

    using executor_type = asio::any_io_executor;

    executor_type get_executor() { return impl_->get_executor(); }

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

    void cancel()
    {
        BOOST_ASSERT(valid());
        impl_->cancel();
    }
};

}  // namespace mysql
}  // namespace boost

#endif

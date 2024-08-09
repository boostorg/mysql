//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_WITH_DIAGNOSTICS_HPP
#define BOOST_MYSQL_IMPL_WITH_DIAGNOSTICS_HPP

#pragma once

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/with_diagnostics.hpp>

#include <boost/mysql/detail/async_helpers.hpp>

#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>

#include <cstddef>
#include <exception>
#include <memory>
#include <tuple>
#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

struct with_diag_handler_fn
{
    template <class Handler, class... Args>
    void operator()(Handler&& handler, error_code ec, Args&&... args)
    {
        std::exception_ptr exc = ec ? std::make_exception_ptr(error_with_diagnostics(ec, diag))
                                    : std::exception_ptr();
        owning_diag.reset();
        std::move(handler)(std::move(exc), std::forward<Args>(args)...);
    }

    // The diagnostics to use, taken from initiation
    const diagnostics& diag;

    // Keep alive any allocated diagnostics
    std::shared_ptr<diagnostics> owning_diag;
};

template <typename Signature>
struct with_diag_signature;

template <typename R, typename... Args>
struct with_diag_signature<R(error_code, Args...)>
{
    using type = R(std::exception_ptr, Args...);
};

template <typename R, typename... Args>
struct with_diag_signature<R(error_code, Args...)&>
{
    using type = R(std::exception_ptr, Args...) &;
};

template <typename R, typename... Args>
struct with_diag_signature<R(error_code, Args...) &&>
{
    using type = R(std::exception_ptr, Args...) &&;
};

#if defined(BOOST_ASIO_HAS_NOEXCEPT_FUNCTION_TYPE)

template <typename R, typename... Args>
struct with_diag_signature<R(error_code, Args...) noexcept>
{
    using type = R(std::exception_ptr, Args...) noexcept;
};

template <typename R, typename... Args>
struct with_diag_signature<R(error_code, Args...) & noexcept>
{
    using type = R(std::exception_ptr, Args...) & noexcept;
};

template <typename R, typename... Args>
struct with_diag_signature<R(error_code, Args...) && noexcept>
{
    using type = R(std::exception_ptr, Args...) && noexcept;
};

#endif  // defined(BOOST_ASIO_HAS_NOEXCEPT_FUNCTION_TYPE)

struct with_diag_init
{
    // We pass the inner token's initiation as 1st arg
    template <class Handler, class Initiation, class... Args>
    void operator()(Handler&& handler, Initiation&& init, Args&&... args) const
    {
        // Find the diagnostics object in the list of arguments
        using types = mp11::mp_list<typename std::decay<Args>::type...>;
        constexpr std::size_t pos = mp11::mp_find<types, diagnostics*>::value;

        // If you're getting an error here, it's because you're trying to use
        // with_diagnostics with an async function unrelated to Boost.MySQL.
        static_assert(
            pos < mp11::mp_size<types>::value,
            "with_diagnostics only works with Boost.MySQL async functions"
        );

        // Actually get the object
        diagnostics*& diag = std::get<pos>(std::tuple<Args&...>{args...});

        // Some functions (e.g. connection_pool) may pass nullptr as diag.
        // When using this token, allocate a diagnostics instance and overwrite the passed value
        std::shared_ptr<diagnostics> owning_diag;
        if (!diag)
        {
            // The allocator to use
            auto base_alloc = asio::get_associated_allocator(handler);
            using alloc_type = typename std::allocator_traits<decltype(base_alloc
            )>::template rebind_alloc<diagnostics>;

            owning_diag = std::allocate_shared<diagnostics>(alloc_type{std::move(base_alloc)});
            diag = owning_diag.get();
        }

        // Actually initiate
        std::forward<Initiation>(init)(
            make_intermediate_handler(
                with_diag_handler_fn{*diag, std::move(owning_diag)},
                std::forward<Handler>(handler)
            ),
            std::forward<Args>(args)...
        );
    }
};

}  // namespace detail
}  // namespace mysql

namespace asio {

template <typename CompletionToken, typename... Signatures>
struct async_result<mysql::with_diagnostics_t<CompletionToken>, Signatures...>
    : async_result<CompletionToken, typename mysql::detail::with_diag_signature<Signatures>::type...>
{
    template <class RawCompletionToken>
    using maybe_const_token_t = typename std::conditional<
        std::is_const<typename std::remove_reference<RawCompletionToken>::type>::value,
        const CompletionToken,
        CompletionToken>::type;

    template <typename Initiation, typename RawCompletionToken, typename... Args>
    static auto initiate(Initiation&& initiation, RawCompletionToken&& token, Args&&... args)
        -> decltype(async_initiate<
                    maybe_const_token_t<RawCompletionToken>,
                    typename mysql::detail::with_diag_signature<Signatures>::type...>(
            mysql::detail::with_diag_init{},
            mysql::detail::access::get_impl(token),
            std::forward<Initiation>(initiation),
            std::forward<Args>(args)...
        ))
    {
        return async_initiate<
            maybe_const_token_t<RawCompletionToken>,
            typename mysql::detail::with_diag_signature<Signatures>::type...>(
            mysql::detail::with_diag_init{},
            mysql::detail::access::get_impl(token),
            std::forward<Initiation>(initiation),
            std::forward<Args>(args)...
        );
    }
};

}  // namespace asio
}  // namespace boost

#endif

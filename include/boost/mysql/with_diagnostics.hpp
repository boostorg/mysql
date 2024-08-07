//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_WITH_DIAGNOSTICS_HPP
#define BOOST_MYSQL_WITH_DIAGNOSTICS_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>

#include <boost/asio/async_result.hpp>
#include <boost/config.hpp>

#include <exception>
#include <type_traits>
#include <utility>

namespace boost {
namespace mysql {

template <class CompletionToken>
struct with_diagnostics_t
{
    CompletionToken token_;
    // TODO: hide this
};

template <class CompletionToken>
constexpr with_diagnostics_t<typename std::decay<CompletionToken>::type> with_diagnostics(
    CompletionToken&& token
)
{
    return {std::forward<CompletionToken>(token)};
}

}  // namespace mysql
}  // namespace boost

// TODO: move away
namespace boost {
namespace mysql {
namespace detail {

template <class Handler>
struct with_diag_handler
{
    template <class... Args>
    void operator()(error_code ec, Args&&... args)
    {
        std::exception_ptr exc = ec ? std::make_exception_ptr(error_with_diagnostics(ec, diag))
                                    : std::exception_ptr();
        std::move(handler)(std::move(exc), std::forward<Args>(args)...);
    }

    Handler handler;
    const diagnostics& diag;
};

template <class Handler>
with_diag_handler<typename std::decay<Handler>::type> make_with_diag_handler(
    Handler&& handler,
    const diagnostics& diag
)
{
    return {std::forward<Handler>(handler), diag};
}

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

template <typename Initiation>
struct with_diag_init
{
    Initiation init;
    const diagnostics& diag;

    template <typename Handler, typename... Args>
    void operator()(Handler&& handler, Args&&... args) &&
    {
        std::move(init)(
            make_with_diag_handler(std::forward<Handler>(handler), diag),
            std::forward<Args>(args)...
        );
    }

    template <typename Handler, typename... Args>
    void operator()(Handler&& handler, Args&&... args) const&
    {
        init(make_with_diag_handler(std::forward<Handler>(handler), diag), std::forward<Args>(args)...);
    }
};

}  // namespace detail
}  // namespace mysql

namespace asio {

template <typename CompletionToken, typename... Signatures>
struct async_result<mysql::with_diagnostics_t<CompletionToken>, Signatures...>
    : async_result<CompletionToken, typename mysql::detail::with_diag_signature<Signatures>::type...>
{
    template <typename Initiation, typename RawCompletionToken, typename... Args>
    static auto do_initiate(
        Initiation&& initiation,
        RawCompletionToken&& token,
        mysql::diagnostics* diag,
        Args&&... args
    )
        -> decltype(async_initiate<
                    conditional_t<
                        is_const<remove_reference_t<RawCompletionToken>>::value,
                        const CompletionToken,
                        CompletionToken>,
                    typename mysql::detail::with_diag_signature<Signatures>::type...>(
            std::declval<mysql::detail::with_diag_init<typename std::decay<Initiation>::type>>(),
            token.token_,
            std::move(diag),
            std::move(args)...
        ))
    {
        return async_initiate<
            conditional_t<
                is_const<remove_reference_t<RawCompletionToken>>::value,
                const CompletionToken,
                CompletionToken>,
            typename mysql::detail::with_diag_signature<Signatures>::type...>(
            mysql::detail::with_diag_init<typename std::decay<Initiation>::type>{
                std::forward<Initiation>(initiation),
                *diag
            },
            token.token_,
            std::move(diag),
            std::move(args)...
        );
    }

    template <typename Initiation, typename RawCompletionToken, typename... Args>
    static auto initiate(Initiation&& initiation, RawCompletionToken&& token, Args&&... args)
        -> decltype(async_initiate<
                    conditional_t<
                        is_const<remove_reference_t<RawCompletionToken>>::value,
                        const CompletionToken,
                        CompletionToken>,
                    typename mysql::detail::with_diag_signature<Signatures>::type...>(
            std::declval<mysql::detail::with_diag_init<typename std::decay<Initiation>::type>>(),
            token.token_,
            std::move(args)...
        ))
    {
        return do_initiate(
            std::forward<Initiation>(initiation),
            std::forward<RawCompletionToken>(token),
            std::move(args)...
        );
    }
};

template <template <typename, typename> class Associator, typename Handler, typename DefaultCandidate>
struct associator<Associator, mysql::detail::with_diag_handler<Handler>, DefaultCandidate>
    : Associator<Handler, DefaultCandidate>
{
    static typename Associator<Handler, DefaultCandidate>::type get(
        const mysql::detail::with_diag_handler<Handler>& h
    ) noexcept
    {
        return Associator<Handler, DefaultCandidate>::get(h.handler);
    }

    static auto get(const mysql::detail::with_diag_handler<Handler>& h, const DefaultCandidate& c) noexcept
        -> decltype(Associator<Handler, DefaultCandidate>::get(h.handler, c))
    {
        return Associator<Handler, DefaultCandidate>::get(h.handler, c);
    }
};

}  // namespace asio
}  // namespace boost

#endif

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
#include <memory>
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
        owning_diag.reset();
        std::move(handler)(std::move(exc), std::forward<Args>(args)...);
    }

    Handler handler;
    std::unique_ptr<diagnostics> owning_diag;
    const diagnostics& diag;
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

template <class Initiation, class Handler, class... Args>
void do_initiate_with_diag(Initiation&& init, Handler&& handler, diagnostics* diag, Args&&... args)
{
    using handler_type = with_diag_handler<typename std::decay<Handler>::type>;
    std::unique_ptr<diagnostics> owning_diag{diag ? nullptr : new diagnostics};  // TODO: use allocator
    if (!diag)
        diag = owning_diag.get();
    std::forward<Initiation>(init)(
        handler_type{std::forward<Handler>(handler), std::move(owning_diag), *diag},
        diag,
        std::forward<Args>(args)...
    );
}

template <typename Initiation>
struct with_diag_init
{
    Initiation init;

    template <typename Handler, typename... Args>
    void operator()(Handler&& handler, diagnostics* diag, Args&&... args) &&
    {
        do_initiate_with_diag(
            std::move(init),
            std::forward<Handler>(handler),
            diag,
            std::forward<Args>(args)...
        );
    }

    template <typename Handler, typename... Args>
    void operator()(Handler&& handler, diagnostics* diag, Args&&... args) const&
    {
        do_initiate_with_diag(init, std::forward<Handler>(handler), diag, std::forward<Args>(args)...);
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
            mysql::detail::with_diag_init<typename std::decay<Initiation>::type>{
                std::forward<Initiation>(initiation),
            },
            token.token_,
            std::forward<Args>(args)...
        ))
    {
        return async_initiate<
            maybe_const_token_t<RawCompletionToken>,
            typename mysql::detail::with_diag_signature<Signatures>::type...>(
            mysql::detail::with_diag_init<typename std::decay<Initiation>::type>{
                std::forward<Initiation>(initiation),
            },
            token.token_,
            std::forward<Args>(args)...
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

//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_ASYNC_HELPERS_HPP
#define BOOST_MYSQL_DETAIL_ASYNC_HELPERS_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/associator.hpp>

namespace boost {
namespace mysql {
namespace detail {

// Base class for initiation objects. Includes a bound executor, so they're compatible
// with asio::cancel_after and similar
struct initiation_base
{
    asio::any_io_executor ex;

    initiation_base(asio::any_io_executor ex) noexcept : ex(std::move(ex)) {}

    using executor_type = asio::any_io_executor;
    const executor_type& get_executor() const noexcept { return ex; }
};

// An intermediate handler that propagates associated characteristics.
// HandlerFn must be a function void(Handler&&, args...) that calls the handler
template <class HandlerFn, class FinalHandler>
struct intermediate_handler
{
    HandlerFn fn;
    FinalHandler handler;

    template <class... Args>
    void operator()(Args&&... args)
    {
        fn(std::move(handler), std::forward<Args>(args)...);
    }
};

template <class HandlerFn, class FinalHandler>
intermediate_handler<typename std::decay<HandlerFn>::type, typename std::decay<FinalHandler>::type> make_intermediate_handler(
    HandlerFn&& fn,
    FinalHandler&& handler
)
{
    return {std::forward<HandlerFn>(fn), std::forward<FinalHandler>(handler)};
}

}  // namespace detail
}  // namespace mysql

namespace asio {

template <
    template <typename, typename>
    class Associator,
    class HandlerFn,
    class FinalHandler,
    typename DefaultCandidate>
struct associator<Associator, mysql::detail::intermediate_handler<HandlerFn, FinalHandler>, DefaultCandidate>
    : Associator<FinalHandler, DefaultCandidate>
{
    static typename Associator<FinalHandler, DefaultCandidate>::type get(
        const mysql::detail::intermediate_handler<HandlerFn, FinalHandler>& h
    ) noexcept
    {
        return Associator<FinalHandler, DefaultCandidate>::get(h.handler);
    }

    static auto get(
        const mysql::detail::intermediate_handler<HandlerFn, FinalHandler>& h,
        const DefaultCandidate& c
    ) noexcept -> decltype(Associator<FinalHandler, DefaultCandidate>::get(h.handler, c))
    {
        return Associator<FinalHandler, DefaultCandidate>::get(h.handler, c);
    }
};

}  // namespace asio
}  // namespace boost

#endif
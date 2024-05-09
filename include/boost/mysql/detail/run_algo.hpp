//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_RUN_ALGO_HPP
#define BOOST_MYSQL_DETAIL_RUN_ALGO_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/connection_state_api.hpp>
#include <boost/mysql/detail/engine.hpp>

#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

// Intermediate handler
template <class AlgoParams, class Handler>
struct generic_algo_handler
{
    static_assert(!has_void_result<AlgoParams>::value, "Internal error: result_type should be non-void");

    using result_t = typename AlgoParams::result_type;

    template <class DeducedHandler>
    generic_algo_handler(DeducedHandler&& h, connection_state& st)
        : final_handler(std::forward<DeducedHandler>(h)), st(&st)
    {
    }

    void operator()(error_code ec)
    {
        std::move(final_handler)(ec, ec ? result_t{} : get_result<AlgoParams>(*st));
    }

    Handler final_handler;  // needs to be accessed by associator
    connection_state* st;
};

// sync
template <class AlgoParams>
typename AlgoParams::result_type run_algo_impl(
    engine& eng,
    connection_state& st,
    AlgoParams params,
    error_code& ec,
    std::true_type /* has_void_result */
)
{
    eng.run(setup(st, params), ec);
}

template <class AlgoParams>
typename AlgoParams::result_type run_algo_impl(
    engine& eng,
    connection_state& st,
    AlgoParams params,
    error_code& ec,
    std::false_type /* has_void_result */
)
{
    eng.run(setup(st, params), ec);
    return get_result<AlgoParams>(st);
}

// async
template <class AlgoParams, class Handler>
void async_run_algo_impl(
    engine& eng,
    connection_state& st,
    AlgoParams params,
    Handler&& handler,
    std::true_type /* has_void_result */
)
{
    eng.async_run(setup(st, params), std::forward<Handler>(handler));
}

template <class AlgoParams, class Handler>
void async_run_algo_impl(
    engine& eng,
    connection_state& st,
    AlgoParams params,
    Handler&& handler,
    std::false_type /* has_void_result */
)
{
    using intermediate_handler_t = generic_algo_handler<AlgoParams, typename std::decay<Handler>::type>;
    eng.async_run(setup(st, params), intermediate_handler_t(std::forward<Handler>(handler), st));
}

// Public API
template <class AlgoParams>
typename AlgoParams::result_type run_algo(
    engine& eng,
    connection_state& st,
    AlgoParams params,
    error_code& ec
)
{
    return run_algo_impl(eng, st, params, ec, has_void_result<AlgoParams>{});
}

template <class AlgoParams, class Handler>
void async_run_algo(engine& eng, connection_state& st, AlgoParams params, Handler&& handler)
{
    async_run_algo_impl(eng, st, params, std::forward<Handler>(handler), has_void_result<AlgoParams>{});
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

// Propagate associated properties
namespace boost {
namespace asio {

template <template <class, class> class Associator, class AlgoParams, class Handler, class DefaultCandidate>
struct associator<Associator, mysql::detail::generic_algo_handler<AlgoParams, Handler>, DefaultCandidate>
    : Associator<Handler, DefaultCandidate>
{
    using mysql_handler = mysql::detail::generic_algo_handler<AlgoParams, Handler>;

    static typename Associator<Handler, DefaultCandidate>::type get(const mysql_handler& h) noexcept
    {
        return Associator<Handler, DefaultCandidate>::get(h.final_handler);
    }

    static auto get(const mysql_handler& h, const DefaultCandidate& c) noexcept
        -> decltype(Associator<Handler, DefaultCandidate>::get(h.final_handler, c))
    {
        return Associator<Handler, DefaultCandidate>::get(h.final_handler, c);
    }
};

}  // namespace asio
}  // namespace boost

#endif

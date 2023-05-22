//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_READ_SOME_ROWS_STATIC_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_READ_SOME_ROWS_STATIC_HPP

#include <boost/mysql/detail/config.hpp>

#ifdef BOOST_MYSQL_CXX14

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/static_execution_state.hpp>

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/network_algorithms/read_some_rows_impl.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream, class SpanRowType, class... RowType>
std::size_t read_some_rows_static(
    channel<Stream>& chan,
    static_execution_state<RowType...>& st,
    span<SpanRowType> output,
    error_code& err,
    diagnostics& diag
)
{
    constexpr std::size_t index = detail::get_type_index<SpanRowType, RowType...>();
    static_assert(
        index != detail::index_not_found,
        "SpanRowType must be one of the types returned by the query"
    );

    return read_some_rows_impl(
        chan,
        detail::impl_access::get_impl(st).get_interface(),
        detail::output_ref(output, index),
        err,
        diag
    );
}

template <
    class Stream,
    class SpanRowType,
    class... RowType,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, rows_view))
async_read_some_rows_static(
    channel<Stream>& chan,
    static_execution_state<RowType...>& st,
    span<SpanRowType> output,
    diagnostics& diag,
    CompletionToken&& token
)
{
    constexpr std::size_t index = detail::get_type_index<SpanRowType, RowType...>();
    static_assert(
        index != detail::index_not_found,
        "SpanRowType must be one of the types returned by the query"
    );

    return async_read_some_rows_impl(
        chan,
        detail::impl_access::get_impl(st).get_interface(),
        detail::output_ref(output, index),
        diag,
        std::forward<CompletionToken>(token)
    );
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif  // BOOST_MYSQL_CXX14

#endif

//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READ_SOME_ROWS_DYNAMIC_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READ_SOME_ROWS_DYNAMIC_HPP

#pragma once

#include <boost/mysql/detail/network_algorithms/read_some_rows_dynamic.hpp>
#include <boost/mysql/detail/network_algorithms/read_some_rows_impl.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline rows_view get_some_rows(const channel_base& ch, const execution_state_impl& st)
{
    return rows_view_access::construct(
        ch.shared_fields().data(),
        ch.shared_fields().size(),
        st.meta().size()
    );
}

template <class Stream>
struct read_some_rows_dynamic_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    diagnostics& diag_;
    execution_state_impl& st_;

    read_some_rows_dynamic_op(channel<Stream>& chan, diagnostics& diag, execution_state_impl& st) noexcept
        : chan_(chan), diag_(diag), st_(st)
    {
    }

    template <class Self>
    void operator()(Self& self, error_code err = {}, std::size_t = 0)
    {
        // Error checking
        if (err)
        {
            self.complete(err, rows_view());
            return;
        }

        // Normal path
        BOOST_ASIO_CORO_REENTER(*this)
        {
            chan_.shared_fields().clear();
            BOOST_ASIO_CORO_YIELD async_read_some_rows_impl(chan_, st_, output_ref(), diag_, std::move(self));
            self.complete(error_code(), get_some_rows(chan_, st_));
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream>
boost::mysql::rows_view boost::mysql::detail::read_some_rows_dynamic(
    channel<Stream>& channel,
    execution_state_impl& st,
    error_code& err,
    diagnostics& diag
)
{
    channel.shared_fields().clear();
    read_some_rows_impl(channel, st, output_ref(), err, diag);
    if (err)
        return rows_view();
    return get_some_rows(channel, st);
}

template <
    class Stream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
        CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code, boost::mysql::rows_view))
boost::mysql::detail::async_read_some_rows_dynamic(
    channel<Stream>& channel,
    execution_state_impl& st,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code, rows_view)>(
        read_some_rows_dynamic_op<Stream>(channel, diag, st),
        token,
        channel
    );
}

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_TEXT_ROW_IPP_ */

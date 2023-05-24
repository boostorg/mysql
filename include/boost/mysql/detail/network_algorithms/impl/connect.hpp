//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CONNECT_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CONNECT_HPP

#pragma once

#include <boost/mysql/detail/network_algorithms/connect.hpp>
#include <boost/mysql/detail/network_algorithms/handshake.hpp>

namespace boost {
namespace mysql {
namespace detail {

struct connect_op : boost::asio::coroutine
{
    channel& chan_;
    diagnostics& diag_;
    const void* ep_;
    handshake_params params_;

    connect_op(channel& chan, diagnostics& diag, const void* ep, const handshake_params& params)
        : chan_(chan), diag_(diag), ep_(ep), params_(params)
    {
    }

    template <class Self>
    void operator()(Self& self, error_code code = {})
    {
        error_code ignored;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            diag_.clear();

            // Physical connect
            BOOST_ASIO_CORO_YIELD chan_.stream().async_connect(ep_, std::move(self));
            if (code)
            {
                chan_.stream().close(ignored);
                self.complete(code);
                BOOST_ASIO_CORO_YIELD break;
            }

            // Handshake
            BOOST_ASIO_CORO_YIELD async_handshake_impl(chan_, params_, diag_, std::move(self));
            if (code)
            {
                chan_.stream().close(ignored);
            }
            self.complete(code);
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

void boost::mysql::detail::connect_impl(
    channel& chan,
    const void* endpoint,
    const handshake_params& params,
    error_code& err,
    diagnostics& diag
)
{
    error_code ignored;
    chan.stream().connect(endpoint, err);
    if (err)
    {
        chan.stream().close(ignored);
        return;
    }
    handshake_impl(chan, params, err, diag);
    if (err)
    {
        chan.stream().close(ignored);
    }
}

void boost::mysql::detail::async_connect_impl(
    channel& chan,
    const void* endpoint,
    const handshake_params& params,
    diagnostics& diag,
    asio::any_completion_handler<void(error_code)> handler
)
{
    return boost::asio::async_compose<asio::any_completion_handler<void(error_code)>, void(error_code)>(
        connect_op{chan, diag, endpoint, params},
        handler,
        chan
    );
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CONNECT_HPP_ */

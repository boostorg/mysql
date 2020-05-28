//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_CONNECTION_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_CONNECTION_HPP

#include "boost/mysql/detail/network_algorithms/quit_connection.hpp"

template <typename StreamType>
void boost::mysql::detail::close_connection(
    channel<StreamType>& chan,
    error_code& code,
    error_info& info
)
{
    // Close = quit + close stream. We close the stream regardless of the quit failing or not
    if (chan.next_layer().is_open())
    {
        quit_connection(chan, code, info);
        auto err = chan.close();
        if (!code)
        {
            code = err;
        }
    }
}

namespace boost {
namespace mysql {
namespace detail {

template<class StreamType>
struct close_connection_op : boost::asio::coroutine
{
    channel<StreamType>& chan_;
    error_info& output_info_;

    close_connection_op(channel<StreamType>& chan, error_info& output_info) :
        chan_(chan),
        output_info_(output_info)
    {
    }

    template<class Self>
    void operator()(
        Self& self,
        error_code err = {}
    )
    {
        error_code close_err;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            if (!chan_.next_layer().is_open())
            {
                BOOST_ASIO_CORO_YIELD boost::asio::post(std::move(self));
                self.complete(error_code());
                BOOST_ASIO_CORO_YIELD break;
            }

            // We call close regardless of the quit outcome
            // There are no async versions of shutdown or close
            BOOST_ASIO_CORO_YIELD async_quit_connection(
                chan_,
                std::move(self),
                output_info_
            );
            close_err = chan_.close();
            self.complete(err ? err : close_err);
        }
    }
};

}
}
}

template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::detail::async_close_connection(
    channel<StreamType>& chan,
    CompletionToken&& token,
    error_info& info
)
{
    return boost::asio::async_compose<
        CompletionToken,
        void(boost::mysql::error_code)
    >(close_connection_op<StreamType>{chan, info}, token, chan);
}


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_CONNECTION_HPP_ */

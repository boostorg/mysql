//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_CONNECTION_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_CONNECTION_HPP_

#include "boost/mysql/detail/network_algorithms/quit_connection.hpp"
#include <boost/asio/yield.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <typename Stream>
error_code close_channel(
    channel<Stream>& chan
)
{
    error_code err0, err1;
    chan.next_layer().shutdown(Stream::shutdown_both, err0);
    chan.next_layer().close(err1);
    return err0 ? err0 : err1;
}

} // detail
} // mysql
} // boost

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
        auto err = close_channel(chan);
        if (!code) code = err;
    }
}

template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
    CompletionToken,
    boost::mysql::detail::close_connection_signature
)
boost::mysql::detail::async_close_connection(
    channel<StreamType>& chan,
    CompletionToken&& token,
    error_info* info
)
{
    using HandlerSignature = close_connection_signature;
    using HandlerType = BOOST_ASIO_HANDLER_TYPE(CompletionToken, HandlerSignature);
    using BaseType = boost::beast::async_base<HandlerType, typename StreamType::executor_type>;

    boost::asio::async_completion<CompletionToken, HandlerSignature> initiator(token);

    struct Op: BaseType, boost::asio::coroutine
    {
        channel<StreamType>& channel_;
        error_info* output_info_;

        Op(
            HandlerType&& handler,
            channel<StreamType>& channel,
            error_info* output_info
        ):
            BaseType(std::move(handler), channel.next_layer().get_executor()),
            channel_(channel),
            output_info_(output_info)
        {
        }

        void operator()(
            error_code err,
            bool cont=true
        )
        {
            error_code close_err;
            reenter(*this)
            {
                if (!channel_.next_layer().is_open())
                {
                    this->complete(cont, error_code());
                    yield break;
                }

                // We call close regardless of the quit outcome
                // There are no async versions of shutdown or close
                yield async_quit_connection(channel_, std::move(*this), output_info_);
                close_err = close_channel(channel_);
                this->complete(cont, err ? err : close_err);
            }
        }
    };

    Op(
        std::move(initiator.completion_handler),
        chan,
        info
    )(error_code(), false);
    return initiator.result.get();
}

#include <boost/asio/unyield.hpp>


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_CONNECTION_HPP_ */

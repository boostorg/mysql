//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CONNECT_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CONNECT_HPP_

#include "boost/mysql/detail/network_algorithms/handshake.hpp"

template <typename StreamType, typename EndpointType>
void boost::mysql::detail::connect(
    channel<StreamType>& chan,
    const EndpointType& endpoint,
    const connection_params& params,
    error_code& err,
    error_info& info
)
{
    chan.next_layer().connect(endpoint, err);
    if (err)
    {
        chan.close();
        info.set_message("Physical connect failed");
        return;
    }
    handshake(chan, params, err, info);
    if (err)
    {
        chan.close();
    }
}

template <typename StreamType, typename EndpointType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
    CompletionToken,
    boost::mysql::detail::connect_signature
)
boost::mysql::detail::async_connect(
    channel<StreamType>& chan,
    const EndpointType& endpoint,
    const connection_params& params,
    CompletionToken&& token,
    error_info* info
)
{
    struct op : async_op<StreamType, CompletionToken, connect_signature, op>
    {
        const EndpointType& ep_;
        connection_params params_;

        op(
            boost::asio::async_completion<CompletionToken, connect_signature>& completion,
            channel<StreamType>& chan,
            error_info* output_info,
            const EndpointType& ep,
            const connection_params& params
        ) :
            async_op<StreamType, CompletionToken, connect_signature, op>(completion, chan, output_info),
            ep_(ep),
            params_(params)
        {
        }

        void operator()(
            error_code code,
            bool cont=true
        )
        {
            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Physical connect
                BOOST_ASIO_CORO_YIELD this->get_channel().next_layer().async_connect(
                    ep_,
                    std::move(*this)
                );
                if (code)
                {
                    this->get_channel().close();
                    if (this->get_output_info())
                    {
                        this->get_output_info()->set_message("Physical connect failed");
                    }
                    this->complete(cont, code);
                    BOOST_ASIO_CORO_YIELD break;
                }

                // Handshake
                BOOST_ASIO_CORO_YIELD async_handshake(
                    this->get_channel(),
                    params_,
                    std::move(*this),
                    this->get_output_info()
                );
                if (code)
                {
                    this->get_channel().close();
                }
                this->complete(cont, code);
            }
        }
    };

    return op::initiate(std::forward<CompletionToken>(token), chan, info, endpoint, params);
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CONNECT_HPP_ */

//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_ASYNC_OP_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_ASYNC_OP_HPP_

#include "boost/mysql/detail/protocol/channel.hpp"
#include "boost/mysql/error.hpp"
#include <boost/asio/coroutine.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/beast/core/async_base.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <
    typename Stream,
    typename CompletionToken,
    typename HandlerSignature
>
using async_op_base = boost::beast::async_base<
    BOOST_ASIO_HANDLER_TYPE(CompletionToken, HandlerSignature),
    typename Stream::executor_type
>;

template <
    typename Stream,
    typename CompletionToken,
    typename HandlerSignature,
    typename Derived // CRTP
>
class async_op :
    public boost::asio::coroutine,
    public async_op_base<Stream, CompletionToken, HandlerSignature>
{
    channel<Stream>& channel_;
    error_info* output_info_;

    using base_type = async_op_base<Stream, CompletionToken, HandlerSignature>;
public:
    using async_completion_type = boost::asio::async_completion<CompletionToken, HandlerSignature>;

    async_op(async_completion_type& initiator, channel<Stream>& chan, error_info* output_info) :
        base_type(std::move(initiator.completion_handler), chan.next_layer().get_executor()),
        channel_(chan),
        output_info_(output_info)
    {
    }

    channel<Stream>& get_channel() noexcept { return channel_; }
    error_info* get_output_info() noexcept { return output_info_; }

    template <typename... Args>
    static auto initiate(
        CompletionToken&& token,
        channel<Stream>& chan,
        error_info* output_info,
        Args&&... args
    )
    {
        async_completion_type completion (token);
        Derived op (completion, chan, output_info, std::forward<Args>(args)...);
        op(error_code());
        return completion.result.get();
    }

    // Reads from channel against the channel internal buffer, using itself as
    void async_read() { async_read(channel_.shared_buffer()); }
    void async_read(bytestring& buff)
    {
        channel_.async_read(
            buff,
            std::move(static_cast<Derived&>(*this))
        );
    }

    void async_write(const bytestring& buff)
    {
        channel_.async_write(
            boost::asio::buffer(buff),
            std::move(static_cast<Derived&>(*this))
        );
    }

    void async_write() { async_write(channel_.shared_buffer()); }
};

} // detail
} // mysql
} // boost



#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_ASYNC_OP_HPP_ */

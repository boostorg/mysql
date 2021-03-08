//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_SOCKET_CONNECTION_HPP
#define BOOST_MYSQL_IMPL_SOCKET_CONNECTION_HPP

#include <boost/mysql/detail/network_algorithms/close_connection.hpp>
#include <boost/mysql/detail/network_algorithms/connect.hpp>

template <class SocketStream>
void boost::mysql::socket_connection<SocketStream>::connect(
    const endpoint_type& endpoint,
    const connection_params& params,
    error_code& ec,
    error_info& info
)
{
    detail::clear_errors(ec, info);
    detail::connect(this->get_channel(), endpoint, params, ec, info);
}

template <class SocketStream>
void boost::mysql::socket_connection<SocketStream>::connect(
    const endpoint_type& endpoint,
    const connection_params& params
)
{
    detail::error_block blk;
    detail::connect(this->get_channel(), endpoint, params, blk.err, blk.info);
    blk.check();
}

template <class SocketStream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::socket_connection<SocketStream>::async_connect(
    const endpoint_type& endpoint,
    const connection_params& params,
    error_info& output_info,
    CompletionToken&& token
)
{
    output_info.clear();
    return detail::async_connect(
        this->get_channel(),
        endpoint,
        params,
        std::forward<CompletionToken>(token),
        output_info
    );
}

template <class SocketStream>
void boost::mysql::socket_connection<SocketStream>::close(
    error_code& err,
    error_info& info
)
{
    detail::clear_errors(err, info);
    detail::close_connection(this->get_channel(), err, info);
}

template <class SocketStream>
void boost::mysql::socket_connection<SocketStream>::close()
{
    detail::error_block blk;
    detail::close_connection(this->get_channel(), blk.err, blk.info);
    blk.check();
}

template <class SocketStream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::socket_connection<SocketStream>::async_close(
    error_info& output_info,
    CompletionToken&& token
)
{
    output_info.clear();
    return detail::async_close_connection(
        this->get_channel(),
        std::forward<CompletionToken>(token),
        output_info
    );
}

#endif
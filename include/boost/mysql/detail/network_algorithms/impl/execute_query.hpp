//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_QUERY_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_QUERY_HPP

#pragma once

#include <boost/mysql/detail/network_algorithms/execute_query.hpp>
#include <boost/mysql/detail/protocol/text_deserialization.hpp>
#include <boost/mysql/detail/protocol/query_messages.hpp>
#include <boost/mysql/detail/network_algorithms/execute_generic.hpp>

template <class Stream>
void boost::mysql::detail::execute_query(
    channel<Stream>& channel,
    boost::string_view query,
    resultset<Stream>& output,
    error_code& err,
    error_info& info
)
{
    com_query_packet request { string_eof(query) };
    execute_generic(
        &deserialize_text_row,
        channel,
        request,
        output,
        err,
        info
    );
}


template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code, boost::mysql::resultset<Stream>)
)
boost::mysql::detail::async_execute_query(
    channel<Stream>& chan,
    boost::string_view query,
    CompletionToken&& token,
    error_info& info
)
{
    com_query_packet request { string_eof(query) };
    return async_execute_generic(
        &deserialize_text_row,
        chan,
        request,
        std::forward<CompletionToken>(token),
        info
    );
}


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_QUERY_HPP_ */

//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CHANNEL_DISABLEABLE_SSL_STREAM_HPP
#define BOOST_MYSQL_DETAIL_CHANNEL_DISABLEABLE_SSL_STREAM_HPP

#include "boost/mysql/error.hpp"
#include <boost/asio/async_result.hpp>
#include <cstddef>
#pragma once

#include <boost/mysql/detail/channel/disableable_ssl_stream.hpp>

namespace boost {
namespace mysql {
namespace detail {


} // detail
} // mysql
} // boost


template <class Stream>
template <class MutableBufferSequence, class CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code, std::size_t))
boost::mysql::detail::disableable_ssl_stream<Stream>::async_read_some(
    const MutableBufferSequence& buff,
    CompletionToken&& token
)
{
    if (ssl_active_)
    {
        return boost::asio::async_read(
            stream_,
            std::forward<BufferSeq>(buff),
            boost::asio::transfer_all(),
            std::forward<CompletionToken>(token)
        );
    }
    else
    {
        return boost::asio::async_read(
            get_non_ssl_stream(stream_),
            std::forward<BufferSeq>(buff),
            boost::asio::transfer_all(),
            std::forward<CompletionToken>(token)
        );
    }
}


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_STATIC_STRING_HPP_ */





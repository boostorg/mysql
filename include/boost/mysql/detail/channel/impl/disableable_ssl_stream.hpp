//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CHANNEL_IMPL_DISABLEABLE_SSL_STREAM_HPP
#define BOOST_MYSQL_DETAIL_CHANNEL_IMPL_DISABLEABLE_SSL_STREAM_HPP

#pragma once

#include <boost/asio/ssl/stream_base.hpp>
#include <boost/mysql/detail/channel/disableable_ssl_stream.hpp>
#include <boost/mysql/error.hpp>
#include <boost/asio/async_result.hpp>
#include <cstddef>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
using is_ssl_stream = std::is_base_of<boost::asio::ssl::stream_base, Stream>;


// Helpers to get the first non-SSL stream. We can't call just next_layer()
// because raw TCP sockets don't support this function. For non-SSL connections,
// we just return the stream itself
template <bool is_ssl_stream>
struct get_non_ssl_stream_t;

template <>
struct get_non_ssl_stream_t<true>
{
    template <typename Stream>
    static auto call(Stream& s) -> decltype(s.next_layer())
    {
        return s.next_layer();
    }
};

template <>
struct get_non_ssl_stream_t<false>
{
    template <typename Stream>
    static Stream& call(Stream& s) noexcept
    {
        return s;
    }
};

template <typename Stream>
auto get_non_ssl_stream(Stream& s) -> decltype(get_non_ssl_stream_t<is_ssl_stream<Stream>::value>::call(s))
{
    return get_non_ssl_stream_t<is_ssl_stream<Stream>::value>::call(s);
}

} // detail
} // mysql
} // boost

template <class Stream>
template <class MutableBufferSequence>
std::size_t boost::mysql::detail::disableable_ssl_stream<Stream>::read_some(
    const MutableBufferSequence& buff,
    error_code& ec
)
{
    if (ssl_active_)
    {
        return inner_stream_.read_some(buff, ec);
    }
    else
    {
        return get_non_ssl_stream(inner_stream_).read_some(buff, ec);
    }
}


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
        return inner_stream_.async_read_some(buff, std::forward<CompletionToken>(token));
    }
    else
    {
        return get_non_ssl_stream(inner_stream_).async_read_some(buff, std::forward<CompletionToken>(token));
    }
}


template <class Stream>
template <class ConstBufferSequence>
std::size_t boost::mysql::detail::disableable_ssl_stream<Stream>::write_some(
    const ConstBufferSequence& buff,
    error_code& ec
)
{
    if (ssl_active_)
    {
        return inner_stream_.write_some(buff, ec);
    }
    else
    {
        return get_non_ssl_stream(inner_stream_).write_some(buff, ec);
    }
}


template <class Stream>
template <class ConstBufferSequence, class CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code, std::size_t))
boost::mysql::detail::disableable_ssl_stream<Stream>::async_write_some(
    const ConstBufferSequence& buff,
    CompletionToken&& token
)
{
    if (ssl_active_)
    {
        return inner_stream_.async_write_some(buff, std::forward<CompletionToken>(token));
    }
    else
    {
        return get_non_ssl_stream(inner_stream_).async_write_some(buff, std::forward<CompletionToken>(token));
    }
}


#endif

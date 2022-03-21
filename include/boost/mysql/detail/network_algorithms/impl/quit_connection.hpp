//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_QUIT_CONNECTION_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_QUIT_CONNECTION_HPP

#pragma once

#include <boost/mysql/detail/network_algorithms/quit_connection.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/mysql/detail/protocol/channel.hpp>
#include <boost/mysql/error.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
void compose_quit(
    channel<Stream>& chan
)
{
    serialize_message(
        quit_packet(),
        chan.current_capabilities(),
        chan.shared_buffer()
    );
    chan.reset_sequence_number();
}

template <typename Stream>
void shutdown_ssl_impl(
    Stream& s,
    std::true_type // supports ssl
)
{
    error_code ignored;
    s.shutdown(ignored);
}

template <typename Stream>
void shutdown_ssl_impl(
    Stream&,
    std::false_type // supports ssl
)
{
}

template <typename Stream>
void shutdown_ssl(Stream& s) { shutdown_ssl_impl(s, is_ssl_stream<Stream>()); }

template<class Stream, bool is_ssl_stream>
struct quit_connection_op;

// For SSL connections
template<class Stream>
struct quit_connection_op<Stream, true> : boost::asio::coroutine
{
    channel<Stream>& chan_;

    quit_connection_op(channel<Stream>& chan) noexcept :
        chan_(chan) {}

    template<class Self>
    void operator()(
        Self& self,
        error_code err = {}
    )
    {
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // Quit message
            BOOST_ASIO_CORO_YIELD chan_.async_write(chan_.shared_buffer(), std::move(self));
            if (err)
            {
                self.complete(err);
            }

            // SSL shutdown error ignored, as MySQL doesn't always gracefully
            // close SSL connections
            BOOST_ASIO_CORO_YIELD chan_.next_layer().async_shutdown(std::move(self));
            self.complete(error_code());
        }
    }
};

// For non-SSL connections
template<class Stream>
struct quit_connection_op<Stream, false> : boost::asio::coroutine
{
    channel<Stream>& chan_;

    quit_connection_op(channel<Stream>& chan) noexcept :
        chan_(chan) {}

    template<class Self>
    void operator()(
        Self& self,
        error_code err = {}
    )
    {
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // Quit message
            BOOST_ASIO_CORO_YIELD chan_.async_write(chan_.shared_buffer(), std::move(self));
            self.complete(err);
        }
    }
};

} // detail
} // mysql
} // boost

template <class Stream>
void boost::mysql::detail::quit_connection(
    channel<Stream>& chan,
    error_code& code,
    error_info&
)
{
    compose_quit(chan);
    chan.write(chan.shared_buffer(), code);
    if (!code)
    {
        // SSL shutdown. Result ignored as MySQL does not always perform
        // graceful SSL shutdowns
        shutdown_ssl(chan.next_layer());
    }
}

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::detail::async_quit_connection(
    channel<Stream>& chan,
    CompletionToken&& token,
    error_info&
)
{
    compose_quit(chan);
    return boost::asio::async_compose<
        CompletionToken,
        void(boost::mysql::error_code)
    >(quit_connection_op<Stream, is_ssl_stream<Stream>::value>{chan}, token, chan);
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_QUIT_CONNECTION_HPP_ */

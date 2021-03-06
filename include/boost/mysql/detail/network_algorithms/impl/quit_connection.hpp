//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_QUIT_CONNECTION_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_QUIT_CONNECTION_HPP

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
    return chan.async_write(
        chan.shared_buffer(),
        std::forward<CompletionToken>(token)
    );
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_QUIT_CONNECTION_HPP_ */

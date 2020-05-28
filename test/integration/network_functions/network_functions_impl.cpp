//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "network_functions_impl.hpp"
#include <boost/asio/use_awaitable.hpp> // for BOOST_ASIO_HAS_CO_AWAIT

using namespace boost::mysql::test;

template <typename Stream>
std::vector<network_functions<Stream>*>
boost::mysql::test::make_all_network_functions()
{
    return {
        sync_errc_functions<Stream>(),
        sync_exc_functions<Stream>(),
        async_callback_errinfo_functions<Stream>(),
        async_callback_noerrinfo_functions<Stream>(),
        async_coroutine_errinfo_functions<Stream>(),
        async_coroutine_noerrinfo_functions<Stream>(),
        async_future_errinfo_functions<Stream>(),
        async_future_noerrinfo_functions<Stream>(),
#ifdef BOOST_ASIO_HAS_CO_AWAIT
        async_coroutinecpp20_errinfo_functions<Stream>(),
        async_coroutinecpp20_noerrinfo_functions<Stream>()
#endif
    };
}

// We implement a single variant for default completion tokens
// Others do not add any value
template <>
std::vector<network_functions<tcp_future_socket>*>
boost::mysql::test::make_all_network_functions<tcp_future_socket>()
{
    return {
        async_future_errinfo_functions<tcp_future_socket>(),
        async_future_noerrinfo_functions<tcp_future_socket>(),
    };
}

template std::vector<boost::mysql::test::network_functions<boost::asio::ip::tcp::socket>*>
boost::mysql::test::make_all_network_functions();

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
template std::vector<boost::mysql::test::network_functions<boost::asio::local::stream_protocol::socket>*>
boost::mysql::test::make_all_network_functions();
#endif

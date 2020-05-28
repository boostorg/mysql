//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef TEST_INTEGRATION_NETWORK_FUNCTIONS_NETWORK_FUNCTIONS_IMPL_HPP_
#define TEST_INTEGRATION_NETWORK_FUNCTIONS_NETWORK_FUNCTIONS_IMPL_HPP_

#include "../network_functions.hpp"
#include <boost/asio/local/stream_protocol.hpp>

namespace boost {
namespace mysql {
namespace test {

template <typename Stream> network_functions<Stream>* sync_errc_functions();
template <typename Stream> network_functions<Stream>* sync_exc_functions();
template <typename Stream> network_functions<Stream>* async_callback_errinfo_functions();
template <typename Stream> network_functions<Stream>* async_callback_noerrinfo_functions();
template <typename Stream> network_functions<Stream>* async_coroutine_errinfo_functions();
template <typename Stream> network_functions<Stream>* async_coroutine_noerrinfo_functions();
template <typename Stream> network_functions<Stream>* async_coroutinecpp20_errinfo_functions();
template <typename Stream> network_functions<Stream>* async_coroutinecpp20_noerrinfo_functions();
template <typename Stream> network_functions<Stream>* async_future_noerrinfo_functions();
template <typename Stream> network_functions<Stream>* async_future_errinfo_functions();

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
#define BOOST_MYSQL_INSTANTIATE_NETWORK_FUNCTIONS_UNIX(fun) \
    template boost::mysql::test::network_functions<boost::asio::local::stream_protocol::socket>* \
    boost::mysql::test::fun<boost::asio::local::stream_protocol::socket>();
#else
#define BOOST_MYSQL_INSTANTIATE_NETWORK_FUNCTIONS_UNIX(fun)
#endif

#define BOOST_MYSQL_INSTANTIATE_NETWORK_FUNCTIONS(fun) \
    template boost::mysql::test::network_functions<boost::asio::ip::tcp::socket>* \
    boost::mysql::test::fun<boost::asio::ip::tcp::socket>(); \
    BOOST_MYSQL_INSTANTIATE_NETWORK_FUNCTIONS_UNIX(fun)


}
}
}



#endif

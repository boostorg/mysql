//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_AUXILIAR_ASYNC_RESULT_MACRO_HPP
#define BOOST_MYSQL_DETAIL_AUXILIAR_ASYNC_RESULT_MACRO_HPP

#include <boost/asio/async_result.hpp>

#ifdef BOOST_MYSQL_DOXYGEN
#define BOOST_MYSQL_INITFN_RESULT_TYPE(ct, sig) DEDUCED
#else
#define BOOST_MYSQL_INITFN_RESULT_TYPE(ct, sig) \
	BOOST_ASIO_INITFN_RESULT_TYPE(ct, sig)
#endif

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_ASYNC_RESULT_MACRO_HPP_ */

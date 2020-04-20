//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_AUXILIAR_VALGRIND_HPP
#define BOOST_MYSQL_DETAIL_AUXILIAR_VALGRIND_HPP

#ifdef BOOST_MYSQL_VALGRIND_TESTS
#include <valgrind/memcheck.h>
#endif

namespace boost {
namespace mysql {
namespace detail {

inline void valgrind_make_mem_defined(
    [[maybe_unused]] boost::asio::const_buffer buff
)
{
#ifdef BOOST_MYSQL_VALGRIND_TESTS
    VALGRIND_MAKE_MEM_DEFINED(buff.data(), buff.size());
#endif
}

} // detail
} // mysql
} // boost



#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_VALGRIND_HPP_ */

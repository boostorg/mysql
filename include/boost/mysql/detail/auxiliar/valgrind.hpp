#ifndef INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_VALGRIND_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_VALGRIND_HPP_

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

#ifndef INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_STRINGIZE_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_STRINGIZE_HPP_

#include <string>
#include <sstream>

namespace boost {
namespace mysql {
namespace detail {

template <typename... Types>
std::string stringize(const Types&... inputs)
{
	std::ostringstream ss;
	(ss << ... << inputs);
	return ss.str();
}

} // detail
} // mysql
} // boost



#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_STRINGIZE_HPP_ */

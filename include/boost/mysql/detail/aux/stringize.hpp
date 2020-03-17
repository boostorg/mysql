#ifndef INCLUDE_BOOST_MYSQL_DETAIL_AUX_STRINGIZE_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_AUX_STRINGIZE_HPP_

#include <string>
#include <sstream>

namespace mysql
{
namespace detail
{

template <typename... Types>
std::string stringize(const Types&... inputs)
{
	std::ostringstream ss;
	(ss << ... << inputs);
	return ss.str();
}

}
}



#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUX_STRINGIZE_HPP_ */

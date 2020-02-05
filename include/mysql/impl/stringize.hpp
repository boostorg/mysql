#ifndef INCLUDE_MYSQL_IMPL_STRINGIZE_HPP_
#define INCLUDE_MYSQL_IMPL_STRINGIZE_HPP_

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



#endif /* INCLUDE_MYSQL_IMPL_STRINGIZE_HPP_ */

#ifndef TEST_TEST_COMMON_HPP_
#define TEST_TEST_COMMON_HPP_

#include "mysql/value.hpp"
#include <vector>

namespace mysql
{
namespace test
{

template <typename... Types>
std::vector<value> makevalues(Types&&... args)
{
	return std::vector<value>{mysql::value(std::forward<Types>(args))...};
}

}
}



#endif /* TEST_TEST_COMMON_HPP_ */

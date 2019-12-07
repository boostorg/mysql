#ifndef INCLUDE_MYSQL_IMPL_CONTAINER_EQUALS_HPP_
#define INCLUDE_MYSQL_IMPL_CONTAINER_EQUALS_HPP_

#include <algorithm>
#include <vector>

namespace mysql
{
namespace detail
{

template <typename TLeft, typename TRight>
inline bool container_equals(
	const std::vector<TLeft>& lhs,
	const std::vector<TRight>& rhs
)
{
	if (lhs.size() != rhs.size()) return false;
	return std::equal(
		lhs.begin(),
		lhs.end(),
		rhs.begin()
	);
}

}
}



#endif /* INCLUDE_MYSQL_IMPL_CONTAINER_EQUALS_HPP_ */

#ifndef MYSQL_ASIO_IMPL_CONTAINER_EQUALS_HPP
#define MYSQL_ASIO_IMPL_CONTAINER_EQUALS_HPP

#include <algorithm>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

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

} // detail
} // mysql
} // boost



#endif

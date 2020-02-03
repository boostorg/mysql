#ifndef INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_COMMON_HPP_
#define INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_COMMON_HPP_

#include <boost/asio/coroutine.hpp>
#include <boost/beast/core/async_base.hpp>
#include "mysql/impl/error.hpp"
#include "mysql/impl/channel.hpp"

namespace mysql
{
namespace detail
{

enum class fetch_result
{
	error,
	row,
	eof
};

}
}



#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_COMMON_HPP_ */

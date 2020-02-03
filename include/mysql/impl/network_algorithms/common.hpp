#ifndef INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_COMMON_HPP_
#define INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_COMMON_HPP_

#include <boost/asio/coroutine.hpp>
#include <boost/beast/core/async_base.hpp>
#include "mysql/error.hpp"
#include "mysql/impl/channel.hpp"
#include "mysql/impl/messages.hpp"

namespace mysql
{
namespace detail
{

enum class read_row_result
{
	error,
	row,
	eof
};

}
}



#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_COMMON_HPP_ */
